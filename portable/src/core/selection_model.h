#pragma once

#include <QPoint>
#include <QRect>

#include "core/constants.h"

namespace sticky::core {

// Selection hit zones for an existing selection: Move = inside the rect,
// the rest are border/corner resize zones (corners win over edges).
enum class Handle {
  None,
  Move,
  NW,
  N,
  NE,
  E,
  SE,
  S,
  SW,
  W,
};

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

  // Border/handle hit-testing and live adjust (move or resize) of an
  // existing selection. dragTo clamps to bounds when it is not null and
  // always keeps the rect at least kMinAdjustPx on each side.
  Handle handleAt(const QPoint& pos, int grip = 6) const;
  bool beginMove(const QPoint& pos);
  bool beginResize(Handle handle, const QPoint& pos);
  void dragTo(const QPoint& pos, const QRect& bounds = QRect());
  void endAdjust() { adjust_ = Handle::None; }
  bool isAdjusting() const { return adjust_ != Handle::None; }

  bool isDragging() const { return dragging_; }
  bool hasSelection() const { return hasSelection_; }
  QRect rect() const;
  bool isValid(int minPx = kMinSelectionPx) const;

 private:
  bool dragging_ = false;
  bool hasSelection_ = false;
  QPoint start_;
  QPoint current_;
  Handle adjust_ = Handle::None;  // active move/resize mode
  QPoint anchor_;                 // press point when the adjust started
  QRect orig_;                    // selection when the adjust started
};

}  // namespace sticky::core
