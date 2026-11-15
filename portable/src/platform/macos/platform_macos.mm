#include "platform/platform.h"

#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>
#import <Vision/Vision.h>

#include <QDesktopServices>
#include <QDir>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QUrl>
#include <QWidget>

#include <QDebug>

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

void setPinBoundToDesktop(QWidget* window, bool bound) {
  if (!isCocoa()) return;
  NSWindow* native = nativeWindow(window);
  if (!native) return;
  if (bound) {
    // 不再跟随所有空间：贴图只停留在它当前所在的空间。
    native.collectionBehavior =
        NSWindowCollectionBehaviorMoveToActiveSpace |
        NSWindowCollectionBehaviorIgnoresCycle |
        NSWindowCollectionBehaviorFullScreenAuxiliary;
  } else {
    native.collectionBehavior =
        NSWindowCollectionBehaviorCanJoinAllSpaces |
        NSWindowCollectionBehaviorStationary |
        NSWindowCollectionBehaviorIgnoresCycle |
        NSWindowCollectionBehaviorFullScreenAuxiliary;
  }
}

bool setAutostart(bool enabled) {
  if (@available(macOS 13.0, *)) {
    SMAppService* service = [SMAppService mainApp];
    NSError* error = nil;
    if (enabled) {
      if (service.status == SMAppServiceStatusEnabled) return true;
      if (![service registerAndReturnError:&error]) {
        qWarning("StickySnap: cannot register login item: %s",
                 error.localizedDescription.UTF8String);
        return false;
      }
      return true;
    }
    if (service.status != SMAppServiceStatusEnabled) return true;
    if (![service unregisterAndReturnError:&error]) {
      qWarning("StickySnap: cannot unregister login item: %s",
               error.localizedDescription.UTF8String);
      return false;
    }
    return true;
  }
  return false;
}

bool isAutostart() {
  if (@available(macOS 13.0, *)) {
    return [SMAppService mainApp].status == SMAppServiceStatusEnabled;
  }
  return false;
}

bool ensureUiElementPermission() {
  if (AXIsProcessTrusted()) return true;
  // 首次调用触发系统的辅助功能授权弹窗。
  NSDictionary* options = @{(id)kAXTrustedCheckOptionPrompt: @YES};
  return AXIsProcessTrustedWithOptions(options) == YES;
}

QRect uiElementAt(const QPoint& globalPoint) {
  if (!AXIsProcessTrusted()) return {};  // 未授权时不反复弹窗

  AXUIElementRef systemWide = AXUIElementCreateSystemWide();
  if (!systemWide) return {};
  AXUIElementRef element = nullptr;
  QRect result;
  if (AXUIElementCopyElementAtPosition(systemWide, globalPoint.x(),
                                       globalPoint.y(),
                                       &element) == kAXErrorSuccess &&
      element) {
    pid_t pid = -1;
    AXUIElementGetPid(element, &pid);
    if (pid != getpid()) {
      CGPoint position = {0, 0};
      CGSize size = {0, 0};
      CFTypeRef value = nullptr;
      if (AXUIElementCopyAttributeValue(element, kAXPositionAttribute,
                                        &value) == kAXErrorSuccess &&
          value) {
        AXValueGetValue(reinterpret_cast<AXValueRef>(value),
                        kAXValueCGPointType, &position);
        CFRelease(value);
      }
      if (AXUIElementCopyAttributeValue(element, kAXSizeAttribute,
                                        &value) == kAXErrorSuccess &&
          value) {
        AXValueGetValue(reinterpret_cast<AXValueRef>(value),
                        kAXValueCGSizeType, &size);
        CFRelease(value);
      }
      if (size.width >= 2.0 && size.height >= 2.0) {
        result = QRect(qRound(position.x), qRound(position.y),
                       qRound(size.width), qRound(size.height));
      }
    }
    CFRelease(element);
  }
  CFRelease(systemWide);
  return result;
}

void ocrText(const QImage& image, OcrCallback callback) {
  if (image.isNull()) {
    callback({}, QStringLiteral("图片为空。"));
    return;
  }

  // QImage ARGB32 = BGRA 字节序，与 kCGImageAlphaNoneSkipLast + 32Big 一致。
  QImage argb = image.convertToFormat(QImage::Format_ARGB32);
  CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
  CGContextRef context = CGBitmapContextCreate(
      argb.bits(), argb.width(), argb.height(), 8, argb.bytesPerLine(),
      colorSpace,
      kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big);
  CGColorSpaceRelease(colorSpace);
  if (!context) {
    callback({}, QStringLiteral("图片转换失败。"));
    return;
  }
  CGImageRef cgImage = CGBitmapContextCreateImage(context);
  CGContextRelease(context);
  if (!cgImage) {
    callback({}, QStringLiteral("图片转换失败。"));
    return;
  }

  dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
    NSError* error = nil;
    __block NSString* recognized = nil;

    VNRecognizeTextRequest* request = [[VNRecognizeTextRequest alloc]
        initWithCompletionHandler:^(VNRequest* inner) {
          NSMutableArray<NSString*>* lines = [NSMutableArray array];
          for (VNRecognizedTextObservation* observation in inner.results) {
            VNRecognizedText* candidate =
                [observation topCandidates:1].firstObject;
            if (candidate) [lines addObject:candidate.string];
          }
          recognized = [lines componentsJoinedByString:@"\n"];
        }];
    request.recognitionLevel = VNRequestTextRecognitionLevelAccurate;
    request.recognitionLanguages = @[ @"zh-Hans", @"en-US" ];

    VNImageRequestHandler* handler = [[VNImageRequestHandler alloc]
        initWithCGImage:cgImage
                options:@{}];
    if (![handler performRequests:@[ request ] error:&error]) {
      dispatch_async(dispatch_get_main_queue(), ^{
        callback({}, QString::fromNSString(error.localizedDescription));
      });
    } else {
      dispatch_async(dispatch_get_main_queue(), ^{
        callback(QString::fromNSString(recognized), QString());
      });
    }
    CGImageRelease(cgImage);
  });
}

}  // namespace sticky::platform
