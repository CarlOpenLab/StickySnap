#pragma once

#include <QPoint>
#include <QRect>

#include "core/constants.h"

namespace sticky::core {

// Platform-neutral drag state for a screenshot selection. Coordinates are in
// whatever space the caller uses; the overlay passes logical widget pixels.
class SelectionModel {
 public:
  void begin(const QPoint& pos);
  void update(const QPoint& pos);
  void finish();
  void reset();

  // Selects an existing window rectangle directly (click-to-snap).
  void setRect(const QRect& rect);

  bool isDragging() const { return dragging_; }
  bool hasSelection() const { return hasSelection_; }
  QRect rect() const;
  bool isValid(int minPx = kMinSelectionPx) const;

 private:
  bool dragging_ = false;
  bool hasSelection_ = false;
  QPoint start_;
  QPoint current_;
};

}  // namespace sticky::core
