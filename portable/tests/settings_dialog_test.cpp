#include <QDialogButtonBox>
#include <QKeySequenceEdit>
#include <QPushButton>
#include <QSlider>
#include <QtTest>

#include "core/settings.h"
#include "ui/settings_dialog.h"

using namespace sticky;

class SettingsDialogTest : public QObject {
  Q_OBJECT

 private slots:
  void showsCurrentValues();
  void savesEditedHotkeysAndDimming();
  void disablesSaveForUnsupportedHotkeys();
  void rendersDialog();
};

void SettingsDialogTest::showsCurrentValues() {
  core::Settings settings;
  settings.accent = QColor(QStringLiteral("#123ABC"));
  settings.hotkeyShot = QKeySequence(Qt::CTRL | Qt::META | Qt::Key_A);
  settings.hotkeyPin = QKeySequence(Qt::CTRL | Qt::META | Qt::Key_P);
  settings.dimPercent = 70;

  ui::SettingsDialog dialog(settings);

  auto* shot = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("shotEdit"));
  auto* pin = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("pinEdit"));
  auto* dim = dialog.findChild<QSlider*>(QStringLiteral("dimSlider"));
  QVERIFY(shot != nullptr);
  QVERIFY(pin != nullptr);
  QVERIFY(dim != nullptr);

  QCOMPARE(shot->keySequence(), settings.hotkeyShot);
  QCOMPARE(pin->keySequence(), settings.hotkeyPin);
  QCOMPARE(dim->value(), 70);
  QCOMPARE(dialog.settings().accent, settings.accent);
}

void SettingsDialogTest::savesEditedHotkeysAndDimming() {
  const core::Settings original;

  ui::SettingsDialog dialog(original);

  auto* shot = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("shotEdit"));
  auto* pin = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("pinEdit"));
  auto* dim = dialog.findChild<QSlider*>(QStringLiteral("dimSlider"));
  auto* box = dialog.findChild<QDialogButtonBox*>(QStringLiteral("buttonBox"));
  QVERIFY(shot != nullptr);
  QVERIFY(pin != nullptr);
  QVERIFY(dim != nullptr);
  QVERIFY(box != nullptr);

  const QKeySequence shotKey(Qt::CTRL | Qt::META | Qt::Key_S);
  const QKeySequence pinKey(Qt::ALT | Qt::Key_2);
  shot->setKeySequence(shotKey);
  pin->setKeySequence(pinKey);
  dim->setValue(80);

  QPushButton* save = box->button(QDialogButtonBox::Save);
  QVERIFY(save != nullptr);
  QVERIFY(save->isEnabled());
  save->click();

  QCOMPARE(dialog.result(), static_cast<int>(QDialog::Accepted));
  const core::Settings result = dialog.settings();
  QCOMPARE(result.hotkeyShot, shotKey);
  QCOMPARE(result.hotkeyPin, pinKey);
  QCOMPARE(result.dimPercent, 80);
  QCOMPARE(result.accent, original.accent);
}

void SettingsDialogTest::disablesSaveForUnsupportedHotkeys() {
  ui::SettingsDialog dialog(core::Settings{});

  auto* shot = dialog.findChild<QKeySequenceEdit*>(QStringLiteral("shotEdit"));
  auto* box = dialog.findChild<QDialogButtonBox*>(QStringLiteral("buttonBox"));
  QVERIFY(shot != nullptr);
  QVERIFY(box != nullptr);

  QPushButton* save = box->button(QDialogButtonBox::Save);
  QVERIFY(save != nullptr);
  QVERIFY(save->isEnabled());

  shot->setKeySequence(QKeySequence(Qt::Key_Space));
  QVERIFY(!save->isEnabled());

  shot->setKeySequence(QKeySequence(Qt::CTRL | Qt::META | Qt::Key_A));
  QVERIFY(save->isEnabled());
}

void SettingsDialogTest::rendersDialog() {
  ui::SettingsDialog dialog(core::Settings{});
  dialog.show();
  QTest::qWait(30);

  QVERIFY(dialog.isVisible());
  const QPixmap shot = dialog.grab();
  QVERIFY(!shot.isNull());
  QVERIFY(shot.save(QStringLiteral("/tmp/stickysnap-settings.png")));

  dialog.close();
}

QTEST_MAIN(SettingsDialogTest)
#include "settings_dialog_test.moc"
