#include "ui/pin_window.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>

#include "core/constants.h"
#include "platform/platform.h"

namespace sticky::ui {
namespace {

constexpr double kZoomStep = 1.1;
constexpr int kAlphaStep = 16;

}  // namespace

PinWindow::PinWindow(QImage image, QString imagePath,
                     const sticky::core::PinSession& state, QWidget* parent)
    : QWidget(parent),
      image_(std::move(image)),
      imagePath_(std::move(imagePath)),
      dpr_(state.dpr > 0.0 ? state.dpr : image_.devicePixelRatio()) {
  if (dpr_ <= 0.0) dpr_ = 1.0;
  image_.setDevicePixelRatio(dpr_);

  logicalSize_ = QSizeF(image_.width() / dpr_, image_.height() / dpr_);
  zoom_ = std::clamp(state.zoom, core::kZoomMin, core::kZoomMax);
  alpha_ = std::clamp(state.alpha, core::kAlphaMin, core::kAlphaMax);
  passThrough_ = state.passThrough;

  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setFocusPolicy(Qt::NoFocus);

  move(state.position);
  applyZoom(false);
  setAlpha(alpha_);
  setPassThrough(passThrough_);
}

void PinWindow::applyZoom(bool notify) {
  const int width =
      std::max(1, qRound(logicalSize_.width() * zoom_));
  const int height =
      std::max(1, qRound(logicalSize_.height() * zoom_));
  resize(width, height);
  update();
  if (notify) emit changed();
}

void PinWindow::setAlpha(int alpha) {
  alpha_ = std::clamp(alpha, core::kAlphaMin, core::kAlphaMax);
  setWindowOpacity(alpha_ / 255.0);
}

void PinWindow::setPassThrough(bool enabled) {
  passThrough_ = enabled;
  if (!overlayMode_) platform::setClickThrough(this, enabled);
}

void PinWindow::setOverlayMode(bool active) {
  if (overlayMode_ == active) return;
  overlayMode_ = active;
  platform::setPinRaisedAboveOverlay(this, active);
  platform::setClickThrough(this, active ? true : passThrough_);
}

core::PinSession PinWindow::session() const {
  core::PinSession state;
  state.imagePath = imagePath_;
  state.position = pos();
  state.zoom = zoom_;
  state.alpha = alpha_;
  state.passThrough = passThrough_;
  state.dpr = dpr_;
  return state;
}

void PinWindow::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (nativeConfigured_) return;
  nativeConfigured_ = true;
  platform::configurePinWindow(this);
  platform::setClickThrough(this, passThrough_);
}

void PinWindow::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ != 1.0);
  const QRectF target(0, 0, width(), height());
  painter.drawImage(target, image_);
}

void PinWindow::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }
  dragging_ = true;
  dragOffset_ = event->globalPosition().toPoint() - pos();
  raise();
  event->accept();
}

void PinWindow::mouseMoveEvent(QMouseEvent* event) {
  if (!dragging_) {
    QWidget::mouseMoveEvent(event);
    return;
  }
  move(event->globalPosition().toPoint() - dragOffset_);
  event->accept();
}

void PinWindow::mouseReleaseEvent(QMouseEvent* event) {
  if (dragging_ && event->button() == Qt::LeftButton) {
    dragging_ = false;
    emit changed();
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton) {
    close();
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}

void PinWindow::wheelEvent(QWheelEvent* event) {
  const int delta = event->angleDelta().y();
  if (delta == 0) {
    event->ignore();
    return;
  }

  const Qt::KeyboardModifiers modifiers = event->modifiers();
  if (modifiers.testFlag(Qt::ControlModifier) ||
      modifiers.testFlag(Qt::MetaModifier)) {
    setAlpha(alpha_ + (delta > 0 ? kAlphaStep : -kAlphaStep));
  } else {
    zoom_ = std::clamp(delta > 0 ? zoom_ * kZoomStep : zoom_ / kZoomStep,
                       core::kZoomMin, core::kZoomMax);
    applyZoom(false);
  }
  emit changed();
  event->accept();
}

void PinWindow::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);
  QAction* pass = menu.addAction(QObject::tr("鼠标穿透"));
  pass->setCheckable(true);
  pass->setChecked(passThrough_);
  menu.addSeparator();
  QAction* copy = menu.addAction(QObject::tr("复制"));
  QAction* save = menu.addAction(QObject::tr("另存为…"));
  menu.addSeparator();
  QAction* closeAction = menu.addAction(QObject::tr("关闭"));
  closeAction->setShortcut(QKeySequence(Qt::Key_Backspace));

  QAction* chosen = menu.exec(event->globalPos());
  if (chosen == pass) {
    setPassThrough(!passThrough_);
    emit changed();
  } else if (chosen == copy) {
    copyToClipboard();
  } else if (chosen == save) {
    saveAs();
  } else if (chosen == closeAction) {
    close();
  }
}

void PinWindow::closeEvent(QCloseEvent* event) {
  emit closed(this);
  QWidget::closeEvent(event);
}

void PinWindow::copyToClipboard() {
  QApplication::clipboard()->setImage(image_);
}

void PinWindow::saveAs() {
  QString suggested = QFileInfo(imagePath_).fileName();
  if (suggested.isEmpty()) suggested = QStringLiteral("StickySnap.png");

  const QString path = QFileDialog::getSaveFileName(
      this, QObject::tr("另存为"),
      QFileInfo(imagePath_).absolutePath() + QLatin1Char('/') + suggested,
      QObject::tr("PNG 图片 (*.png);;JPEG 图片 (*.jpg);;所有文件 (*)"));
  if (path.isEmpty()) return;

  QString target = path;
  if (QFileInfo(target).suffix().isEmpty()) target += QStringLiteral(".png");
  if (!image_.save(target)) {
    qWarning("StickySnap: failed to save image to %s",
             qPrintable(target));
  }
}

}  // namespace sticky::ui
