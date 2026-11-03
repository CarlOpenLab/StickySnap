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

// App data locations and small platform integrations.
void configureApplication();
void activateApplication();  // bring a menu-bar app forward before a dialog
QString appDataDirectory();
QString pinImageDirectory();
QString sessionIndexPath();
QString settingsPath();
void openPath(const QString& path);
void openScreenRecordingSettings();

}  // namespace sticky::platform
