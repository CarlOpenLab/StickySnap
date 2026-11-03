#pragma once

#include <QColor>
#include <QKeySequence>
#include <QString>

namespace sticky::core {

// User-editable configuration, persisted as key=value text.
struct Settings {
  QColor accent = QColor(QStringLiteral("#FF4B3E"));
#ifdef Q_OS_MACOS
  // Command+Control+A: Qt maps Command to ControlModifier and Control to
  // MetaModifier on macOS.
  QKeySequence hotkeyShot = QKeySequence(Qt::CTRL | Qt::META | Qt::Key_A);
#else
  QKeySequence hotkeyShot = QKeySequence(Qt::Key_F1);
#endif
  QKeySequence hotkeyPin = QKeySequence(Qt::Key_F3);
  int dimPercent = 30;
};

// True when the sequence can be registered as a global shortcut. Function
// keys may stand alone; letters and digits need Ctrl/Meta/Alt so a plain
// keystroke is never stolen from the focused app.
bool isSupportedHotkey(const QKeySequence& sequence);

// Reads and writes config.ini. Missing files yield defaults; invalid values
// fall back per field rather than rejecting the whole file.
class SettingsStore {
 public:
  explicit SettingsStore(QString filePath);

  Settings load(QString* error = nullptr) const;
  bool save(const Settings& settings, QString* error = nullptr) const;

  static Settings parse(const QString& text);
  static QString serialize(const Settings& settings);

 private:
  QString filePath_;
};

}  // namespace sticky::core
