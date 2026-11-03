#include "core/selection_model.h"

namespace sticky::core {

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
  start_ = {};
  current_ = {};
}

void SelectionModel::setRect(const QRect& rect) {
  dragging_ = false;
  hasSelection_ = !rect.isEmpty();
  start_ = rect.topLeft();
  current_ = rect.topLeft() + QPoint(rect.width(), rect.height());
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
