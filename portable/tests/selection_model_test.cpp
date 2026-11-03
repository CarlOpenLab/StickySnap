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

QTEST_GUILESS_MAIN(SelectionModelTest)
#include "selection_model_test.moc"
