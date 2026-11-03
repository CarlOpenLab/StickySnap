#include "ui/overlay_session.h"

#include <QCursor>
#include <QGuiApplication>
#include <QScreen>

namespace sticky::ui {

OverlaySession::OverlaySession(
    const QVector<platform::DisplayCapture>& captures,
    const core::Settings& settings, CommitFn onCommit, FinishedFn onFinished,
    QObject* parent)
    : QObject(parent),
      onCommit_(std::move(onCommit)),
      onFinished_(std::move(onFinished)) {
  QWidget* focusTarget = nullptr;
  const QPoint cursor = QCursor::pos();

  for (const platform::DisplayCapture& capture : captures) {
    auto* overlay = new OverlayWindow(
        capture, settings,
        [this](const QImage& image, const QPoint& position) {
          if (finished_) return;
          if (onCommit_) onCommit_(image, position);
          finish();
        },
        [this] { finish(); });
    overlays_.push_back(overlay);
    overlay->show();
    if (overlay->geometry().contains(cursor)) focusTarget = overlay;
  }

  if (!focusTarget && !overlays_.isEmpty()) focusTarget = overlays_.first();
  if (focusTarget) {
    focusTarget->raise();
    focusTarget->activateWindow();
    focusTarget->setFocus(Qt::ActiveWindowFocusReason);
  }
}

OverlaySession::~OverlaySession() {
  for (OverlayWindow* overlay : overlays_) {
    if (overlay) overlay->deleteLater();
  }
  overlays_.clear();
}

void OverlaySession::closeAll() {
  const QVector<OverlayWindow*> copy = overlays_;
  overlays_.clear();
  for (OverlayWindow* overlay : copy) {
    if (!overlay) continue;
    overlay->close();
    overlay->deleteLater();
  }
}

void OverlaySession::finish() {
  if (finished_) return;
  finished_ = true;
  closeAll();
  if (onFinished_) onFinished_();
  deleteLater();
}

}  // namespace sticky::ui
