#include <QtTest>
#include <QTemporaryDir>

#include "core/constants.h"
#include "core/session_store.h"

using namespace sticky::core;

class SessionStoreTest : public QObject {
  Q_OBJECT

 private slots:
  void serializeRoundTripsAllFields();
  void deserializeClampsZoomAndAlpha();
  void deserializeAcceptsLegacySixFieldLines();
  void deserializeIgnoresMalformedLines();
  void saveWritesAndLoadsIndex();
  void loadMissingFileYieldsEmptyWithError();
};

void SessionStoreTest::serializeRoundTripsAllFields() {
  PinSession pin;
  pin.imagePath = QStringLiteral("/tmp/pins/20261103-101500-001.png");
  pin.position = QPoint(120, -40);
  pin.zoom = 1.75;
  pin.alpha = 180;
  pin.passThrough = true;
  pin.dpr = 2.0;

  const auto parsed =
      SessionStore::deserialize(SessionStore::serialize({pin}));

  QCOMPARE(parsed.size(), 1);
  QCOMPARE(parsed.at(0).imagePath, pin.imagePath);
  QCOMPARE(parsed.at(0).position, pin.position);
  QCOMPARE(parsed.at(0).zoom, pin.zoom);
  QCOMPARE(parsed.at(0).alpha, pin.alpha);
  QCOMPARE(parsed.at(0).passThrough, pin.passThrough);
  QCOMPARE(parsed.at(0).dpr, pin.dpr);
}

void SessionStoreTest::deserializeClampsZoomAndAlpha() {
  const auto parsed = SessionStore::deserialize(
      QStringLiteral("a.png\t1\t2\t99\t5\t1\n"
                     "b.png\t3\t4\t-2\t999\t0\n"));

  QCOMPARE(parsed.size(), 2);
  QCOMPARE(parsed.at(0).zoom, kZoomMax);
  QCOMPARE(parsed.at(0).alpha, kAlphaMin);
  QCOMPARE(parsed.at(0).passThrough, true);
  QCOMPARE(parsed.at(1).zoom, kZoomMin);
  QCOMPARE(parsed.at(1).alpha, kAlphaMax);
  QCOMPARE(parsed.at(1).passThrough, false);
}

void SessionStoreTest::deserializeAcceptsLegacySixFieldLines() {
  const auto parsed =
      SessionStore::deserialize(QStringLiteral("old.png\t1\t2\t1.5\t200\t1\n"));

  QCOMPARE(parsed.size(), 1);
  QCOMPARE(parsed.at(0).zoom, 1.5);
  QCOMPARE(parsed.at(0).dpr, 1.0);
}

void SessionStoreTest::deserializeIgnoresMalformedLines() {
  const auto parsed = SessionStore::deserialize(
      QStringLiteral("# comment\n"
                     "\n"
                     "missing-fields.png\t1\t2\n"
                     "bad-coords.png\tx\ty\t1\t255\t0\n"
                     "\t1\t2\t1\t255\t0\n"
                     "good.png\t5\t6\t1\t255\t0\n"));

  QCOMPARE(parsed.size(), 1);
  QCOMPARE(parsed.at(0).imagePath, QStringLiteral("good.png"));
  QCOMPARE(parsed.at(0).position, QPoint(5, 6));
}

void SessionStoreTest::saveWritesAndLoadsIndex() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  const QString indexPath = dir.filePath("index.txt");
  SessionStore store(indexPath);

  PinSession pin;
  pin.imagePath = dir.filePath("pins/one.png");
  pin.position = QPoint(-12, 34);
  pin.zoom = 0.5;
  pin.alpha = 200;
  pin.passThrough = false;

  QString error;
  QVERIFY2(store.save({pin}, &error), qPrintable(error));
  QVERIFY(QFile::exists(indexPath));
  QVERIFY(!QFile::exists(indexPath + QStringLiteral(".tmp")));

  const auto loaded = store.load(&error);
  QCOMPARE(loaded.size(), 1);
  QCOMPARE(loaded.at(0).imagePath, pin.imagePath);
  QCOMPARE(loaded.at(0).position, pin.position);
  QCOMPARE(loaded.at(0).zoom, pin.zoom);
  QCOMPARE(loaded.at(0).alpha, pin.alpha);
}

void SessionStoreTest::loadMissingFileYieldsEmptyWithError() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());
  SessionStore store(dir.filePath("does-not-exist/index.txt"));

  QString error;
  const auto loaded = store.load(&error);

  QVERIFY(loaded.isEmpty());
  QVERIFY(!error.isEmpty());
}

QTEST_GUILESS_MAIN(SessionStoreTest)
#include "session_store_test.moc"
