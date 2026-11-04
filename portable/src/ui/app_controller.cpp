#include "ui/app_controller.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QFileInfo>
#include <QLockFile>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSystemTrayIcon>

#include <algorithm>
#include <utility>

#include "core/settings.h"
#include "platform/platform.h"
#include "ui/settings_dialog.h"
#include "ui/tray_icon.h"

namespace sticky::ui {

AppController::AppController(QObject* parent) : QObject(parent) {}

AppController::~AppController() {
  pins_.closeAll();
  if (tray_) {
    tray_->hide();
    tray_->setContextMenu(nullptr);
    delete tray_;
    tray_ = nullptr;
  }
  delete trayMenu_;
  delete instanceLock_;
}

bool AppController::start() {
  instanceLock_ =
      new QLockFile(platform::appDataDirectory() +
                    QStringLiteral("/StickySnap.lock"));
  instanceLock_->setStaleLockTime(0);
  if (!instanceLock_->tryLock(100)) {
    QMessageBox::information(nullptr, QStringLiteral("StickySnap"),
                             tr("StickySnap 已经在运行。"));
    return false;
  }

  loadSettings();
  setupTray();
  pins_.restore();
  registerHotkeys();
  return true;
}

void AppController::loadSettings() {
  const QString path = platform::settingsPath();
  core::SettingsStore store(path);
  if (!QFileInfo::exists(path)) {
    settings_ = core::Settings{};
    QString error;
    if (!store.save(settings_, &error)) {
      qWarning("StickySnap: cannot write default settings: %s",
               qPrintable(error));
    }
    return;
  }

  QString error;
  settings_ = store.load(&error);
  if (!error.isEmpty()) {
    qWarning("StickySnap: settings error: %s", qPrintable(error));
  }
}

void AppController::setupTray() {
  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    qWarning("StickySnap: no system tray available");
    return;
  }

  trayMenu_ = new QMenu();
  captureAction_ = trayMenu_->addAction(QString());
  pasteAction_ = trayMenu_->addAction(QString());
  trayMenu_->addSeparator();
  QAction* settings = trayMenu_->addAction(tr("设置…"));
  trayMenu_->addSeparator();
  QAction* quitAction = trayMenu_->addAction(tr("退出"));

  connect(captureAction_, &QAction::triggered, this,
          &AppController::startCapture);
  connect(pasteAction_, &QAction::triggered, this,
          &AppController::pinClipboard);
  connect(settings, &QAction::triggered, this, &AppController::showSettings);
  connect(quitAction, &QAction::triggered, this, &AppController::quit);
  updateTrayTexts();

  tray_ = new QSystemTrayIcon(makeTrayIcon(), this);
  tray_->setToolTip(QStringLiteral("StickySnap"));
  tray_->setContextMenu(trayMenu_);
  tray_->show();
}

void AppController::updateTrayTexts() {
  const auto label = [](const QKeySequence& sequence) {
    return sequence.toString(QKeySequence::NativeText);
  };
  if (captureAction_) {
    captureAction_->setText(tr("截图  (%1)").arg(label(settings_.hotkeyShot)));
  }
  if (pasteAction_) {
    pasteAction_->setText(tr("贴剪贴板  (%1)").arg(label(settings_.hotkeyPin)));
  }
}

bool AppController::registerHotkeys(QStringList* errors) {
  bool ok = true;
  const auto add = [&](const QKeySequence& sequence,
                       platform::GlobalHotkeys::Callback callback) {
    QString error;
    if (!hotkeys_.add(sequence, std::move(callback), &error)) {
      ok = false;
      qWarning("StickySnap: %s", qPrintable(error));
      if (errors) {
        errors->append(error);
      } else {
        showMessage(error);
      }
    }
  };

  add(settings_.hotkeyShot, [this] { startCapture(); });
  add(settings_.hotkeyPin, [this] { pinClipboard(); });
  return ok;
}

