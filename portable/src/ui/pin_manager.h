#pragma once

#include <QImage>
#include <QObject>
#include <QPoint>
#include <QTimer>
#include <QVector>

#include "ui/pin_window.h"

namespace sticky::ui {

// Owns every pinned window, persists their state, and restores them at startup.
class PinManager : public QObject {
  Q_OBJECT

 public:
  explicit PinManager(QObject* parent = nullptr);
  ~PinManager() override;

  PinWindow* createPin(const QImage& image, const QPoint& globalPosition);
  void restore();
  void closePin(PinWindow* pin);
  void closeAll();
  void saveNow();

  // Capture overlay: pins stay visible above the frozen image but ignore input.
  void setOverlayActive(bool active);
  bool overlayActive() const { return overlayActive_; }

  const QVector<PinWindow*>& pins() const { return pins_; }

 signals:
  void pinsChanged();

 private:
  void markDirty();
  void addPinWindow(PinWindow* pin);
  QRect virtualDesktop() const;
  QPoint clampPosition(const QPoint& position) const;
  QString newImagePath() const;

  QVector<PinWindow*> pins_;
  QTimer saveTimer_;
  bool overlayActive_ = false;
};

}  // namespace sticky::ui
