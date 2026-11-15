#include "core/annotation_model.h"

#include <QJsonArray>
#include <QJsonObject>

#include <optional>

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

namespace {

QString shapeTypeName(ShapeType type) {
  switch (type) {
    case ShapeType::Rect: return QStringLiteral("rect");
    case ShapeType::Ellipse: return QStringLiteral("ellipse");
    case ShapeType::Arrow: return QStringLiteral("arrow");
    case ShapeType::Pen: return QStringLiteral("pen");
    case ShapeType::Marker: return QStringLiteral("marker");
    case ShapeType::Mosaic: return QStringLiteral("mosaic");
  }
  return QStringLiteral("rect");
}

std::optional<ShapeType> shapeTypeFromName(const QString& name) {
  if (name == QLatin1String("rect")) return ShapeType::Rect;
  if (name == QLatin1String("ellipse")) return ShapeType::Ellipse;
  if (name == QLatin1String("arrow")) return ShapeType::Arrow;
  if (name == QLatin1String("pen")) return ShapeType::Pen;
  if (name == QLatin1String("marker")) return ShapeType::Marker;
  if (name == QLatin1String("mosaic")) return ShapeType::Mosaic;
  return std::nullopt;
}

QJsonObject pointToJson(const QPoint& point) {
  QJsonObject object;
  object.insert(QLatin1String("x"), point.x());
  object.insert(QLatin1String("y"), point.y());
  return object;
}

QPoint pointFromJson(const QJsonValue& value) {
  const QJsonObject object = value.toObject();
  return QPoint(object.value(QLatin1String("x")).toInt(),
                object.value(QLatin1String("y")).toInt());
}

}  // namespace

QJsonDocument annotationToJson(const AnnotationModel& model) {
  QJsonArray shapes;
  for (const Shape& shape : model.shapes()) {
    QJsonObject object;
    object.insert(QLatin1String("type"), shapeTypeName(shape.type));
    object.insert(QLatin1String("color"),
                  shape.color.name(QColor::HexArgb));
    object.insert(QLatin1String("width"), shape.width);
    object.insert(QLatin1String("scale"), shape.scale);
    QJsonArray points;
    for (const QPoint& point : shape.points) {
      points.append(pointToJson(point));
    }
    object.insert(QLatin1String("points"), points);
    shapes.append(object);
  }

  QJsonObject root;
  root.insert(QLatin1String("version"), 1);
  root.insert(QLatin1String("shapes"), shapes);
  return QJsonDocument(root);
}

bool annotationFromJson(const QJsonDocument& document, AnnotationModel* model) {
  if (!model || !document.isObject()) return false;
  const QJsonObject root = document.object();
  if (root.value(QLatin1String("version")).toInt() != 1) return false;
  const QJsonValue value = root.value(QLatin1String("shapes"));
  if (!value.isArray()) return false;

  AnnotationModel parsed;
  for (const QJsonValue& entry : value.toArray()) {
    const QJsonObject object = entry.toObject();
    const auto type = shapeTypeFromName(
        object.value(QLatin1String("type")).toString());
    if (!type) return false;

    Shape shape;
    shape.type = *type;
    const QColor color(object.value(QLatin1String("color")).toString());
    if (!color.isValid()) return false;
    shape.color = color;
    const int width = object.value(QLatin1String("width")).toInt();
    if (width <= 0) return false;
    shape.width = width;
    shape.scale = object.value(QLatin1String("scale")).toDouble(1.0);
    if (shape.scale <= 0.0) return false;

    const QJsonValue points = object.value(QLatin1String("points"));
    if (!points.isArray()) return false;
    for (const QJsonValue& point : points.toArray()) {
      shape.points.push_back(pointFromJson(point));
    }
    if (shape.points.isEmpty()) return false;
    parsed.addShape(std::move(shape));
  }

  *model = std::move(parsed);
  return true;
}

}  // namespace sticky::core
