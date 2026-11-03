#include "platform/platform.h"

#import <Carbon/Carbon.h>

#include <QTimer>

#include <cstdint>
#include <vector>

namespace sticky::platform {
namespace {

constexpr UInt32 kFunctionKeyCodes[12] = {
    kVK_F1,  kVK_F2,  kVK_F3,  kVK_F4,  kVK_F5,  kVK_F6,
    kVK_F7,  kVK_F8,  kVK_F9,  kVK_F10, kVK_F11, kVK_F12};

constexpr UInt32 kLetterKeyCodes[26] = {
    kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E,
    kVK_ANSI_F, kVK_ANSI_G, kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J,
    kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N, kVK_ANSI_O,
    kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T,
    kVK_ANSI_U, kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y,
    kVK_ANSI_Z};

constexpr UInt32 kDigitKeyCodes[10] = {
    kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
    kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9};

bool nativeKey(const QKeySequence& sequence, UInt32* keyCode) {
  if (sequence.isEmpty() || sequence.count() != 1) return false;
  const int key = sequence[0].key();
  if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
    *keyCode = kFunctionKeyCodes[key - Qt::Key_F1];
    return true;
  }
  if (key >= Qt::Key_A && key <= Qt::Key_Z) {
    *keyCode = kLetterKeyCodes[key - Qt::Key_A];
    return true;
  }
  if (key >= Qt::Key_0 && key <= Qt::Key_9) {
    *keyCode = kDigitKeyCodes[key - Qt::Key_0];
    return true;
  }
  return false;
}

UInt32 nativeModifiers(Qt::KeyboardModifiers modifiers) {
  UInt32 carbon = 0;
  if (modifiers & Qt::ShiftModifier) carbon |= shiftKey;
  // Qt maps ControlModifier to Command on macOS and MetaModifier to Control.
  if (modifiers & Qt::ControlModifier) carbon |= cmdKey;
  if (modifiers & Qt::MetaModifier) carbon |= controlKey;
  if (modifiers & Qt::AltModifier) carbon |= optionKey;
  return carbon;
}

}  // namespace

struct GlobalHotkeys::Impl {
  struct Entry {
    UInt32 id = 0;
    EventHotKeyRef ref = nullptr;
    GlobalHotkeys::Callback callback;
  };

  EventHandlerRef handler = nullptr;
  std::vector<Entry> entries;
  UInt32 nextId = 1;

  Impl() {
    const EventTypeSpec type = {kEventClassKeyboard, kEventHotKeyPressed};
    InstallApplicationEventHandler(&Impl::handle, 1, &type, this, &handler);
  }

  ~Impl() {
    for (const Entry& entry : entries) {
      if (entry.ref) UnregisterEventHotKey(entry.ref);
    }
    if (handler) RemoveEventHandler(handler);
  }

  void dispatch(UInt32 id) {
    for (const Entry& entry : entries) {
      if (entry.id != id) continue;
      const auto callback = entry.callback;
      QTimer::singleShot(0, [callback] { callback(); });
      return;
    }
  }

  static OSStatus handle(EventHandlerCallRef, EventRef event, void* userData) {
    EventHotKeyID hotkeyId{};
    GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID,
                      nullptr, sizeof(hotkeyId), nullptr, &hotkeyId);
    static_cast<Impl*>(userData)->dispatch(hotkeyId.id);
    return noErr;
  }
};

GlobalHotkeys::GlobalHotkeys() : impl_(std::make_unique<Impl>()) {}

GlobalHotkeys::~GlobalHotkeys() = default;

void GlobalHotkeys::clear() {
  for (const Impl::Entry& entry : impl_->entries) {
    if (entry.ref) UnregisterEventHotKey(entry.ref);
  }
  impl_->entries.clear();
  impl_->nextId = 1;
}

bool GlobalHotkeys::add(const QKeySequence& sequence, Callback callback,
                        QString* error) {
  UInt32 keyCode = 0;
  if (!nativeKey(sequence, &keyCode)) {
    if (error) {
      *error = QObject::tr(
          "Unsupported global hotkey %1. Use F1-F12, or a letter/digit with "
          "a modifier.")
                   .arg(sequence.toString(QKeySequence::NativeText));
    }
    return false;
  }

  const UInt32 id = impl_->nextId++;
  const EventHotKeyID hotkeyId = {'SSNP', id};
  EventHotKeyRef ref = nullptr;
  const OSStatus status =
      RegisterEventHotKey(keyCode, nativeModifiers(sequence[0].keyboardModifiers()),
                          hotkeyId, GetApplicationEventTarget(), 0, &ref);
  if (status != noErr) {
    if (error) {
      *error = QObject::tr("Could not register %1 (system error %2).")
                   .arg(sequence.toString(QKeySequence::NativeText))
                   .arg(static_cast<int>(status));
    }
    return false;
  }

  impl_->entries.push_back(Impl::Entry{id, ref, std::move(callback)});
  return true;
}

}  // namespace sticky::platform
