#pragma once

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QVector>

class QPainter;

namespace sticky::core {

enum class ShapeType {
  Rect,
  Ellipse,
  Arrow,
  Pen,
  Marker,
  Mosaic,
};

// One annotation stroke in image pixel coordinates.
struct Shape {
  ShapeType type = ShapeType::Rect;
  QColor color = Qt::red;
  int width = 2;
  QVector<QPoint> points;
};

// Ordered annotations over a captured image. Rendering groups mosaic first,
// then highlighter, then solid strokes, matching the M0 tool's layer order.
class AnnotationModel {
 public:
  void addShape(Shape shape);
  bool undo();
  void clear();

  const QVector<Shape>& shapes() const { return shapes_; }
  bool empty() const { return shapes_.isEmpty(); }

  // Returns a copy of base with every annotation composited on top.
  QImage flatten(const QImage& base) const;

 private:
  QVector<Shape> shapes_;
};

// Draws one stroke with the same style flatten() uses (used for live preview).
void paintShape(QPainter& painter, const Shape& shape);

}  // namespace sticky::core
