#include "platform/platform.h"

#include <QGuiApplication>
#include <QImage>
#include <QObject>
#include <QScreen>
#include <QString>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace sticky::platform {
namespace {

struct X11Guard {
  Display* display = nullptr;
  ~X11Guard() {
    if (display) XCloseDisplay(display);
  }
};

Display* displayFor() {
  static X11Guard guard;
  if (!guard.display) guard.display = XOpenDisplay(nullptr);
  return guard.display;
}

bool windowPropertyCardinal(Display* display, Window window, Atom property,
                            long* out) {
  Atom actualType = None;
  int actualFormat = 0;
  unsigned long count = 0;
  unsigned long bytesAfter = 0;
  unsigned char* data = nullptr;
  const bool ok =
      XGetWindowProperty(display, window, property, 0, 32, False, XA_CARDINAL,
                         &actualType, &actualFormat, &count, &bytesAfter,
                         &data) == Success &&
      data && count >= 1;
  if (ok) *out = reinterpret_cast<long*>(data)[0];
  if (data) XFree(data);
  return ok;
}

bool windowPropertyAtom(Display* display, Window window, Atom property,
                        Atom* out) {
  Atom actualType = None;
  int actualFormat = 0;
  unsigned long count = 0;
  unsigned long bytesAfter = 0;
  unsigned char* data = nullptr;
  const bool ok =
      XGetWindowProperty(display, window, property, 0, 64, False, XA_ATOM,
                         &actualType, &actualFormat, &count, &bytesAfter,
                         &data) == Success &&
      data && count >= 1;
  if (ok) *out = reinterpret_cast<Atom*>(data)[0];
  if (data) XFree(data);
  return ok;
}

bool isOwnWindow(Display* display, Window window) {
  const Atom netWmPid = XInternAtom(display, "_NET_WM_PID", True);
  long pid = 0;
  if (netWmPid != None &&
      windowPropertyCardinal(display, window, netWmPid, &pid)) {
    return pid == static_cast<long>(getpid());
  }
  return false;
}

bool isDesktopOrDock(Display* display, Window window) {
  const Atom netWmType = XInternAtom(display, "_NET_WM_WINDOW_TYPE", True);
  if (netWmType == None) return false;
  const Atom dock = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DOCK", True);
  const Atom desktop = XInternAtom(display, "_NET_WM_WINDOW_TYPE_DESKTOP", True);
  Atom type = None;
  if (!windowPropertyAtom(display, window, netWmType, &type)) return false;
  return (dock != None && type == dock) || (desktop != None && type == desktop);
}

// X11 上 Qt 逻辑坐标 ≈ 设备像素，仅主屏存在高倍缩放时需换算。
qreal globalScale() {
  QScreen* primary = QGuiApplication::primaryScreen();
  const qreal factor = primary ? primary->devicePixelRatio() : 1.0;
  return factor > 0.0 ? factor : 1.0;
}

QPoint toDevicePoint(const QPoint& globalPoint) {
  const qreal factor = globalScale();
  return QPoint(qRound(globalPoint.x() * factor),
                qRound(globalPoint.y() * factor));
}

}  // namespace

bool ensureScreenCapturePermission() {
  // X11 抓屏无需系统授权（Wayland 另行处理，见 README）。
  return true;
}

