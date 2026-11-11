#include <QtTest>

#include "core/selection_model.h"

using namespace sticky::core;

class SelectionModelTest : public QObject {
  Q_OBJECT

 private slots:
  void dragProducesNormalizedRect();
  void reverseDragNormalizes();
  void tinyDragIsInvalid();
  void threePixelDragIsValid();
  void updateBeforeBeginIsIgnored();
  void windowRectSetsExactSize();
  void resetClearsSelection();
  void finishKeepsRectButStopsDragging();

  // Border hit-testing and live adjust.
  void handleAtZones();
  void handleAtOutsideIsNone();
  void resizeSEGrowsRect();
  void resizeNWMovesOrigin();
  void resizeKeepsMinSize();
  void resizeClampsToBounds();
  void moveTranslatesRect();
  void moveClampsToBounds();
  void adjustWritesBackToRect();
};

void SelectionModelTest::dragProducesNormalizedRect() {
  SelectionModel selection;
  selection.begin(QPoint(20, 30));
  selection.update(QPoint(100, 100));

  QVERIFY(selection.hasSelection());
  QCOMPARE(selection.rect(), QRect(QPoint(20, 30), QSize(80, 70)));
  QVERIFY(selection.isValid());
}

void SelectionModelTest::reverseDragNormalizes() {
  SelectionModel selection;
  selection.begin(QPoint(100, 100));
  selection.update(QPoint(20, 30));

  QCOMPARE(selection.rect(), QRect(QPoint(20, 30), QSize(80, 70)));
}

void SelectionModelTest::tinyDragIsInvalid() {
  SelectionModel selection;
  selection.begin(QPoint(10, 10));
  selection.update(QPoint(12, 12));

  QVERIFY(selection.hasSelection());
  QVERIFY(!selection.isValid());
}

void SelectionModelTest::threePixelDragIsValid() {
  SelectionModel selection;
  selection.begin(QPoint(10, 10));
  selection.update(QPoint(13, 13));

  QVERIFY(selection.isValid());
}

void SelectionModelTest::updateBeforeBeginIsIgnored() {
  SelectionModel selection;
  selection.update(QPoint(50, 50));

  QVERIFY(!selection.hasSelection());
  QVERIFY(!selection.isDragging());
  QCOMPARE(selection.rect(), QRect());
}

void SelectionModelTest::windowRectSetsExactSize() {
  SelectionModel selection;
  selection.setRect(QRect(QPoint(10, 20), QSize(100, 50)));

  QVERIFY(!selection.isDragging());
  QVERIFY(selection.hasSelection());
  QCOMPARE(selection.rect(), QRect(QPoint(10, 20), QSize(100, 50)));
  QVERIFY(selection.isValid());
}

void SelectionModelTest::resetClearsSelection() {
  SelectionModel selection;
  selection.begin(QPoint(0, 0));
  selection.update(QPoint(10, 10));
  selection.finish();
  selection.reset();

  QVERIFY(!selection.hasSelection());
  QVERIFY(!selection.isDragging());
  QCOMPARE(selection.rect(), QRect());
}

void SelectionModelTest::finishKeepsRectButStopsDragging() {
  SelectionModel selection;
  selection.begin(QPoint(0, 0));
  selection.update(QPoint(10, 10));
  selection.finish();

  QVERIFY(!selection.isDragging());
  QVERIFY(selection.hasSelection());
  QCOMPARE(selection.rect(), QRect(QPoint(0, 0), QSize(10, 10)));
}

namespace {
// Fixed 100x60 selection at (50, 40) for the adjust tests.
SelectionModel adjustedSelection() {
  SelectionModel selection;
  selection.setRect(QRect(QPoint(50, 40), QSize(100, 60)));
  return selection;
}
}  // namespace

void SelectionModelTest::handleAtZones() {
  const SelectionModel selection = adjustedSelection();
  // 100x60 at (50,40): spans x 50..149, y 40..99.
  QCOMPARE(selection.handleAt(QPoint(55, 45)), Handle::NW);   // corner grip
  QCOMPARE(selection.handleAt(QPoint(145, 45)), Handle::NE);
  QCOMPARE(selection.handleAt(QPoint(55, 95)), Handle::SW);
  QCOMPARE(selection.handleAt(QPoint(145, 95)), Handle::SE);
  QCOMPARE(selection.handleAt(QPoint(100, 43)), Handle::N);   // top edge mid
  QCOMPARE(selection.handleAt(QPoint(100, 96)), Handle::S);
  QCOMPARE(selection.handleAt(QPoint(53, 70)), Handle::W);
  QCOMPARE(selection.handleAt(QPoint(146, 70)), Handle::E);
  QCOMPARE(selection.handleAt(QPoint(100, 70)), Handle::Move);  // deep inside
  // Just outside the border but within the 6px grip still hits an edge.
  QCOMPARE(selection.handleAt(QPoint(100, 37)), Handle::N);
  QCOMPARE(selection.handleAt(QPoint(153, 70)), Handle::E);
}

