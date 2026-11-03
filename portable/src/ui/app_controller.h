#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>

#include "core/settings.h"
#include "platform/platform.h"
#include "ui/overlay_session.h"
#include "ui/pin_manager.h"

class QLockFile;
class QMenu;
class QSystemTrayIcon;
class QAction;

namespace sticky::ui {

class SettingsDialog;

class AppController : public QObject {
  Q_OBJECT

 public:
  explicit AppController(QObject* parent = nullptr);
  ~AppController() override;

  bool start();

 public slots:
  void startCapture();
  void pinClipboard();

 private:
  void loadSettings();
  void setupTray();
  bool registerHotkeys(QStringList* errors = nullptr);
  bool applySettings(const core::Settings& settings);
  void showSettings();
  void updateTrayTexts();
  void quit();
  void showMessage(const QString& text);

  core::Settings settings_;
  platform::GlobalHotkeys hotkeys_;
  PinManager pins_;
  QSystemTrayIcon* tray_ = nullptr;
  QMenu* trayMenu_ = nullptr;
  QAction* captureAction_ = nullptr;
  QAction* pasteAction_ = nullptr;
  SettingsDialog* settingsDialog_ = nullptr;
  QLockFile* instanceLock_ = nullptr;
  QPointer<OverlaySession> session_;
};

}  // namespace sticky::ui
