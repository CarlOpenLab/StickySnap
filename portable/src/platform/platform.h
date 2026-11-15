#pragma once

#include <QImage>
#include <QKeySequence>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

class QScreen;
class QWidget;

namespace sticky::platform {

// One frozen display, captured at native pixel resolution.
struct DisplayCapture {
  QScreen* screen = nullptr;
  QRect geometry;   // global logical points
  QImage image;     // native pixels
  qreal dpr = 1.0;  // image pixels per logical point
};

// Returns false when macOS screen-recording permission is missing. May trigger
// the system permission prompt.
bool ensureScreenCapturePermission();
QVector<DisplayCapture> captureDisplays(QString* error = nullptr);

// Topmost normal window containing the given global point. Returns a null rect
// when the point is over the desktop or over one of our own windows.
QRect windowAt(const QPoint& globalPoint);

// Smallest accessibility UI element containing the given global point
// (button / text field / menu item level). Returns a null rect when the
// point is over one of our own windows, over nothing tractable, or the
// platform denies accessibility access.
QRect uiElementAt(const QPoint& globalPoint);

// Windows has no accessibility gate and always returns true; macOS triggers
// the system Accessibility permission prompt on first call.
bool ensureUiElementPermission();

// Global shortcuts backed by each platform's native mechanism.
class GlobalHotkeys {
 public:
  using Callback = std::function<void()>;

  GlobalHotkeys();
  ~GlobalHotkeys();
  GlobalHotkeys(const GlobalHotkeys&) = delete;
  GlobalHotkeys& operator=(const GlobalHotkeys&) = delete;

  bool add(const QKeySequence& sequence, Callback callback,
           QString* error = nullptr);
  void clear();  // unregister everything, e.g. while editing shortcuts

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Window behavior that Qt cannot express portably.
void configureOverlayWindow(QWidget* window);
void configurePinWindow(QWidget* window);
void setClickThrough(QWidget* window, bool enabled);
void setPinRaisedAboveOverlay(QWidget* window, bool raised);

// 仅在当前桌面/空间显示（macOS 走 NSWindow collectionBehavior；Windows 的
// TOPMOST 贴图天然跨虚拟桌面，此调用为 no-op）。
void setPinBoundToDesktop(QWidget* window, bool bound);

// App data locations and small platform integrations.
void configureApplication();
void activateApplication();  // bring a menu-bar app forward before a dialog
QString appDataDirectory();
QString pinImageDirectory();
QString sessionIndexPath();
QString settingsPath();
void openPath(const QString& path);
void openScreenRecordingSettings();

// Launch at login. Returns false when the platform refused the change.
bool setAutostart(bool enabled);
bool isAutostart();

// 在本机系统 OCR 引擎上识别图片文字（Windows: Windows.Media.Ocr；
// macOS: Vision）。异步完成，callback 会被 marshaled 回 GUI 线程调用；
// 失败时 text 为空、error 说明原因。平台不支持时 error 说明原因。
using OcrCallback = std::function<void(const QString& text, const QString& error)>;
void ocrText(const QImage& image, OcrCallback callback);

}  // namespace sticky::platform
