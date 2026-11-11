#include "core/selection_model.h"

namespace sticky::core {

namespace {
// Smallest selection side while adjusting so handles stay usable.
constexpr int kMinAdjustPx = 12;
}  // namespace

void SelectionModel::begin(const QPoint& pos) {
  dragging_ = true;
  hasSelection_ = true;
  start_ = pos;
  current_ = pos;
}

void SelectionModel::update(const QPoint& pos) {
  if (!dragging_) return;
  current_ = pos;
}

void SelectionModel::finish() { dragging_ = false; }

void SelectionModel::reset() {
  dragging_ = false;
  hasSelection_ = false;
  adjust_ = Handle::None;
  start_ = {};
  current_ = {};
}

void SelectionModel::setRect(const QRect& rect) {
  dragging_ = false;
  hasSelection_ = !rect.isEmpty();
  start_ = rect.topLeft();
  current_ = rect.topLeft() + QPoint(rect.width(), rect.height());
}

SelectionModel::Handle SelectionModel::handleAt(const QPoint& pos,
                                                int grip) const {
  if (!hasSelection_) return Handle::None;
  const QRect r = rect();
  if (r.width() <= 0 || r.height() <= 0) return Handle::None;
  if (!r.adjusted(-grip, -grip, grip, grip).contains(pos))
    return Handle::None;
  const bool nearL = pos.x() <= r.left() + grip;
  const bool nearR = pos.x() >= r.right() - grip;
  const bool nearT = pos.y() <= r.top() + grip;
  const bool nearB = pos.y() >= r.bottom() - grip;
  if (nearL && nearT) return Handle::NW;
  if (nearR && nearT) return Handle::NE;
  if (nearL && nearB) return Handle::SW;
  if (nearR && nearB) return Handle::SE;
  if (nearT) return Handle::N;
  if (nearB) return Handle::S;
  if (nearL) return Handle::W;
  if (nearR) return Handle::E;
  return r.contains(pos) ? Handle::Move : Handle::None;
}

bool SelectionModel::beginMove(const QPoint& pos) {
  if (!hasSelection_) return false;
  adjust_ = Handle::Move;
  anchor_ = pos;
  orig_ = rect();
  return true;
}

bool SelectionModel::beginResize(Handle handle, const QPoint& pos) {
  if (!hasSelection_ || handle == Handle::None || handle == Handle::Move)
    return false;
  adjust_ = handle;
  anchor_ = pos;
  orig_ = rect();
  return true;
}

void SelectionModel::dragTo(const QPoint& pos, const QRect& bounds) {
  if (adjust_ == Handle::None) return;
  const int dx = pos.x() - anchor_.x();
  const int dy = pos.y() - anchor_.y();
  const bool clamped = !bounds.isNull();
  QRect r = orig_;

  // Per-edge setters: clamp into bounds first, then keep kMinAdjustPx
  // against the fixed opposite edge.
  const auto setLeft = [&](int v) {
    r.setLeft(clamped ? qBound(bounds.left(), v, orig_.right() - kMinAdjustPx)
                      : qMin(v, orig_.right() - kMinAdjustPx));
  };
  const auto setRight = [&](int v) {
    r.setRight(clamped
                   ? qBound(orig_.left() + kMinAdjustPx, v, bounds.right())
                   : qMax(v, orig_.left() + kMinAdjustPx));
  };
  const auto setTop = [&](int v) {
    r.setTop(clamped ? qBound(bounds.top(), v, orig_.bottom() - kMinAdjustPx)
                     : qMin(v, orig_.bottom() - kMinAdjustPx));
  };
  const auto setBottom = [&](int v) {
    r.setBottom(clamped
                    ? qBound(orig_.top() + kMinAdjustPx, v, bounds.bottom())
                    : qMax(v, orig_.top() + kMinAdjustPx));
  };

  switch (adjust_) {
    case Handle::Move: {
      int mx = dx;
      int my = dy;
      if (clamped) {
        mx = qBound(-orig_.left(), mx, bounds.right() + 1 - orig_.right());
        my = qBound(-orig_.top(), my, bounds.bottom() + 1 - orig_.bottom());
      }
      r.translate(mx, my);
      break;
    }
    case Handle::NW:
      setLeft(orig_.left() + dx);
      setTop(orig_.top() + dy);
      break;
    case Handle::N:
      setTop(orig_.top() + dy);
      break;
    case Handle::NE:
      setRight(orig_.right() + dx);
      setTop(orig_.top() + dy);
      break;
    case Handle::E:
      setRight(orig_.right() + dx);
      break;
    case Handle::SE:
      setRight(orig_.right() + dx);
      setBottom(orig_.bottom() + dy);
      break;
    case Handle::S:
      setBottom(orig_.bottom() + dy);
      break;
    case Handle::SW:
      setLeft(orig_.left() + dx);
      setBottom(orig_.bottom() + dy);
      break;
    case Handle::W:
      setLeft(orig_.left() + dx);
      break;
    case Handle::None:
      return;
  }

  // Keep rect() (derived from start_/current_) in sync with the adjusted
  // geometry; orig_/anchor_ stay fixed so the drag is absolute, not incremental.
  start_ = r.topLeft();
  current_ = r.topLeft() + QPoint(r.width(), r.height());
}

QRect SelectionModel::rect() const {
  if (!hasSelection_) return {};
  const int left = qMin(start_.x(), current_.x());
  const int top = qMin(start_.y(), current_.y());
  const int width = qAbs(current_.x() - start_.x());
  const int height = qAbs(current_.y() - start_.y());
  return QRect(QPoint(left, top), QSize(width, height));
}

bool SelectionModel::isValid(int minPx) const {
  if (!hasSelection_) return false;
  const QRect r = rect();
  return r.width() >= minPx && r.height() >= minPx;
}

}  // namespace sticky::core
