#include "platform/platform.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QSocketNotifier>
#include <QTimer>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <memory>
#include <utility>
#include <vector>

namespace sticky::platform {
namespace {

// 抓键时同时覆盖 NumLock / CapsLock 组合，避免锁定键按下时热键失效。
constexpr unsigned int kLockMaskCombos[] = {
    0, Mod2Mask, LockMask, Mod2Mask | LockMask};

bool nativeKey(const QKeySequence& sequence, KeySym* keysym) {
  if (sequence.isEmpty() || sequence.count() != 1) return false;
  const int key = sequence[0].key();
  if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
    *keysym = XK_F1 + static_cast<KeySym>(key - Qt::Key_F1);
    return true;
  }
  if (key >= Qt::Key_A && key <= Qt::Key_Z) {
    *keysym = XK_a + static_cast<KeySym>(key - Qt::Key_A);
    return true;
  }
  if (key >= Qt::Key_0 && key <= Qt::Key_9) {
    *keysym = XK_0 + static_cast<KeySym>(key - Qt::Key_0);
    return true;
  }
  return false;
}

unsigned int nativeModifiers(Qt::KeyboardModifiers modifiers) {
  unsigned int mods = 0;
  if (modifiers.testFlag(Qt::ShiftModifier)) mods |= ShiftMask;
  if (modifiers.testFlag(Qt::ControlModifier)) mods |= ControlMask;
  if (modifiers.testFlag(Qt::MetaModifier)) mods |= Mod4Mask;  // Super
  if (modifiers.testFlag(Qt::AltModifier)) mods |= Mod1Mask;
  return mods;
}

}  // namespace

struct GlobalHotkeys::Impl {
  struct Entry {
    KeyCode keycode = 0;
    unsigned int modifiers = 0;
    GlobalHotkeys::Callback callback;
  };

  Display* display = nullptr;
  std::unique_ptr<QSocketNotifier> notifier;
  std::vector<Entry> entries;

  Impl() {
    display = XOpenDisplay(nullptr);
    if (!display) return;
    notifier = std::make_unique<QSocketNotifier>(
        XConnectionNumber(display), QSocketNotifier::Read);
    QObject::connect(notifier.get(), &QSocketNotifier::activated,
                     [this] { drainEvents(); });
  }

  ~Impl() {
    unregisterAll();
    if (display) XCloseDisplay(display);
  }

  void grabKey(const Entry& entry) {
    if (!display) return;
    for (int screen = 0; screen < ScreenCount(display); ++screen) {
      const Window root = RootWindow(display, screen);
      for (unsigned int lock : kLockMaskCombos) {
        XGrabKey(display, entry.keycode, entry.modifiers | lock, root, True,
                 GrabModeAsync, GrabModeAsync);
      }
    }
  }

  void ungrabKey(const Entry& entry) {
    if (!display) return;
    for (int screen = 0; screen < ScreenCount(display); ++screen) {
      const Window root = RootWindow(display, screen);
      for (unsigned int lock : kLockMaskCombos) {
        XUngrabKey(display, entry.keycode, entry.modifiers | lock, root);
      }
    }
  }

  void unregisterAll() {
    for (const Entry& entry : entries) ungrabKey(entry);
    entries.clear();
  }

  void drainEvents() {
    if (!display) return;
    while (XPending(display) > 0) {
      XEvent event;
      XNextEvent(display, &event);
      if (event.type != KeyPress) continue;
      const XKeyEvent& key = event.xkey;
      // 锁定键位不属于热键语义，匹配时剔除。
      const unsigned int state =
          key.state & (ShiftMask | ControlMask | Mod1Mask | Mod4Mask);
      for (const Entry& entry : entries) {
        if (entry.keycode == key.keycode && entry.modifiers == state) {
          const auto callback = entry.callback;
          QTimer::singleShot(0, [callback] { callback(); });
          break;
        }
      }
    }
  }
};

GlobalHotkeys::GlobalHotkeys() : impl_(std::make_unique<Impl>()) {}

GlobalHotkeys::~GlobalHotkeys() = default;

void GlobalHotkeys::clear() { impl_->unregisterAll(); }

bool GlobalHotkeys::add(const QKeySequence& sequence, Callback callback,
                        QString* error) {
  KeySym keysym = 0;
  if (!nativeKey(sequence, &keysym)) {
    if (error) {
      *error = QObject::tr(
          "不支持的全局快捷键 %1。请使用 F1–F12，或搭配修饰键的字母、数字。")
                   .arg(sequence.toString(QKeySequence::NativeText));
    }
    return false;
  }
  if (!impl_->display) {
    if (error) {
      *error = QObject::tr("无法连接 X 显示服务器，全局快捷键不可用。");
    }
    return false;
  }

  const KeyCode keycode = XKeysymToKeycode(impl_->display, keysym);
  if (keycode == 0) {
    if (error) {
      *error = QObject::tr("无法映射按键 %1。")
                   .arg(sequence.toString(QKeySequence::NativeText));
    }
    return false;
  }

  Impl::Entry entry;
  entry.keycode = keycode;
  entry.modifiers = nativeModifiers(sequence[0].keyboardModifiers());
  entry.callback = std::move(callback);
  impl_->grabKey(entry);
  impl_->entries.push_back(std::move(entry));
  return true;
}

}  // namespace sticky::platform
