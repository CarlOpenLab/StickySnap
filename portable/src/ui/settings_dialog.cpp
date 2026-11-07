#include "ui/settings_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QSlider>
#include <QVBoxLayout>

namespace sticky::ui {

SettingsDialog::SettingsDialog(const core::Settings& settings, QWidget* parent)
    : QDialog(parent), accent_(settings.accent) {
  setWindowTitle(tr("StickySnap 设置"));
  setModal(true);
  setMinimumWidth(420);

  auto* hint = new QLabel(
      tr("快捷键支持 F1–F12，或搭配 ⌘ / ⌃ / ⌥ 的字母、数字。保存后立即生效。"),
      this);
  hint->setWordWrap(true);

  shotEdit_ = new QKeySequenceEdit(settings.hotkeyShot, this);
  shotEdit_->setObjectName(QStringLiteral("shotEdit"));
  shotEdit_->setMaximumSequenceLength(1);

  pinEdit_ = new QKeySequenceEdit(settings.hotkeyPin, this);
  pinEdit_->setObjectName(QStringLiteral("pinEdit"));
  pinEdit_->setMaximumSequenceLength(1);

  dimSlider_ = new QSlider(Qt::Horizontal, this);
  dimSlider_->setObjectName(QStringLiteral("dimSlider"));
  dimSlider_->setRange(10, 90);
  dimSlider_->setValue(settings.dimPercent);

  dimValue_ = new QLabel(this);
  dimValue_->setMinimumWidth(40);
  dimValue_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  auto* dimRow = new QWidget(this);
  auto* dimLayout = new QHBoxLayout(dimRow);
  dimLayout->setContentsMargins(0, 0, 0, 0);
  dimLayout->addWidget(dimSlider_, 1);
  dimLayout->addWidget(dimValue_);

  accentButton_ = new QPushButton(this);
  accentButton_->setObjectName(QStringLiteral("accentButton"));

  pinBorder_ = new QCheckBox(tr("显示 1px 边框"), this);
  pinBorder_->setObjectName(QStringLiteral("pinBorderCheck"));
  pinBorder_->setChecked(settings.pinBorder);

  auto* form = new QFormLayout;
  form->addRow(tr("截图快捷键"), shotEdit_);
  form->addRow(tr("贴图快捷键"), pinEdit_);
  form->addRow(tr("调暗程度"), dimRow);
  form->addRow(tr("贴图边框"), pinBorder_);
  form->addRow(tr("强调色"), accentButton_);

  buttons_ = new QDialogButtonBox(
      QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
  buttons_->setObjectName(QStringLiteral("buttonBox"));
  if (QPushButton* save = buttons_->button(QDialogButtonBox::Save)) {
    save->setText(tr("保存"));
  }
  if (QPushButton* cancel = buttons_->button(QDialogButtonBox::Cancel)) {
    cancel->setText(tr("取消"));
  }

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(hint);
  layout->addSpacing(6);
  layout->addLayout(form);
  layout->addSpacing(6);
  layout->addWidget(buttons_);

  connect(shotEdit_, &QKeySequenceEdit::keySequenceChanged, this,
          &SettingsDialog::updateSaveState);
  connect(pinEdit_, &QKeySequenceEdit::keySequenceChanged, this,
          &SettingsDialog::updateSaveState);
  connect(dimSlider_, &QSlider::valueChanged, this,
          &SettingsDialog::updateDimLabel);
  connect(accentButton_, &QPushButton::clicked, this,
          &SettingsDialog::pickAccent);
  connect(buttons_, &QDialogButtonBox::accepted, this,
          &SettingsDialog::accept);
  connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);

  updateDimLabel(dimSlider_->value());
  updateAccentButton();
  updateSaveState();
}

core::Settings SettingsDialog::settings() const {
  core::Settings result;
  result.accent = accent_;
  result.hotkeyShot = shotEdit_->keySequence();
  result.hotkeyPin = pinEdit_->keySequence();
  result.dimPercent = dimSlider_->value();
  result.pinBorder = pinBorder_->isChecked();
  return result;
}

void SettingsDialog::pickAccent() {
  const QColor chosen =
      QColorDialog::getColor(accent_, this, tr("选择强调色"));
  if (!chosen.isValid()) return;
  accent_ = chosen;
  updateAccentButton();
}

void SettingsDialog::updateAccentButton() {
  QPixmap swatch(18, 18);
  swatch.fill(accent_);
  accentButton_->setIcon(QIcon(swatch));
  accentButton_->setIconSize(QSize(18, 18));
  accentButton_->setText(accent_.name(QColor::HexRgb).toUpper());
}

void SettingsDialog::updateDimLabel(int value) {
  dimValue_->setText(QStringLiteral("%1%").arg(value));
}

void SettingsDialog::updateSaveState() {
  QPushButton* save = buttons_->button(QDialogButtonBox::Save);
  if (!save) return;
  save->setEnabled(core::isSupportedHotkey(shotEdit_->keySequence()) &&
                   core::isSupportedHotkey(pinEdit_->keySequence()));
}

// 点击保存后立即生效：已存在的贴图无需重建即可重绘。
void SettingsDialog::accept() {
  applyPinAppearance();
  QDialog::accept();
}

void SettingsDialog::applyPinAppearance() {
  core::PinAppearance appearance = core::pinAppearance();
  appearance.border = pinBorder_->isChecked();
  appearance.accent = accent_;
  core::setPinAppearance(appearance);

  // 已打开的贴图窗按新外观重绘，不需要重建窗口。
  for (QWidget* widget : QApplication::topLevelWidgets()) widget->update();
}

}  // namespace sticky::ui
