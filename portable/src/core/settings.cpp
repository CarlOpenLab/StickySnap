#include "core/settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>

namespace sticky::core {
namespace {

constexpr int kDimMin = 10;
constexpr int kDimMax = 90;

QString trim(const QString& value) { return value.trimmed(); }

bool parseAccent(const QString& value, QColor* accent) {
  static const QRegularExpression pattern(
      QStringLiteral("^#[0-9A-Fa-f]{6}$"));
  if (!pattern.match(value).hasMatch()) return false;
  const QColor parsed(value);
  if (!parsed.isValid()) return false;
  *accent = parsed;
  return true;
}

bool parseHotkey(const QString& value, QKeySequence* sequence) {
  const QKeySequence parsed =
      QKeySequence::fromString(value, QKeySequence::PortableText);
  if (!isSupportedHotkey(parsed)) return false;
  *sequence = parsed;
  return true;
}

}  // namespace

bool isSupportedHotkey(const QKeySequence& sequence) {
  if (sequence.isEmpty() || sequence.count() != 1) return false;

  const int key = sequence[0].key();
  if (key >= Qt::Key_F1 && key <= Qt::Key_F12) return true;

  const bool letter = key >= Qt::Key_A && key <= Qt::Key_Z;
  const bool digit = key >= Qt::Key_0 && key <= Qt::Key_9;
  if (!letter && !digit) return false;

  // A bare letter or digit would swallow ordinary typing system-wide.
  const Qt::KeyboardModifiers modifiers = sequence[0].keyboardModifiers();
  return modifiers.testFlag(Qt::ControlModifier) ||
         modifiers.testFlag(Qt::MetaModifier) ||
         modifiers.testFlag(Qt::AltModifier);
}

SettingsStore::SettingsStore(QString filePath) : filePath_(std::move(filePath)) {}

Settings SettingsStore::parse(const QString& text) {
  Settings settings;

  const auto lines = text.split(QLatin1Char('\n'));
  for (const QString& rawLine : lines) {
    const QString line = trim(rawLine);
    if (line.isEmpty() || line.startsWith(QLatin1Char('#')) ||
        line.startsWith(QLatin1Char('['))) {
      continue;
    }

    const qsizetype equals = line.indexOf(QLatin1Char('='));
    if (equals < 0) continue;
    const QString key = trim(line.left(equals));
    const QString value = trim(line.mid(equals + 1));

    if (key == QLatin1String("accent")) {
      QColor accent;
      if (parseAccent(value, &accent)) settings.accent = accent;
    } else if (key == QLatin1String("hotkey_shot")) {
      QKeySequence sequence;
      if (parseHotkey(value, &sequence)) settings.hotkeyShot = sequence;
    } else if (key == QLatin1String("hotkey_pin")) {
      QKeySequence sequence;
      if (parseHotkey(value, &sequence)) settings.hotkeyPin = sequence;
    } else if (key == QLatin1String("dim_opacity")) {
      bool ok = false;
      const int percent = value.toInt(&ok);
      if (ok && percent >= kDimMin && percent <= kDimMax) {
        settings.dimPercent = percent;
      }
    }
  }

  return settings;
}

QString SettingsStore::serialize(const Settings& settings) {
  QString text;
  text += QStringLiteral(
      "# StickySnap configuration. Restart the app after editing.\n\n");
  text += QStringLiteral(
      "# Accent color for the toolbar and annotation UI (#RRGGBB)\n");
  text += QStringLiteral("accent=%1\n\n")
              .arg(settings.accent.name(QColor::HexRgb));
  text += QStringLiteral(
      "# Global hotkeys (F1-F12, or a letter/digit with a modifier)\n");
  text += QStringLiteral("hotkey_shot=%1\n")
              .arg(settings.hotkeyShot.toString(QKeySequence::PortableText));
  text += QStringLiteral("hotkey_pin=%1\n\n")
              .arg(settings.hotkeyPin.toString(QKeySequence::PortableText));
  text += QStringLiteral("# Dimming of the frozen screen, percent (10-90)\n");
  text += QStringLiteral("dim_opacity=%1\n").arg(settings.dimPercent);
  return text;
}

Settings SettingsStore::load(QString* error) const {
  QFile file(filePath_);
  if (!file.exists()) {
    if (error) error->clear();
    return Settings{};
  }
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error) *error = file.errorString();
    return Settings{};
  }
  if (error) error->clear();
  return parse(QString::fromUtf8(file.readAll()));
}

bool SettingsStore::save(const Settings& settings, QString* error) const {
  const QFileInfo info(filePath_);
  if (!QDir().mkpath(info.absolutePath())) {
    if (error) *error = QStringLiteral("cannot create directory: %1")
                            .arg(info.absolutePath());
    return false;
  }

  QSaveFile file(filePath_);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (error) *error = file.errorString();
    return false;
  }

  const QByteArray data = serialize(settings).toUtf8();
  if (file.write(data) != data.size()) {
    if (error) *error = file.errorString();
    file.cancelWriting();
    return false;
  }
  if (!file.commit()) {
    if (error) *error = file.errorString();
    return false;
  }
  return true;
}

}  // namespace sticky::core
