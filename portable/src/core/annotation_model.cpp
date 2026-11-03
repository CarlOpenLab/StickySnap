#include "core/annotation_model.h"

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QPen>
#include <QtMath>

#include <algorithm>

namespace sticky::core {
namespace {

constexpr int kMarkerAlpha = 110;
constexpr int kMosaicBlock = 10;  // Logical points per mosaic block.

qreal shapeScale(const Shape& shape) {
  return shape.scale > 0.0 ? shape.scale : 1.0;
}

qreal brushRadius(const Shape& shape) {
  return (8.0 + shape.width * 3.0) * shapeScale(shape);
}

void drawStroke(QPainter& painter, const Shape& shape) {
  if (shape.points.isEmpty()) return;

  const qreal scale = shapeScale(shape);
  QColor color = shape.color;
  int penWidth = std::max(1, qRound(shape.width * scale));
  if (shape.type == ShapeType::Marker) {
    color.setAlpha(kMarkerAlpha);
    penWidth = std::max(1, qRound(shape.width * 4 * scale));
  }

  painter.setPen(
      QPen(color, penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);

  const QPoint& first = shape.points.first();

  switch (shape.type) {
    case ShapeType::Rect:
      if (shape.points.size() >= 2) {
        painter.drawRect(QRect(first, shape.points.at(1)).normalized());
      }
      break;

    case ShapeType::Ellipse:
      if (shape.points.size() >= 2) {
        painter.drawEllipse(
            QRect(first, shape.points.at(1)).normalized());
      }
      break;

    case ShapeType::Arrow: {
      if (shape.points.size() < 2) break;
      const QPoint& last = shape.points.at(1);
      painter.drawLine(first, last);

      const double angle = std::atan2(static_cast<double>(last.y() - first.y()),
                                     static_cast<double>(last.x() - first.x()));
      const double headLength = (3.0 * shape.width + 4.0) * scale;
      const QPointF head1(
          last.x() - headLength * std::cos(angle - 0.45),
          last.y() - headLength * std::sin(angle - 0.45));
      const QPointF head2(
          last.x() - headLength * std::cos(angle + 0.45),
          last.y() - headLength * std::sin(angle + 0.45));
      painter.drawLine(QPointF(last), head1);
      painter.drawLine(QPointF(last), head2);
      break;
    }

    case ShapeType::Pen:
    case ShapeType::Marker: {
      if (shape.points.size() >= 2) {
        QPainterPath path(first);
        for (qsizetype i = 1; i < shape.points.size(); ++i) {
          path.lineTo(shape.points.at(i));
        }
        painter.drawPath(path);
      } else {
        const qreal radius = penWidth / 2.0;
        painter.drawEllipse(QPointF(first), radius, radius);
      }
      break;
    }

    case ShapeType::Mosaic:
      break;
  }
}

QImage pixelate(const QImage& source, int blockSize) {
  QImage blocky = source.convertToFormat(QImage::Format_ARGB32);
  const int width = blocky.width();
  const int height = blocky.height();
  blockSize = std::max(1, blockSize);

  for (int by = 0; by < height; by += blockSize) {
    const int blockHeight = std::min(blockSize, height - by);
    for (int bx = 0; bx < width; bx += blockSize) {
      const int blockWidth = std::min(blockSize, width - bx);

      qint64 sumR = 0;
      qint64 sumG = 0;
      qint64 sumB = 0;
      qint64 sumA = 0;
      qint64 count = 0;
      for (int y = by; y < by + blockHeight; ++y) {
        const QRgb* line = reinterpret_cast<const QRgb*>(blocky.constScanLine(y));
        for (int x = bx; x < bx + blockWidth; ++x) {
          sumR += qRed(line[x]);
          sumG += qGreen(line[x]);
          sumB += qBlue(line[x]);
          sumA += qAlpha(line[x]);
          ++count;
        }
      }
      if (count == 0) continue;
      const QRgb average = qRgba(static_cast<int>(sumR / count),
                                 static_cast<int>(sumG / count),
                                 static_cast<int>(sumB / count),
                                 static_cast<int>(sumA / count));
      for (int y = by; y < by + blockHeight; ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(blocky.scanLine(y));
        for (int x = bx; x < bx + blockWidth; ++x) line[x] = average;
      }
    }
  }
  return blocky;
}

QPainterPath mosaicMask(const Shape& shape) {
  const qreal radius = brushRadius(shape);

  if (shape.points.size() == 1) {
    QPainterPath dot;
    dot.addEllipse(QPointF(shape.points.first()), radius, radius);
    return dot;
  }

  QPainterPath centerline(shape.points.first());
  for (qsizetype i = 1; i < shape.points.size(); ++i) {
    centerline.lineTo(shape.points.at(i));
  }

  QPainterPathStroker stroker;
  stroker.setWidth(radius * 2.0);
  stroker.setCapStyle(Qt::RoundCap);
  stroker.setJoinStyle(Qt::RoundJoin);
  return stroker.createStroke(centerline);
}

void applyMosaic(QImage& target, const QImage& base, const Shape& shape) {
  if (shape.points.isEmpty()) return;

  const QImage blocky =
      pixelate(base, qRound(kMosaicBlock * shapeScale(shape)));
  QPainter painter(&target);
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.setClipPath(mosaicMask(shape), Qt::ReplaceClip);
  painter.drawImage(0, 0, blocky);
}

}  // namespace

void AnnotationModel::addShape(Shape shape) {
  shapes_.push_back(std::move(shape));
}

bool AnnotationModel::undo() {
  if (shapes_.isEmpty()) return false;
  shapes_.removeLast();
  return true;
}

void AnnotationModel::clear() { shapes_.clear(); }

QImage AnnotationModel::flatten(const QImage& base) const {
  if (base.isNull()) return {};

  QImage result = base.convertToFormat(QImage::Format_ARGB32);

  for (const Shape& shape : shapes_) {
    if (shape.type == ShapeType::Mosaic) applyMosaic(result, base, shape);
  }

  QPainter painter(&result);
  painter.setRenderHint(QPainter::Antialiasing, true);

  for (const Shape& shape : shapes_) {
    if (shape.type == ShapeType::Marker) drawStroke(painter, shape);
  }
  for (const Shape& shape : shapes_) {
    if (shape.type != ShapeType::Mosaic && shape.type != ShapeType::Marker) {
      drawStroke(painter, shape);
    }
  }

  return result;
}

void paintShape(QPainter& painter, const Shape& shape) {
  drawStroke(painter, shape);
}

}  // namespace sticky::core
