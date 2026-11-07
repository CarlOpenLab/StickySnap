#pragma once

#include <QColor>
#include <QKeySequence>
#include <QString>

namespace sticky::core {

// User-editable configuration, persisted as key=value text.
struct Settings {
  QColor accent = QColor(QStringLiteral("#2F6FED"));
#ifdef Q_OS_MACOS
  // Command+Control+A: Qt maps Command to ControlModifier and Control to
  // MetaModifier on macOS.
  QKeySequence hotkeyShot = QKeySequence(Qt::CTRL | Qt::META | Qt::Key_A);
#else
  QKeySequence hotkeyShot = QKeySequence(Qt::Key_F1);
#endif
  QKeySequence hotkeyPin = QKeySequence(Qt::Key_F3);
  int dimPercent = 30;
  // 贴图窗常显 1px 边框（外黑内白，悬停时内圈换成强调色）。
  bool pinBorder = true;
};

// True when the sequence can be registered as a global shortcut. Function
// keys may stand alone; letters and digits need Ctrl/Meta/Alt so a plain
// keystroke is never stolen from the focused app.
bool isSupportedHotkey(const QKeySequence& sequence);

// 进程级共享的贴图外观：贴图窗首次创建时从 config.ini 载入，设置对话框
// 保存后更新，已打开的贴图窗据此重绘。
struct PinAppearance {
  bool border = true;
  QColor accent = QColor(QStringLiteral("#2F6FED"));
};

PinAppearance pinAppearance();
void setPinAppearance(const PinAppearance& appearance);

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
