#pragma once

#include <QImage>
#include <QObject>
#include <QPoint>
#include <QVector>

#include <functional>

#include "core/settings.h"
#include "platform/platform.h"
#include "ui/overlay_window.h"

namespace sticky::ui {

// Owns one OverlayWindow per captured display and closes them as a group.
class OverlaySession : public QObject {
  Q_OBJECT

 public:
  using CommitFn = std::function<void(const QImage&, const QPoint&)>;
  using FinishedFn = std::function<void()>;

  OverlaySession(const QVector<platform::DisplayCapture>& captures,
                 const core::Settings& settings, CommitFn onCommit,
                 FinishedFn onFinished, QObject* parent = nullptr);
  ~OverlaySession() override;

  void closeAll();

 private:
  void finish();

  QVector<OverlayWindow*> overlays_;
  CommitFn onCommit_;
  FinishedFn onFinished_;
  bool finished_ = false;
};

}  // namespace sticky::ui
