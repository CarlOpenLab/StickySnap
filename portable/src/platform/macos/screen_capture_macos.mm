#include "platform/platform.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <QGuiApplication>
#include <QObject>
#include <QScreen>
#include <QString>

#include <algorithm>
#include <cstring>

namespace sticky::platform {
namespace {

QScreen* screenForFrame(CGRect frame) {
  const QRect rect(qRound(frame.origin.x), qRound(frame.origin.y),
                   qRound(frame.size.width), qRound(frame.size.height));
  QScreen* best = nullptr;
  int bestArea = 0;
  for (QScreen* screen : QGuiApplication::screens()) {
    const QRect overlap = screen->geometry().intersected(rect);
    const int area = overlap.width() * overlap.height();
    if (area > bestArea) {
      bestArea = area;
      best = screen;
    }
  }
  return best;
}

QImage imageFromCGImage(CGImageRef cgImage) {
  if (!cgImage) return {};
  const size_t width = CGImageGetWidth(cgImage);
  const size_t height = CGImageGetHeight(cgImage);
  if (width == 0 || height == 0) return {};

  CGDataProviderRef provider = CGImageGetDataProvider(cgImage);
  if (!provider) return {};
  CFDataRef data = CGDataProviderCopyData(provider);
  if (!data) return {};

  const uint8_t* bytes = CFDataGetBytePtr(data);
  const size_t stride = CGImageGetBytesPerRow(cgImage);
  QImage image(static_cast<int>(width), static_cast<int>(height),
               QImage::Format_ARGB32_Premultiplied);
  for (size_t y = 0; y < height; ++y) {
    std::memcpy(image.scanLine(static_cast<int>(y)), bytes + y * stride,
                width * 4);
  }
  CFRelease(data);
  return image;
}

}  // namespace

bool ensureScreenCapturePermission() {
  if (CGPreflightScreenCaptureAccess()) return true;
  // An accessory app is not active, and macOS attaches the TCC alert to the
  // active app. Bring StickySnap forward so the prompt is actually visible.
  [NSApp activateIgnoringOtherApps:YES];
  CGRequestScreenCaptureAccess();
  return CGPreflightScreenCaptureAccess();
}

QVector<DisplayCapture> captureDisplays(QString* error) {
  QVector<DisplayCapture> captures;
  if (!CGPreflightScreenCaptureAccess()) {
    if (error) {
      *error = QObject::tr(
          "Screen Recording permission is not granted. Enable StickySnap in "
          "System Settings > Privacy & Security > Screen Recording.");
    }
    return captures;
  }

  __block SCShareableContent* content = nil;
  __block NSError* contentError = nil;
  dispatch_semaphore_t contentSem = dispatch_semaphore_create(0);
  [SCShareableContent
      getShareableContentExcludingDesktopWindows:NO
                             onScreenWindowsOnly:YES
                              completionHandler:^(SCShareableContent* result,
                                                  NSError* captureError) {
                                content = result;
                                contentError = captureError;
                                dispatch_semaphore_signal(contentSem);
                              }];
  if (dispatch_semaphore_wait(
          contentSem, dispatch_time(DISPATCH_TIME_NOW, 8 * NSEC_PER_SEC)) != 0) {
    if (error) *error = QObject::tr("Timed out while reading displays.");
    return captures;
  }
  if (contentError || !content) {
    if (error) {
      *error = contentError
                   ? QString::fromNSString(contentError.localizedDescription)
                   : QObject::tr("Screen capture is unavailable.");
    }
    return captures;
  }

  NSMutableArray<SCWindow*>* ownWindows = [NSMutableArray array];
  const pid_t ownPid = getpid();
  for (SCWindow* window in content.windows) {
    SCRunningApplication* app = window.owningApplication;
    if (app && app.processID == ownPid) [ownWindows addObject:window];
  }

  for (SCDisplay* display in content.displays) {
    SCContentFilter* filter =
        [[SCContentFilter alloc] initWithDisplay:display
                               excludingWindows:ownWindows];
    SCStreamConfiguration* configuration = [[SCStreamConfiguration alloc] init];
    configuration.width = display.width;
    configuration.height = display.height;
    configuration.showsCursor = NO;
    configuration.captureResolution = SCCaptureResolutionBest;

    __block CGImageRef cgImage = NULL;
    __block NSError* captureError = nil;
    dispatch_semaphore_t captureSem = dispatch_semaphore_create(0);
    [SCScreenshotManager
        captureImageWithFilter:filter
                 configuration:configuration
             completionHandler:^(CGImageRef image, NSError* error) {
               if (image) cgImage = CGImageRetain(image);
               captureError = error;
               dispatch_semaphore_signal(captureSem);
             }];
    if (dispatch_semaphore_wait(
            captureSem, dispatch_time(DISPATCH_TIME_NOW, 8 * NSEC_PER_SEC)) !=
        0) {
      if (error) *error = QObject::tr("Timed out while capturing a display.");
      continue;
    }
    if (captureError || !cgImage) {
      if (error) {
        *error = captureError
                     ? QString::fromNSString(captureError.localizedDescription)
                     : QObject::tr("Screen capture failed.");
      }
      if (cgImage) CGImageRelease(cgImage);
      continue;
    }

    DisplayCapture capture;
    capture.image = imageFromCGImage(cgImage);
    CGImageRelease(cgImage);
    if (capture.image.isNull()) continue;

    const CGRect frame = display.frame;
    capture.geometry = QRect(qRound(frame.origin.x), qRound(frame.origin.y),
                             qRound(frame.size.width),
                             qRound(frame.size.height));
    capture.screen = screenForFrame(frame);
    if (capture.geometry.width() > 0) {
      capture.dpr = static_cast<qreal>(capture.image.width()) /
                    static_cast<qreal>(capture.geometry.width());
    }
    captures.push_back(std::move(capture));
  }

  if (captures.isEmpty() && error && error->isEmpty()) {
    *error = QObject::tr("No displays were captured.");
  }
  return captures;
}

QRect windowAt(const QPoint& globalPoint) {
  CFArrayRef windows = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
      kCGNullWindowID);
  if (!windows) return {};

  const pid_t ownPid = getpid();
  QRect found;
  for (NSDictionary* info in (__bridge NSArray*)windows) {
    const int layer = [info[(__bridge NSString*)kCGWindowLayer] intValue];
    if (layer != 0) continue;

    const pid_t pid = [info[(__bridge NSString*)kCGWindowOwnerPID] intValue];
    if (pid == ownPid) continue;

    CGRect bounds;
    if (!CGRectMakeWithDictionaryRepresentation(
            (__bridge CFDictionaryRef)info[(__bridge NSString*)kCGWindowBounds],
            &bounds)) {
      continue;
    }
    if (bounds.size.width < 2.0 || bounds.size.height < 2.0) continue;

    const QRect rect(qRound(bounds.origin.x), qRound(bounds.origin.y),
                     qRound(bounds.size.width), qRound(bounds.size.height));
    if (rect.contains(globalPoint)) {
      found = rect;
      break;
    }
  }
  CFRelease(windows);
  return found;
}

}  // namespace sticky::platform
