#include <QtTest>

#include "core/annotation_model.h"

using namespace sticky::core;

namespace {

QImage blackCanvas(int width = 64, int height = 64) {
  QImage image(width, height, QImage::Format_ARGB32);
  image.fill(Qt::black);
  return image;
}

bool isReddish(const QColor& color) {
  return color.red() > 180 && color.green() < 80 && color.blue() < 80;
}

Shape makeShape(ShapeType type, QColor color, int width,
                QVector<QPoint> points) {
  Shape shape;
  shape.type = type;
  shape.color = color;
  shape.width = width;
  shape.points = std::move(points);
  return shape;
}

}  // namespace

class AnnotationModelTest : public QObject {
  Q_OBJECT

 private slots:
  void rectDrawsBorderButNotInterior();
  void undoRemovesLastShape();
  void clearRemovesEverything();
  void penConnectsPoints();
  void markerIsSemiTransparentAndWider();
  void arrowDrawsLineAndHead();
  void mosaicPixelsWithinABlockAreEqual();
};

void AnnotationModelTest::rectDrawsBorderButNotInterior() {
  AnnotationModel model;
  model.addShape(makeShape(ShapeType::Rect, Qt::red, 2,
                           {QPoint(10, 10), QPoint(30, 30)}));

  const QImage out = model.flatten(blackCanvas());

  QVERIFY(isReddish(out.pixelColor(20, 10)));
  QCOMPARE(out.pixelColor(20, 20), QColor(Qt::black));
}

void AnnotationModelTest::undoRemovesLastShape() {
  AnnotationModel model;
  model.addShape(makeShape(ShapeType::Rect, Qt::red, 2,
                           {QPoint(10, 10), QPoint(30, 30)}));
  QVERIFY(model.undo());

  const QImage out = model.flatten(blackCanvas());
  QCOMPARE(out.pixelColor(20, 10), QColor(Qt::black));
  QVERIFY(!model.undo());
}

void AnnotationModelTest::clearRemovesEverything() {
  AnnotationModel model;
  model.addShape(makeShape(ShapeType::Rect, Qt::red, 2,
                           {QPoint(10, 10), QPoint(30, 30)}));
  model.clear();

  QVERIFY(model.empty());
  QCOMPARE(model.shapes().size(), 0);
}

void AnnotationModelTest::penConnectsPoints() {
  AnnotationModel model;
  model.addShape(makeShape(ShapeType::Pen, Qt::red, 2,
                           {QPoint(10, 30), QPoint(40, 30)}));

  const QImage out = model.flatten(blackCanvas());

  QVERIFY(isReddish(out.pixelColor(25, 30)));
  QVERIFY(isReddish(out.pixelColor(10, 30)));
  QVERIFY(isReddish(out.pixelColor(40, 30)));
}

void AnnotationModelTest::markerIsSemiTransparentAndWider() {
  AnnotationModel model;
  model.addShape(makeShape(ShapeType::Marker, Qt::white, 2,
                           {QPoint(20, 30), QPoint(40, 30)}));

  const QImage out = model.flatten(blackCanvas());

  const QColor center = out.pixelColor(30, 30);
  QVERIFY2(center.red() > 90 && center.red() < 130,
           qPrintable(QStringLiteral("marker red=%1").arg(center.red())));
  QCOMPARE(center.alpha(), 255);
  // Marker width is width * 4, so a pixel 3px off the stroke is still painted.
  QVERIFY(out.pixelColor(30, 33).red() > 0);
}

void AnnotationModelTest::arrowDrawsLineAndHead() {
  AnnotationModel model;
  model.addShape(makeShape(ShapeType::Arrow, Qt::red, 2,
                           {QPoint(10, 32), QPoint(50, 32)}));

  const QImage out = model.flatten(blackCanvas());

  QVERIFY(isReddish(out.pixelColor(30, 32)));
  // The head spreads away from the shaft near the tip.
  int headPixels = 0;
  for (int y = 26; y <= 38; ++y) {
    for (int x = 38; x <= 50; ++x) {
      if (isReddish(out.pixelColor(x, y))) ++headPixels;
    }
  }
  QVERIFY(headPixels > 20);
}

void AnnotationModelTest::mosaicPixelsWithinABlockAreEqual() {
  QImage base(64, 64, QImage::Format_ARGB32);
  for (int y = 0; y < base.height(); ++y) {
    for (int x = 0; x < base.width(); ++x) {
      base.setPixelColor(x, y,
                         QColor((x * 7 + y * 3) % 256, (x * 11) % 256,
                                (y * 5) % 256));
    }
  }

  AnnotationModel model;
  model.addShape(
      makeShape(ShapeType::Mosaic, Qt::black, 2, {QPoint(25, 25)}));

  const QImage out = model.flatten(base);
  QVERIFY(!out.isNull());
  QCOMPARE(out.size(), base.size());
  const QColor a = out.pixelColor(22, 22);
  const QColor b = out.pixelColor(27, 27);

  QCOMPARE(a, b);
  QVERIFY(a != base.pixelColor(22, 22));
}

QTEST_MAIN(AnnotationModelTest)
#include "annotation_model_test.moc"
