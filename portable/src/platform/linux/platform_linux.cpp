#include "platform/platform.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QUrl>
#include <QWidget>
#include <QWindow>

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

namespace sticky::platform {
namespace {

QString ensureDirectory(const QString& path) {
  QDir().mkpath(path);
  return path;
}

// 独立的 X 连接（抓屏/热键/穿透各自使用，全部惰性建立）。
Display* x11Display() {
  static Display* display = XOpenDisplay(nullptr);
  return display;
}

QString autostartDesktopPath() {
  // XDG 自启目录：~/.config/autostart/StickySnap.desktop
  QString base = QStandardPaths::writableLocation(
      QStandardPaths::GenericConfigLocation);
  if (base.isEmpty()) base = QDir::homePath() + QStringLiteral("/.config");
  return ensureDirectory(base + QStringLiteral("/autostart")) +
         QStringLiteral("/StickySnap.desktop");
}

QString autostartDesktopContent() {
  const QString command =
      QStringLiteral("\"%1\"").arg(QCoreApplication::applicationFilePath());
  return QStringLiteral(
             "[Desktop Entry]\n"
             "Type=Application\n"
             "Name=StickySnap\n"
             "Exec=%1\n"
             "X-GNOME-Autostart-enabled=true\n"
             "Hidden=false\n")
      .arg(command);
}

}  // namespace

void configureApplication() {
  // Qt 无主窗口应用在 Linux 下同样不进任务栏，无需额外处理。
}

void activateApplication() {
  for (QWindow* window : QGuiApplication::topLevelWindows()) {
    if (window->isVisible() &&
        (window->flags() & Qt::Window) == Qt::Window) {
      window->requestActivate();
      return;
    }
  }
}

QString appDataDirectory() {
  QString base = QStandardPaths::writableLocation(
      QStandardPaths::GenericConfigLocation);
  if (base.isEmpty()) base = QDir::homePath() + QStringLiteral("/.config");
  return ensureDirectory(base + QStringLiteral("/StickySnap"));
}

QString pinImageDirectory() {
  return ensureDirectory(appDataDirectory() + QStringLiteral("/pins"));
}

QString sessionIndexPath() {
  return appDataDirectory() + QStringLiteral("/index.txt");
}

QString settingsPath() {
  return appDataDirectory() + QStringLiteral("/config.ini");
}

void openPath(const QString& path) {
  QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void openScreenRecordingSettings() {
  // Linux 没有屏幕录制授权页；X11 抓屏无需授权。
}

void configureOverlayWindow(QWidget* window) {
  Q_UNUSED(window);
  // Qt WindowStaysOnTopHint 已覆盖置顶；穿透与层序由其余接口负责。
}

void configurePinWindow(QWidget* window) {
  Q_UNUSED(window);
}

void setClickThrough(QWidget* window, bool enabled) {
  Display* display = x11Display();
  if (!display || !window) return;
  const Window win = static_cast<Window>(window->winId());
  if (enabled) {
    // ShapeInput 置空 → 鼠标事件穿透到下层窗口。
    XShapeCombineRectangles(display, win, ShapeInput, 0, 0, nullptr, 0,
                            ShapeSet, 0);
  } else {
    XShapeCombineMask(display, win, ShapeInput, 0, 0, None, ShapeSet);
  }
  XFlush(display);
}

void setPinRaisedAboveOverlay(QWidget* window, bool raised) {
  if (window && raised) window->raise();
}

void setPinBoundToDesktop(QWidget* window, bool bound) {
  // X11 的贴图窗口默认跨工作区显示；按工作区绑定需要 EWMH 桌面协商，
  // 暂不支持（README 有说明）。
  Q_UNUSED(window);
  Q_UNUSED(bound);
}

bool setAutostart(bool enabled) {
  const QString path = autostartDesktopPath();
  if (!enabled) {
    if (!QFile::exists(path)) return true;
    return QFile::remove(path);
  }
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
  return file.write(autostartDesktopContent().toUtf8()) > 0;
}

bool isAutostart() { return QFile::exists(autostartDesktopPath()); }

}  // namespace sticky::platform
