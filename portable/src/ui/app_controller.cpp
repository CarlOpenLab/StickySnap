#include "ui/app_controller.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QColorDialog>
#include <QCursor>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLockFile>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QStandardPaths>
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
  pins_.setNotifySink([this](const QString& message) { showMessage(message); });
  pins_.restore();
  registerHotkeys();

  // The config file is authoritative at startup, like the Windows native
  // build; system-side login-item changes get overwritten on next launch.
  if (!platform::setAutostart(settings_.autostart)) {
    showMessage(tr("开机自启设置失败。"));
  }
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
  QAction* pasteFileAction = trayMenu_->addAction(tr("贴图片文件…"));
  QAction* pasteTextAction = trayMenu_->addAction(tr("贴文本…"));
  QAction* pasteColorAction = trayMenu_->addAction(tr("贴颜色…"));
  trayMenu_->addSeparator();
  QMenu* groupMenu = trayMenu_->addMenu(tr("贴图管理"));
  QAction* showAllAction = groupMenu->addAction(tr("显示全部"));
  QAction* hideAllAction = groupMenu->addAction(tr("隐藏全部"));
  QAction* closeAllAction = groupMenu->addAction(tr("关闭全部"));
  groupMenu->addSeparator();
  QAction* groupSection = groupMenu->addAction(tr("分组（随贴图右键菜单创建）"));
  groupSection->setEnabled(false);
  connect(groupMenu, &QMenu::aboutToShow, this,
          [this, groupMenu, groupSection, showAllAction, hideAllAction,
           closeAllAction] {
    // 分组项动态重建：删除旧的分组项（保留前四个固定项）。
    const QList<QAction*> actions = groupMenu->actions();
    for (QAction* action : actions) {
      if (action == showAllAction || action == hideAllAction ||
          action == closeAllAction || action == groupSection) {
        continue;
      }
      groupMenu->removeAction(action);
      delete action;
    }
    const QStringList names = pins_.groups();
    groupSection->setVisible(!names.isEmpty());
    for (const QString& name : names) {
      QMenu* one = groupMenu->addMenu(name);
      QAction* showGroup = one->addAction(tr("显示"));
      connect(showGroup, &QAction::triggered, this,
              [this, name] { pins_.setGroupVisible(name, true); });
      QAction* hideGroup = one->addAction(tr("隐藏"));
      connect(hideGroup, &QAction::triggered, this,
              [this, name] { pins_.setGroupVisible(name, false); });
      QAction* closeGroup = one->addAction(tr("关闭"));
      connect(closeGroup, &QAction::triggered, this,
              [this, name] { pins_.closeGroup(name); });
    }
  });
  trayMenu_->addSeparator();
  QAction* settings = trayMenu_->addAction(tr("设置…"));
  QAction* openConfig = trayMenu_->addAction(tr("打开配置文件(C)"));
  trayMenu_->addSeparator();
  QAction* quitAction = trayMenu_->addAction(tr("退出"));

  connect(captureAction_, &QAction::triggered, this,
          &AppController::startCapture);
  connect(pasteAction_, &QAction::triggered, this,
          &AppController::pinClipboard);
  connect(pasteFileAction, &QAction::triggered, this,
          &AppController::pinImageFile);
  connect(pasteTextAction, &QAction::triggered, this,
          &AppController::pinText);
  connect(pasteColorAction, &QAction::triggered, this,
          &AppController::pinColor);
  connect(showAllAction, &QAction::triggered, &pins_, &PinManager::showAllPins);
  connect(hideAllAction, &QAction::triggered, &pins_, &PinManager::hideAllPins);
  connect(closeAllAction, &QAction::triggered, &pins_, &PinManager::closeAll);
  connect(settings, &QAction::triggered, this, &AppController::showSettings);
  connect(openConfig, &QAction::triggered, this, [this] {
    platform::openPath(platform::settingsPath());
  });
  connect(quitAction, &QAction::triggered, this, &AppController::quit);
  updateTrayTexts();

  tray_ = new QSystemTrayIcon(makeTrayIcon(), this);
  tray_->setToolTip(QStringLiteral("StickySnap"));
  tray_->setContextMenu(trayMenu_);
  // 左键单击直接截图，右键弹出菜单（对齐 Windows 原生版）。
  connect(tray_, &QSystemTrayIcon::activated, this,
          [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) startCapture();
          });
  tray_->show();

  connect(&pins_, &PinManager::limitReached, this, [this] {
    showMessage(tr("贴图已达上限（200），请先关闭一些贴图。"));
  });
  connect(&pins_, &PinManager::persistFailed, this, [this] {
    showMessage(tr("贴图图片保存失败，重启后无法恢复该贴图。"));
  });
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
  if (!platform::setAutostart(settings_.autostart)) {
    showMessage(tr("开机自启设置失败。"));
  }
  updateTrayTexts();
  return true;
}