bool AppController::applySettings(const core::Settings& settings) {
  const core::Settings previous = settings_;

  hotkeys_.clear();
  settings_ = settings;

  QStringList errors;
  if (!registerHotkeys(&errors)) {
    settings_ = previous;
    hotkeys_.clear();
    registerHotkeys(nullptr);

    platform::activateApplication();
    QMessageBox box(QMessageBox::Warning, QStringLiteral("StickySnap"),
                    tr("无法注册快捷键：\n%1\n\n已保留原设置。")
                        .arg(errors.join(QLatin1Char('\n'))));
    box.addButton(QMessageBox::Ok);
    box.show();
    box.exec();
    return false;
  }

  QString error;
  core::SettingsStore store(platform::settingsPath());
  if (!store.save(settings_, &error)) {
    showMessage(tr("设置已生效，但保存失败：%1").arg(error));
  }
  updateTrayTexts();
  return true;
}

void AppController::startCapture() {
  if (session_ || settingsDialog_) return;

  qInfo("StickySnap: capture requested");
  if (!platform::ensureScreenCapturePermission()) {
    qWarning("StickySnap: screen recording permission missing");
    platform::activateApplication();
    QMessageBox box(QMessageBox::Warning, QStringLiteral("StickySnap"),
                    tr("StickySnap 需要「屏幕录制」权限才能截图。\n"
                       "请在系统设置中勾选 StickySnap，然后重新启动应用。"));
    QAbstractButton* open = box.addButton(tr("打开系统设置"),
                                          QMessageBox::AcceptRole);
    box.addButton(tr("取消"), QMessageBox::RejectRole);
    // An accessory app has no main window; showing the box first makes Cocoa
    // create the native window before exec() enters its modal loop.
    box.show();
    box.exec();
    if (box.clickedButton() == open) platform::openScreenRecordingSettings();
    return;
  }

  QString error;
  const QVector<platform::DisplayCapture> captures =
      platform::captureDisplays(&error);
  qInfo("StickySnap: captured %lld displays", static_cast<long long>(captures.size()));
  if (captures.isEmpty()) {
    platform::activateApplication();
    QMessageBox box(QMessageBox::Warning, QStringLiteral("StickySnap"),
                    error.isEmpty() ? tr("截图失败。") : error);
    QAbstractButton* open = box.addButton(tr("打开屏幕录制设置"),
                                          QMessageBox::AcceptRole);
    box.addButton(tr("取消"), QMessageBox::RejectRole);
    box.show();
    box.exec();
    if (box.clickedButton() == open) platform::openScreenRecordingSettings();
    return;
  }

  pins_.setOverlayActive(true);
  session_ = new OverlaySession(
      captures, settings_,
      [this](const QImage& image, const QPoint& position) {
        pins_.createPin(image, position);
      },
      [this] {
        pins_.setOverlayActive(false);
        session_ = nullptr;
      },
      this);
}

void AppController::pinClipboard() {
  if (session_ || settingsDialog_) return;

  const QImage image = QApplication::clipboard()->image();
  if (image.isNull()) {
    showMessage(tr("剪贴板里没有图片。"));
    return;
  }

  pins_.createPin(image, QCursor::pos() + QPoint(12, 12));
}

void AppController::showSettings() {
  if (settingsDialog_ || session_) return;

  // Release the current shortcuts so they can be recorded in the editor.
  hotkeys_.clear();

  platform::activateApplication();
  SettingsDialog dialog(settings_);
  settingsDialog_ = &dialog;
  dialog.show();
  const int result = dialog.exec();
  settingsDialog_ = nullptr;

  if (result == QDialog::Accepted) {
    applySettings(dialog.settings());
  } else {
    registerHotkeys();
  }
}

void AppController::quit() {
  if (session_) session_->closeAll();
  pins_.saveNow();
  QApplication::quit();
}

void AppController::showMessage(const QString& text) {
  if (tray_) {
    tray_->showMessage(QStringLiteral("StickySnap"), text,
                       QSystemTrayIcon::Information, 5000);
  }
}

}  // namespace sticky::ui
