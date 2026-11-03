#include "platform/platform.h"

#import <AppKit/AppKit.h>

#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QUrl>
#include <QWidget>

namespace sticky::platform {
namespace {

bool isCocoa() {
  return QGuiApplication::platformName() == QLatin1String("cocoa");
}

NSWindow* nativeWindow(QWidget* widget) {
  if (!widget) return nil;
  NSView* view = (__bridge NSView*)(reinterpret_cast<void*>(widget->winId()));
  return view.window;
}

QString ensureDirectory(const QString& path) {
  QDir().mkpath(path);
  return path;
}

}  // namespace

void configureApplication() {
  [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}

void activateApplication() {
  if (!isCocoa()) return;
  [NSApp activateIgnoringOtherApps:YES];
}

QString appDataDirectory() {
  QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (path.isEmpty()) {
    path = QDir::homePath() + QStringLiteral(
                                 "/Library/Application Support/StickySnap");
  }
  return ensureDirectory(path);
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
  NSURL* url = [NSURL
      URLWithString:@"x-apple.systempreferences:com.apple.preference.security?"
                    @"Privacy_ScreenCapture"];
  [[NSWorkspace sharedWorkspace] openURL:url];
}

void configureOverlayWindow(QWidget* window) {
  if (!isCocoa()) return;
  NSWindow* native = nativeWindow(window);
  if (!native) return;
  native.level = NSStatusWindowLevel;
  native.collectionBehavior =
      NSWindowCollectionBehaviorCanJoinAllSpaces |
      NSWindowCollectionBehaviorStationary |
      NSWindowCollectionBehaviorIgnoresCycle |
      NSWindowCollectionBehaviorFullScreenAuxiliary;
  native.opaque = NO;
  native.backgroundColor = [NSColor clearColor];
  native.hasShadow = NO;
  native.hidesOnDeactivate = NO;
  native.movable = NO;
  // AppKit's default open animation scales/fades a new window in. On a
  // full-screen overlay that reads as the frozen page shaking for ~200 ms.
  native.animationBehavior = NSWindowAnimationBehaviorNone;
  [native setIgnoresMouseEvents:NO];
  [native makeKeyAndOrderFront:nil];
}

void configurePinWindow(QWidget* window) {
  if (!isCocoa()) return;
  NSWindow* native = nativeWindow(window);
  if (!native) return;
  native.level = NSFloatingWindowLevel;
  native.collectionBehavior =
      NSWindowCollectionBehaviorCanJoinAllSpaces |
      NSWindowCollectionBehaviorStationary |
      NSWindowCollectionBehaviorIgnoresCycle |
      NSWindowCollectionBehaviorFullScreenAuxiliary;
  native.opaque = NO;
  native.backgroundColor = [NSColor clearColor];
  native.hasShadow = NO;
  native.hidesOnDeactivate = NO;
  [native setIgnoresMouseEvents:NO];
}

void setClickThrough(QWidget* window, bool enabled) {
  if (!isCocoa()) return;
  NSWindow* native = nativeWindow(window);
  if (!native) return;
  [native setIgnoresMouseEvents:enabled ? YES : NO];
}

void setPinRaisedAboveOverlay(QWidget* window, bool raised) {
  if (!isCocoa()) return;
  NSWindow* native = nativeWindow(window);
  if (!native) return;
  native.level = raised ? NSStatusWindowLevel + 1 : NSFloatingWindowLevel;
}

}  // namespace sticky::platform
