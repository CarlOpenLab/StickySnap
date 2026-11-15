#include "platform/platform.h"

#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QPoint>
#include <QSettings>
#include <QStandardPaths>
#include <QWidget>
#include <QWindow>

#include <windows.h>
#include <shellapi.h>

namespace sticky::platform {
namespace {

// Same Run-key value the Windows native build uses (src/autostart.h), so both
// flavors of the app read and write the same setting.
constexpr char kRunKey[] = "HKEY_CURRENT_USER/Software/Microsoft/Windows/CurrentVersion/Run";
constexpr char kRunValueName[] = "StickySnap";

QString ensureDirectory(const QString& path) {
  QDir().mkpath(path);
  return path;
}

HWND hwndOf(QWidget* widget) {
  if (!widget) return nullptr;
  return reinterpret_cast<HWND>(widget->winId());
}

void restyleExStyle(HWND hwnd, LONG_PTR add, LONG_PTR remove) {
  const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  const LONG_PTR restyled = (style | add) & ~remove;
  if (restyled == style) return;
  SetWindowLongPtrW(hwnd, GWL_EXSTYLE, restyled);
  SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

// Pins and overlays must never show up in later screenshots; BitBlt honors
// WDA_EXCLUDEFROMCAPTURE just like the Windows native build.
void excludeFromCapture(HWND hwnd) {
  SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE);
}

void raiseTopmost(HWND hwnd) {
  SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

}  // namespace

void configureApplication() {
  // Qt creates no main window for this app, so nothing shows in the taskbar;
  // there is no Windows equivalent of the accessory activation policy.
}

void activateApplication() {
  // Menu-bar apps have no obvious foreground window on macOS; on Windows the
  // settings dialog activates itself, but help it along if a window exists.
  for (QWindow* window : QGuiApplication::topLevelWindows()) {
    if (window->isVisible() &&
        (window->flags() & Qt::Window) == Qt::Window) {
      window->requestActivate();
      return;
    }
  }
}

QString appDataDirectory() {
  // GenericDataLocation is %APPDATA% (Roaming) on Windows, so this lands on
  // the same directory the native build uses: %APPDATA%\StickySnap.
  QString base =
      QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
  if (base.isEmpty()) {
    base = QDir::homePath() + QStringLiteral("/AppData/Roaming");
  }
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
  ShellExecuteW(nullptr, L"open", reinterpret_cast<LPCWSTR>(path.utf16()),
                nullptr, nullptr, SW_SHOWNORMAL);
}

void openScreenRecordingSettings() {
  // No Windows counterpart; captures never hit the permission dialog.
}

void configureOverlayWindow(QWidget* window) {
  HWND hwnd = hwndOf(window);
  if (!hwnd) return;
  restyleExStyle(hwnd, WS_EX_TOOLWINDOW, 0);
  excludeFromCapture(hwnd);
  raiseTopmost(hwnd);
}

void configurePinWindow(QWidget* window) {
  HWND hwnd = hwndOf(window);
  if (!hwnd) return;
  restyleExStyle(hwnd, WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, 0);
  excludeFromCapture(hwnd);
  raiseTopmost(hwnd);
}

void setClickThrough(QWidget* window, bool enabled) {
  HWND hwnd = hwndOf(window);
  if (!hwnd) return;
  restyleExStyle(hwnd, enabled ? WS_EX_TRANSPARENT : 0,
                 enabled ? 0 : WS_EX_TRANSPARENT);
}

void setPinRaisedAboveOverlay(QWidget* window, bool raised) {
  HWND hwnd = hwndOf(window);
  if (!hwnd || !raised) return;
  // Overlay windows are created after the pins are raised, so re-asserting
  // TOPMOST here keeps pins floating above the frozen-screen overlay.
  raiseTopmost(hwnd);
}

void setPinBoundToDesktop(QWidget* window, bool bound) {
  // Windows 的 TOPMOST 窗口天然显示在所有虚拟桌面，没有公开 API 按 HID
  // 绑定；保持跨桌面常显（README 有说明）。
  Q_UNUSED(window);
  Q_UNUSED(bound);
}

bool setAutostart(bool enabled) {
  QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
  if (enabled) {
    const QString command =
        QStringLiteral("\"%1\"").arg(QCoreApplication::applicationFilePath());
    run.setValue(QString::fromLatin1(kRunValueName), command);
  } else {
    run.remove(QString::fromLatin1(kRunValueName));
  }
  run.sync();
  return run.status() == QSettings::NoError;
}

bool isAutostart() {
  QSettings run(QString::fromLatin1(kRunKey), QSettings::NativeFormat);
  return run.contains(QString::fromLatin1(kRunValueName));
}

}  // namespace sticky::platform
