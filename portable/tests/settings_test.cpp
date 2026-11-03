#include <QTemporaryDir>
#include <QtTest>

#include "core/settings.h"

using namespace sticky::core;

class SettingsTest : public QObject {
  Q_OBJECT

 private slots:
  void missingFileYieldsDefaults();
  void parsesAllKeys();
  void invalidValuesFallBackToDefaults();
  void rejectsBareLetterHotkeys();
  void supportedHotkeyRules();
  void serializeRoundTrips();
  void saveWritesReadableFile();
};

void SettingsTest::missingFileYieldsDefaults() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  SettingsStore store(dir.filePath(QStringLiteral("config.ini")));

  QString error;
  const Settings settings = store.load(&error);

  QCOMPARE(settings.accent, QColor(QStringLiteral("#FF4B3E")));
#ifdef Q_OS_MACOS
  QCOMPARE(settings.hotkeyShot,
           QKeySequence(Qt::CTRL | Qt::META | Qt::Key_A));
#else
  QCOMPARE(settings.hotkeyShot, QKeySequence(Qt::Key_F1));
#endif
  QCOMPARE(settings.hotkeyPin, QKeySequence(Qt::Key_F3));
  QCOMPARE(settings.dimPercent, 30);
  QVERIFY(error.isEmpty());
}

void SettingsTest::supportedHotkeyRules() {
  QVERIFY(isSupportedHotkey(QKeySequence(Qt::Key_F1)));
  QVERIFY(isSupportedHotkey(QKeySequence(Qt::CTRL | Qt::META | Qt::Key_A)));
  QVERIFY(isSupportedHotkey(QKeySequence(Qt::ALT | Qt::Key_1)));
  QVERIFY(!isSupportedHotkey(QKeySequence()));
  QVERIFY(!isSupportedHotkey(QKeySequence(Qt::Key_A)));
  QVERIFY(!isSupportedHotkey(QKeySequence(Qt::SHIFT | Qt::Key_A)));
  QVERIFY(!isSupportedHotkey(QKeySequence(Qt::Key_F13)));
  QVERIFY(!isSupportedHotkey(QKeySequence(Qt::Key_Space)));
  QVERIFY(!isSupportedHotkey(
      QKeySequence(QStringLiteral("Ctrl+A, Ctrl+B"))));
}

void SettingsTest::rejectsBareLetterHotkeys() {
  const Settings defaults;
  const Settings settings = SettingsStore::parse(
      QStringLiteral("hotkey_shot=A\n"
                     "hotkey_pin=Shift+B\n"));

  QCOMPARE(settings.hotkeyShot, defaults.hotkeyShot);
  QCOMPARE(settings.hotkeyPin, defaults.hotkeyPin);
}

void SettingsTest::parsesAllKeys() {
  const Settings settings = SettingsStore::parse(
      QStringLiteral("# comment\n"
                     "accent=#123abc\n"
                     "hotkey_shot=F5\n"
                     "hotkey_pin=F9\n"
                     "dim_opacity=70\n"));

  QCOMPARE(settings.accent, QColor(QStringLiteral("#123ABC")));
  QCOMPARE(settings.hotkeyShot, QKeySequence(Qt::Key_F5));
  QCOMPARE(settings.hotkeyPin, QKeySequence(Qt::Key_F9));
  QCOMPARE(settings.dimPercent, 70);
}

void SettingsTest::invalidValuesFallBackToDefaults() {
  const Settings defaults;
  const Settings settings = SettingsStore::parse(
      QStringLiteral("accent=not-a-color\n"
                     "hotkey_shot=NotAKey\n"
                     "hotkey_pin=\n"
                     "dim_opacity=5\n"));

  QCOMPARE(settings.accent, defaults.accent);
  QCOMPARE(settings.hotkeyShot, defaults.hotkeyShot);
  QCOMPARE(settings.hotkeyPin, defaults.hotkeyPin);
  QCOMPARE(settings.dimPercent, defaults.dimPercent);
}

void SettingsTest::serializeRoundTrips() {
  Settings original;
  original.accent = QColor(QStringLiteral("#123ABC"));
  original.hotkeyShot = QKeySequence(Qt::Key_F7);
  original.hotkeyPin = QKeySequence(Qt::Key_F8);
  original.dimPercent = 80;

  const Settings parsed = SettingsStore::parse(SettingsStore::serialize(original));

  QCOMPARE(parsed.accent, original.accent);
  QCOMPARE(parsed.hotkeyShot, original.hotkeyShot);
  QCOMPARE(parsed.hotkeyPin, original.hotkeyPin);
  QCOMPARE(parsed.dimPercent, original.dimPercent);
}

void SettingsTest::saveWritesReadableFile() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString path = dir.filePath(QStringLiteral("config.ini"));
  SettingsStore store(path);

  Settings settings;
  settings.accent = QColor(QStringLiteral("#00AA55"));
  settings.hotkeyShot = QKeySequence(Qt::Key_F6);
  settings.hotkeyPin = QKeySequence(Qt::Key_F7);
  settings.dimPercent = 65;

  QString error;
  QVERIFY2(store.save(settings, &error), qPrintable(error));
  QVERIFY(QFile::exists(path));

  const Settings loaded = store.load(&error);
  QVERIFY(error.isEmpty());
  QCOMPARE(loaded.accent, settings.accent);
  QCOMPARE(loaded.hotkeyShot, settings.hotkeyShot);
  QCOMPARE(loaded.hotkeyPin, settings.hotkeyPin);
  QCOMPARE(loaded.dimPercent, settings.dimPercent);
}

QTEST_GUILESS_MAIN(SettingsTest)
#include "settings_test.moc"
