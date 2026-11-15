#include "platform/platform.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <CoreServices/CoreServices.h>
#import <Vision/Vision.h>

#include <QCoreApplication>
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

// 登录项走 LSSharedFileList（系统设置 > 通用 > 登录项 可见）。SMAppService
// 在 CI 的 Xcode 26 SDK 上头文件只有前向声明，为保编译兼容统一用旧 API。
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace {

NSURL* sharedApplicationUrl() {
  const QString path = QCoreApplication::applicationFilePath();
  return [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.toUtf8().constData()]];
}

LSSharedFileListRef sharedLoginItemsList() {
  return LSSharedFileListCreate(kCFAllocatorDefault,
                                kLSSharedFileListSessionLoginItems, nullptr);
}

}  // namespace

bool setAutostart(bool enabled) {
  LSSharedFileListRef list = sharedLoginItemsList();
  if (!list) return false;

  NSURL* appUrl = sharedApplicationUrl();
  bool ok = false;
  if (enabled) {
    LSSharedFileListItemRef item = LSSharedFileListInsertItemURL(
        list, kLSSharedFileListItemLast, nullptr, nullptr,
        (__bridge CFURLRef)appUrl, nullptr, nullptr);
    ok = item != nullptr;
    if (item) CFRelease(item);
  } else {
    UInt32 seed = 0;
    CFArrayRef snapshot = LSSharedFileListCopySnapshot(list, &seed);
    ok = true;
    if (snapshot) {
      for (CFIndex i = 0; i < CFArrayGetCount(snapshot); ++i) {
        LSSharedFileListItemRef item = reinterpret_cast<LSSharedFileListItemRef>(
            CFArrayGetValueAtIndex(snapshot, i));
        CFURLRef url = LSSharedFileListItemCopyResolvedURL(
            item,
            kLSSharedFileListNoUserInteraction | kLSSharedFileListDoNotMountVolumes,
            nullptr);
        if (url) {
          NSURL* itemUrl = (__bridge NSURL*)url;
          if ([itemUrl.path isEqualToString:appUrl.path]) {
            ok = LSSharedFileListItemRemove(list, item) == noErr;
          }
          CFRelease(url);
        }
      }
      CFRelease(snapshot);
    }
  }
  CFRelease(list);
  return ok;
}

bool isAutostart() {
  LSSharedFileListRef list = sharedLoginItemsList();
  if (!list) return false;

  NSURL* appUrl = sharedApplicationUrl();
  bool found = false;
  UInt32 seed = 0;
  CFArrayRef snapshot = LSSharedFileListCopySnapshot(list, &seed);
  if (snapshot) {
    for (CFIndex i = 0; i < CFArrayGetCount(snapshot); ++i) {
      LSSharedFileListItemRef item = reinterpret_cast<LSSharedFileListItemRef>(
          CFArrayGetValueAtIndex(snapshot, i));
      CFURLRef url = LSSharedFileListItemCopyResolvedURL(
          item,
          kLSSharedFileListNoUserInteraction | kLSSharedFileListDoNotMountVolumes,
          nullptr);
      if (url) {
        NSURL* itemUrl = (__bridge NSURL*)url;
        if ([itemUrl.path isEqualToString:appUrl.path]) found = true;
        CFRelease(url);
      }
      if (found) break;
    }
    CFRelease(snapshot);
  }
  CFRelease(list);
  return found;
}

#pragma clang diagnostic pop

bool ensureUiElementPermission() {
  if (AXIsProcessTrusted()) return true;
  // 首次调用触发系统的辅助功能授权弹窗。
  NSDictionary* options = @{(__bridge id)kAXTrustedCheckOptionPrompt: @YES};
  return AXIsProcessTrustedWithOptions(
             (__bridge CFDictionaryRef)options) == YES;
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
        // 新 SDK 把 kAXValueCGPointType 声明成 const UInt32，需显式转枚举。
        AXValueGetValue(reinterpret_cast<AXValueRef>(value),
                        static_cast<AXValueType>(kAXValueCGPointType),
                        &position);
        CFRelease(value);
      }
      if (AXUIElementCopyAttributeValue(element, kAXSizeAttribute,
                                        &value) == kAXErrorSuccess &&
          value) {
        AXValueGetValue(reinterpret_cast<AXValueRef>(value),
                        static_cast<AXValueType>(kAXValueCGSizeType),
                        &size);
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
        initWithCompletionHandler:^(VNRequest* inner,
                                    NSError* requestError) {
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
