#pragma once

#include <QColor>
#include <QDialog>

#include "core/settings.h"

class QDialogButtonBox;
class QKeySequenceEdit;
class QLabel;
class QPushButton;
class QSlider;

namespace sticky::ui {

// Modal editor for hotkeys, dimming and accent color. Save is disabled while
// either shortcut is unsupported, so callers only see valid settings.
class SettingsDialog : public QDialog {
  Q_OBJECT

 public:
  explicit SettingsDialog(const core::Settings& settings,
                          QWidget* parent = nullptr);

  core::Settings settings() const;

 private:
  void pickAccent();
  void updateAccentButton();
  void updateDimLabel(int value);
  void updateSaveState();

  QKeySequenceEdit* shotEdit_ = nullptr;
  QKeySequenceEdit* pinEdit_ = nullptr;
  QSlider* dimSlider_ = nullptr;
  QLabel* dimValue_ = nullptr;
  QPushButton* accentButton_ = nullptr;
  QDialogButtonBox* buttons_ = nullptr;
  QColor accent_;
};

}  // namespace sticky::ui