QVector<DisplayCapture> captureDisplays(QString* error) {
  QVector<DisplayCapture> captures;
  Display* display = displayFor();
  if (!display) {
    if (error) *error = QObject::tr("无法连接 X 显示服务器。");
    return captures;
  }

  const Window root = DefaultRootWindow(display);
  XWindowAttributes rootAttrs{};
  if (!XGetWindowAttributes(display, root, &rootAttrs) ||
      rootAttrs.width <= 0 || rootAttrs.height <= 0) {
    if (error) *error = QObject::tr("没有截取到任何显示器。");
    return captures;
  }

  // X11 没有 per-window 抓屏排除：抓图前临时 unmap 自己的顶层窗口，
  // 抓完立即恢复（会有一次轻微闪烁，X11 工具的通行做法）。
  Window rootReturn = None;
  Window parentReturn = None;
  Window* children = nullptr;
  unsigned int childCount = 0;
  std::vector<Window> unmapped;
  if (XQueryTree(display, root, &rootReturn, &parentReturn, &children,
                 &childCount) == Success && children) {
    for (unsigned int i = 0; i < childCount; ++i) {
      XWindowAttributes attrs{};
      if (!XGetWindowAttributes(display, children[i], &attrs)) continue;
      if (attrs.map_state != IsViewable) continue;
      if (isOwnWindow(display, children[i])) {
        XUnmapWindow(display, children[i]);
        unmapped.push_back(children[i]);
      }
    }
  }
  if (children) XFree(children);
  XSync(display, False);

  QImage full;
  if (XImage* image = XGetImage(display, root, 0, 0, rootAttrs.width,
                                rootAttrs.height, AllPlanes, ZPixmap)) {
    if (image->bits_per_pixel == 32) {
      // ZPixmap + LSBFirst 的 32bpp 行序 (B,G,R,X) 与 Format_RGB32 一致。
      full = QImage(reinterpret_cast<const uchar*>(image->data), image->width,
                    image->height,
                    static_cast<qsizetype>(image->bytes_per_line),
                    QImage::Format_RGB32)
                 .copy();
    }
    XDestroyImage(image);
  }

  for (Window window : unmapped) XMapWindow(display, window);
  XFlush(display);

  if (full.isNull()) {
    if (error) *error = QObject::tr("屏幕截取失败。");
    return captures;
  }

  // 合并模式下一个 X Screen 覆盖整个虚拟桌面，按 QScreen 逻辑几何切片。
  for (QScreen* screen : QGuiApplication::screens()) {
    const QRect logical = screen->geometry();
    if (logical.isEmpty()) continue;
    const qreal dpr = screen->devicePixelRatio();
    if (dpr <= 0.0) continue;

    const QRect device(qRound(logical.x() * dpr), qRound(logical.y() * dpr),
                       qRound(logical.width() * dpr),
                       qRound(logical.height() * dpr));
    const QImage slice = full.copy(device);
    if (slice.isNull() || slice.width() <= 0) continue;

    DisplayCapture capture;
    capture.screen = screen;
    capture.geometry = logical;
    capture.image = slice;
    capture.dpr = static_cast<qreal>(slice.width()) / logical.width();
    captures.push_back(std::move(capture));
  }

  if (captures.isEmpty() && error && error->isEmpty()) {
    *error = QObject::tr("没有截取到任何显示器。");
  }
  return captures;
}

QRect windowAt(const QPoint& globalPoint) {
  Display* display = displayFor();
  if (!display) return {};

  const QPoint devicePoint = toDevicePoint(globalPoint);
  const Window root = DefaultRootWindow(display);
  Window rootReturn = None;
  Window parentReturn = None;
  Window* children = nullptr;
  unsigned int childCount = 0;
  if (XQueryTree(display, root, &rootReturn, &parentReturn, &children,
                 &childCount) != Success || !children) {
    return {};
  }

  // QueryTree 自底向上排列；倒序找第一个命中的即最顶层。
  QRect found;
  for (int i = static_cast<int>(childCount) - 1; i >= 0; --i) {
    const Window window = children[i];
    XWindowAttributes attrs{};
    if (!XGetWindowAttributes(display, window, &attrs)) continue;
    if (attrs.map_state != IsViewable || attrs.c_class == InputOnly) continue;
    if (isDesktopOrDock(display, window)) continue;
    if (isOwnWindow(display, window)) continue;  // 契约：自己的窗口返回空

    const QRect rect(attrs.x, attrs.y, attrs.width, attrs.height);
    if (rect.contains(devicePoint)) {
      const qreal factor = globalScale();
      found = QRect(qRound(rect.x() / factor), qRound(rect.y() / factor),
                    qRound(rect.width() / factor),
                    qRound(rect.height() / factor));
      break;
    }
  }
  XFree(children);
  return found;
}

bool ensureUiElementPermission() {
  // Linux 没有开箱即用的跨应用元素级 API（at-spi 需要 DBus 会话集成）。
  return true;
}

QRect uiElementAt(const QPoint& globalPoint) {
  Q_UNUSED(globalPoint);
  return {};  // Linux 暂无元素级吸附
}

void ocrText(const QImage& image, OcrCallback callback) {
  Q_UNUSED(image);
  callback({}, QObject::tr("Linux 暂未内置 OCR 引擎（tesseract 集成待后续版本）。"));
}

}  // namespace sticky::platform