void AppController::startCapture() {
  if (session_ || settingsDialog_) return;

  qInfo("StickySnap: capture requested");
  if (settings_.elementSnap && !platform::ensureUiElementPermission()) {
    showMessage(tr("元素吸附需要「辅助功能」权限，已临时改用窗口吸附。"));
  }
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

  session_ = new OverlaySession(
      captures, settings_,
      [this](const QImage& image, const QPoint& position) {
        pins_.createPin(image, position);
      },
      [this] {
        pins_.setOverlayActive(false);
        session_ = nullptr;
      },
      [this](const QString& message) { showMessage(message); }, this);
  // Raising the pins after the overlay exists keeps them floating above the
  // frozen screen on Windows (macOS uses explicit window levels instead).
  pins_.setOverlayActive(true);
}

void AppController::pinClipboard() {
  if (session_ || settingsDialog_) return;

  // 剪贴板里带动画字节（如 image/gif）时优先做动图贴图。
  const QMimeData* mime = QApplication::clipboard()->mimeData();
  if (mime && mime->hasFormat(QStringLiteral("image/gif"))) {
    const QByteArray bytes = mime->data(QStringLiteral("image/gif"));
    pins_.createAnimatedPin(bytes, QStringLiteral("gif"),
                            QCursor::pos() + QPoint(12, 12));
    return;
  }

  const QImage image = QApplication::clipboard()->image();
  if (image.isNull()) {
    showMessage(tr("剪贴板里没有图片。"));
    return;
  }

  pins_.createPin(image, QCursor::pos() + QPoint(12, 12));
}

void AppController::pinImageFile() {
  if (session_ || settingsDialog_) return;

  const QString path = QFileDialog::getOpenFileName(
      nullptr, tr("选择要贴的图片"),
      QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
      tr("图片 (*.png *.gif *.apng *.webp *.jpg *.jpeg *.bmp);;所有文件 (*)"));
  if (path.isEmpty()) return;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    showMessage(tr("无法读取文件：%1").arg(path));
    return;
  }
  const QByteArray bytes = file.readAll();
  file.close();

  const QString suffix = QFileInfo(path).suffix().toLower();
  static const QStringList animatedSuffixes = {QStringLiteral("gif"),
                                               QStringLiteral("apng"),
                                               QStringLiteral("webp")};
  if (animatedSuffixes.contains(suffix)) {
    if (!pins_.createAnimatedPin(bytes, suffix, QCursor::pos() + QPoint(12, 12))) {
      showMessage(tr("无法播放该动图，已放弃贴图。"));
    }
    return;
  }

  QImage image;
  image.loadFromData(bytes);
  if (image.isNull()) {
    showMessage(tr("不是可识别的图片文件。"));
    return;
  }
  pins_.createPin(image, QCursor::pos() + QPoint(12, 12));
}

void AppController::pinText() {
  if (session_ || settingsDialog_) return;

  const QString text =
      QInputDialog::getMultiLineText(nullptr, tr("贴文本"), tr("文本内容："));
  if (text.trimmed().isEmpty()) return;

  pins_.createTextPin(text, QCursor::pos() + QPoint(12, 12));
}

void AppController::pinColor() {
  if (session_ || settingsDialog_) return;

  const QColor color =
      QColorDialog::getColor(QColor(QStringLiteral("#2F6FED")), nullptr,
                             tr("贴颜色"));
  if (!color.isValid()) return;

  pins_.createColorPin(color, QCursor::pos() + QPoint(12, 12));
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
