#include "platform/platform.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QTimer>

#include <windows.h>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace sticky::platform {
namespace {

// WM_HOTKEY ids are global per process; start high to stay out of Qt's way.
constexpr UINT kFirstHotkeyId = 0xB000;
constexpr char kWindowsEventType[] = "windows_generic_MSG";

bool nativeKey(const QKeySequence& sequence, UINT* keyCode) {
  if (sequence.isEmpty() || sequence.count() != 1) return false;
  const int key = sequence[0].key();
  if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
    *keyCode = VK_F1 + static_cast<UINT>(key - Qt::Key_F1);
    return true;
  }
  // Qt letters and digits share the ASCII virtual-key codes.
  if (key >= Qt::Key_A && key <= Qt::Key_Z) {
    *keyCode = static_cast<UINT>(key);
    return true;
  }
  if (key >= Qt::Key_0 && key <= Qt::Key_9) {
    *keyCode = static_cast<UINT>(key);
    return true;
  }
  return false;
}

UINT nativeModifiers(Qt::KeyboardModifiers modifiers) {
  UINT mods = MOD_NOREPEAT;
  if (modifiers & Qt::ShiftModifier) mods |= MOD_SHIFT;
  if (modifiers & Qt::ControlModifier) mods |= MOD_CONTROL;
  // Qt maps MetaModifier to the Windows key.
  if (modifiers & Qt::MetaModifier) mods |= MOD_WIN;
  if (modifiers & Qt::AltModifier) mods |= MOD_ALT;
  return mods;
}

class HotkeyEventFilter : public QAbstractNativeEventFilter {
 public:
  using Dispatch = std::function<void(UINT)>;

  explicit HotkeyEventFilter(Dispatch dispatch)
      : dispatch_(std::move(dispatch)) {}

  bool nativeEventFilter(const QByteArray& eventType, void* message,
                         qint64* result) override {
    Q_UNUSED(result);
    if (eventType != QByteArray(kWindowsEventType)) return false;
    const MSG* msg = static_cast<MSG*>(message);
    if (msg && msg->message == WM_HOTKEY) {
      dispatch_(static_cast<UINT>(msg->wParam));
    }
    return false;
  }

 private:
  Dispatch dispatch_;
};

}  // namespace

struct GlobalHotkeys::Impl {
  struct Entry {
    UINT id = 0;
    GlobalHotkeys::Callback callback;
  };

  std::unique_ptr<HotkeyEventFilter> filter;
  std::vector<Entry> entries;
  UINT nextId = kFirstHotkeyId;

  Impl()
      : filter(new HotkeyEventFilter([this](UINT id) { dispatch(id); })) {
    if (QCoreApplication* app = QCoreApplication::instance()) {
      app->installNativeEventFilter(filter.get());
    }
  }

  ~Impl() {
    if (QCoreApplication* app = QCoreApplication::instance()) {
      app->removeNativeEventFilter(filter.get());
    }
    unregisterAll();
  }

  void unregisterAll() {
    for (const Entry& entry : entries) {
      UnregisterHotKey(nullptr, static_cast<int>(entry.id));
    }
    entries.clear();
    nextId = kFirstHotkeyId;
  }

  void dispatch(UINT id) {
    for (const Entry& entry : entries) {
      if (entry.id != id) continue;
      const auto callback = entry.callback;
      QTimer::singleShot(0, [callback] { callback(); });
      return;
    }
  }
};

GlobalHotkeys::GlobalHotkeys() : impl_(std::make_unique<Impl>()) {}

GlobalHotkeys::~GlobalHotkeys() = default;

void GlobalHotkeys::clear() { impl_->unregisterAll(); }

bool GlobalHotkeys::add(const QKeySequence& sequence, Callback callback,
                        QString* error) {
  UINT keyCode = 0;
  if (!nativeKey(sequence, &keyCode)) {
    if (error) {
      *error = QObject::tr(
          "不支持的全局快捷键 %1。请使用 F1–F12，或搭配修饰键的字母、数字。")
                   .arg(sequence.toString(QKeySequence::NativeText));
    }
    return false;
  }

  const UINT id = impl_->nextId++;
  if (!RegisterHotKey(nullptr, static_cast<int>(id),
                      nativeModifiers(sequence[0].keyboardModifiers()),
                      keyCode)) {
    if (error) {
      *error = QObject::tr("无法注册快捷键 %1（系统错误 %2）。")
                   .arg(sequence.toString(QKeySequence::NativeText))
                   .arg(static_cast<int>(GetLastError()));
    }
    impl_->nextId--;
    return false;
  }

  impl_->entries.push_back(Impl::Entry{id, std::move(callback)});
  return true;
}

}  // namespace sticky::platform
