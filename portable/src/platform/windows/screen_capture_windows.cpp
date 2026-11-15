#include "platform/platform.h"

#include <QGuiApplication>
#include <QObject>
#include <QScreen>
#include <QString>

#include <windows.h>
#include <dwmapi.h>
#include <objbase.h>
#include <uiautomationcore.h>
#include <uiautomationclient.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace sticky::platform {
namespace {

struct VirtualScreen {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

VirtualScreen virtualScreen() {
  return {GetSystemMetrics(SM_XVIRTUALSCREEN),
          GetSystemMetrics(SM_YVIRTUALSCREEN),
          GetSystemMetrics(SM_CXVIRTUALSCREEN),
          GetSystemMetrics(SM_CYVIRTUALSCREEN)};
}

// Qt positions every screen in one logical space anchored at the primary
// screen's top-left, which is also the Windows screen-coordinate origin, so a
// logical point converts to device pixels by scaling with the primary factor.
// On mixed-DPI multi-monitor setups secondary screens may drift slightly.
qreal globalScale() {
  QScreen* primary = QGuiApplication::primaryScreen();
  const qreal factor = primary ? primary->devicePixelRatio() : 1.0;
  return factor > 0.0 ? factor : 1.0;
}

POINT toDevicePoint(const QPoint& globalPoint) {
  const qreal factor = globalScale();
  return POINT{qRound(globalPoint.x() * factor),
               qRound(globalPoint.y() * factor)};
}

QRect toLogicalRect(const RECT& bounds) {
  const qreal factor = globalScale();
  return QRect(QPoint(qRound(bounds.left / factor), qRound(bounds.top / factor)),
               QPoint(qRound(bounds.right / factor),
                      qRound(bounds.bottom / factor)));
}

bool isOwnWindow(HWND hwnd) {
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  return pid == GetCurrentProcessId();
}

bool isCloaked(HWND hwnd) {
  BOOL cloaked = FALSE;
  if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked,
                                      sizeof(cloaked)))) {
    return cloaked != FALSE;
  }
  return false;
}

// UIA 会话在首次悬停查询时惰性建立（GUI 线程 STA）。
IUIAutomation* uiAutomation() {
  static IUIAutomation* automation = [] {
    IUIAutomation* result = nullptr;
    // RPC_E_CHANGED_MODE 时忽略返回值：UIA 在 MTA 下同样可用。
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&result));
    return result;
  }();
  return automation;
}

}  // namespace

bool ensureScreenCapturePermission() {
  // Windows has no screen-capture permission gate; BitBlt of the desktop
  // always works for the interactive user.
  return true;
}

QVector<DisplayCapture> captureDisplays(QString* error) {
  QVector<DisplayCapture> captures;
  const VirtualScreen vs = virtualScreen();
  if (vs.width <= 0 || vs.height <= 0) {
    if (error) *error = QObject::tr("屏幕截取不可用。");
    return captures;
  }

  HDC screenDc = GetDC(nullptr);
  if (!screenDc) {
    if (error) *error = QObject::tr("屏幕截取失败。");
    return captures;
  }

  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = vs.width;
  info.bmiHeader.biHeight = -vs.height;  // negative: top-down rows
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP bitmap =
      CreateDIBSection(screenDc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
  HDC memoryDc = bitmap ? CreateCompatibleDC(screenDc) : nullptr;
  HGDIOBJ oldBitmap = memoryDc ? SelectObject(memoryDc, bitmap) : nullptr;
  const bool blitted =
      memoryDc && BitBlt(memoryDc, 0, 0, vs.width, vs.height, screenDc, vs.x,
                         vs.y, SRCCOPY | CAPTUREBLT);

  QImage full;
  if (blitted && bits) {
    // GDI writes BGRA rows, which matches Format_RGB32 storage on
    // little-endian; RGB32 ignores the alpha byte BitBlt leaves untouched.
    full = QImage(static_cast<const uchar*>(bits), vs.width, vs.height,
                  static_cast<qsizetype>(vs.width) * 4, QImage::Format_RGB32)
               .copy();
  }

  if (oldBitmap) SelectObject(memoryDc, oldBitmap);
  if (memoryDc) DeleteDC(memoryDc);
  if (bitmap) DeleteObject(bitmap);
  ReleaseDC(nullptr, screenDc);

  if (full.isNull()) {
    if (error) *error = QObject::tr("屏幕截取失败。");
    return captures;
  }

  for (QScreen* screen : QGuiApplication::screens()) {
    const QRect logical = screen->geometry();
    if (logical.isEmpty()) continue;
    const qreal dpr = screen->devicePixelRatio();
    if (dpr <= 0.0) continue;

    const QRect device(
        qRound(logical.x() * dpr) - vs.x, qRound(logical.y() * dpr) - vs.y,
        qRound(logical.width() * dpr), qRound(logical.height() * dpr));
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
  const POINT point = toDevicePoint(globalPoint);
  HWND hwnd = WindowFromPoint(point);
  if (!hwnd) return {};
  hwnd = GetAncestor(hwnd, GA_ROOT);
  if (!hwnd) return {};

  // Contract: a null rect when the point is over one of our own windows.
  if (isOwnWindow(hwnd)) return {};
  if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return {};

  const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  if (exStyle & WS_EX_TRANSPARENT) return {};
  if (isCloaked(hwnd)) return {};

  wchar_t className[64] = {};
  if (GetClassNameW(hwnd, className, 64) > 0 &&
      (wcscmp(className, L"Progman") == 0 || wcscmp(className, L"WorkerW") == 0)) {
    return {};  // the desktop itself
  }

  RECT bounds{};
  if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds,
                                   sizeof(bounds)))) {
    if (!GetWindowRect(hwnd, &bounds)) return {};
  }

  return toLogicalRect(bounds);
}

bool ensureUiElementPermission() {
  // Windows 的 UI Automation 无需额外授权。
  return true;
}

QRect uiElementAt(const QPoint& globalPoint) {
  IUIAutomation* automation = uiAutomation();
  if (!automation) return {};

  const POINT point = toDevicePoint(globalPoint);
  IUIAutomationElement* element = nullptr;
  if (FAILED(automation->ElementFromPoint(point, &element)) || !element) {
    return {};
  }

  int pid = 0;
  element->get_CurrentProcessId(&pid);
  if (pid == GetCurrentProcessId()) {
    element->Release();
    return {};  // 自己的贴图 / 遮罩
  }

  RECT bounds{};
  element->get_CurrentBoundingRectangle(&bounds);
  element->Release();

  if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
    return {};
  }
  return toLogicalRect(bounds);
}

}  // namespace sticky::platform