void SelectionModelTest::handleAtOutsideIsNone() {
  const SelectionModel selection = adjustedSelection();
  QCOMPARE(selection.handleAt(QPoint(100, 30)), Handle::None);  // above grip
  QCOMPARE(selection.handleAt(QPoint(30, 70)), Handle::None);
  QCOMPARE(selection.handleAt(QPoint(200, 200)), Handle::None);
}

void SelectionModelTest::resizeSEGrowsRect() {
  SelectionModel selection = adjustedSelection();
  QVERIFY(selection.beginResize(Handle::SE, QPoint(149, 99)));
  QVERIFY(selection.isAdjusting());
  selection.dragTo(QPoint(200, 150));
  // QRect borders are inclusive: right 200 / bottom 150 → 151x111.
  QCOMPARE(selection.rect(), QRect(QPoint(50, 40), QSize(151, 111)));
  selection.endAdjust();
  QVERIFY(!selection.isAdjusting());
  QCOMPARE(selection.rect(), QRect(QPoint(50, 40), QSize(151, 111)));
}

void SelectionModelTest::resizeNWMovesOrigin() {
  SelectionModel selection = adjustedSelection();
  QVERIFY(selection.beginResize(Handle::NW, QPoint(50, 40)));
  selection.dragTo(QPoint(20, 30));
  QCOMPARE(selection.rect(), QRect(QPoint(20, 30), QSize(130, 70)));
}

void SelectionModelTest::resizeKeepsMinSize() {
  SelectionModel selection = adjustedSelection();
  QVERIFY(selection.beginResize(Handle::SE, QPoint(149, 99)));
  selection.dragTo(QPoint(52, 42));  // would collapse to 3x3
  const QRect r = selection.rect();
  QVERIFY(r.width() >= 12);
  QVERIFY(r.height() >= 12);
  QCOMPARE(r.topLeft(), QPoint(50, 40));  // fixed opposite edge stays
}

void SelectionModelTest::resizeClampsToBounds() {
  SelectionModel selection = adjustedSelection();
  const QRect bounds(QPoint(0, 0), QSize(200, 120));
  QVERIFY(selection.beginResize(Handle::SE, QPoint(149, 99)));
  selection.dragTo(QPoint(500, 500), bounds);  // far outside the widget
  QCOMPARE(selection.rect(), QRect(QPoint(50, 40), QSize(150, 80)));
}

void SelectionModelTest::moveTranslatesRect() {
  SelectionModel selection = adjustedSelection();
  QVERIFY(selection.beginMove(QPoint(100, 70)));
  selection.dragTo(QPoint(130, 90));
  QCOMPARE(selection.rect(), QRect(QPoint(80, 60), QSize(100, 60)));
  selection.endAdjust();
  QCOMPARE(selection.rect(), QRect(QPoint(80, 60), QSize(100, 60)));
}

void SelectionModelTest::moveClampsToBounds() {
  SelectionModel selection = adjustedSelection();
  const QRect bounds(QPoint(0, 0), QSize(200, 120));
  QVERIFY(selection.beginMove(QPoint(100, 70)));
  selection.dragTo(QPoint(500, 70), bounds);  // clips flush to the right edge
  QCOMPARE(selection.rect(), QRect(QPoint(101, 40), QSize(100, 60)));
}

void SelectionModelTest::adjustWritesBackToRect() {
  SelectionModel selection = adjustedSelection();
  QVERIFY(selection.beginResize(Handle::E, QPoint(149, 70)));
  selection.dragTo(QPoint(180, 70));
  selection.endAdjust();
  // After endAdjust the model state must reflect the adjusted geometry, so
  // further hit tests see the new border.
  QCOMPARE(selection.handleAt(QPoint(176, 70)), Handle::E);
  QCOMPARE(selection.handleAt(QPoint(100, 70)), Handle::Move);
}

QTEST_GUILESS_MAIN(SelectionModelTest)
#include "selection_model_test.moc"
