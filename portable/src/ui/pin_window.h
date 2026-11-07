#pragma once

#include <QImage>
#include <QPoint>
#include <QSizeF>
#include <QWidget>

#include "core/session_store.h"

class QEnterEvent;

namespace sticky::ui {

// A borderless always-on-top image pinned to the desktop.
class PinWindow : public QWidget {
  Q_OBJECT

 public:
  PinWindow(QImage image, QString imagePath,
            const sticky::core::PinSession& state,
            QWidget* parent = nullptr);

  sticky::core::PinSession session() const;

  void setPassThrough(bool enabled);
  bool passThrough() const { return passThrough_; }

  // During a capture the pinned windows are raised above the frozen overlay
  // and made click-through; this restores their normal state afterwards.
  void setOverlayMode(bool active);

  int alphaValue() const { return alpha_; }
  double zoom() const { return zoom_; }

 signals:
  void changed();
  void closed(PinWindow* self);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  void showEvent(QShowEvent* event) override;

 private:
  void applyZoom(bool notify);
  void setAlpha(int alpha);
  void copyToClipboard();
  void saveAs();

  QImage image_;
  QString imagePath_;
  qreal dpr_ = 1.0;
  QSizeF logicalSize_;
  double zoom_ = 1.0;
  int alpha_ = 255;
  bool passThrough_ = false;
  bool overlayMode_ = false;
  bool nativeConfigured_ = false;
  bool dragging_ = false;
  bool hovered_ = false;
  QPoint dragOffset_;
};

}  // namespace sticky::ui
