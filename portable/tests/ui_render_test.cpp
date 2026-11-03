#include <QApplication>
#include <QTemporaryDir>
#include <QtTest>

#include "core/session_store.h"
#include "ui/overlay_window.h"
#include "ui/pin_window.h"
#include "ui/toolbar_widget.h"

using namespace sticky;

namespace {

platform::DisplayCapture makeCapture(int width, int height) {
  platform::DisplayCapture capture;
  capture.geometry = QRect(0, 0, width, height);
  capture.dpr = 1.0;
  capture.image = QImage(width, height, QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      capture.image.setPixelColor(x, y,
                                  QColor((x * 3) % 256, (y * 5) % 256,
                                         ((x + y) * 2) % 256));
    }
  }
  return capture;
}

}  // namespace

class UiRenderTest : public QObject {
  Q_OBJECT

 private slots:
  void overlaySelectionShowsToolbarAndCommits();
  void pinWindowRendersAtLogicalSize();
};

void UiRenderTest::overlaySelectionShowsToolbarAndCommits() {
  core::Settings settings;
  QImage committed;
  QPoint committedAt;
  bool closed = false;

  ui::OverlayWindow overlay(
      makeCapture(900, 600), settings,
      [&](const QImage& image, const QPoint& position) {
        committed = image;
        committedAt = position;
      },
      [&] { closed = true; });

  overlay.show();
  QTest::qWait(30);

  QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
  QTest::mouseMove(&overlay, QPoint(300, 250));
  QTest::mouseMove(&overlay, QPoint(500, 400));
  QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier,
                      QPoint(500, 400));
  QTest::qWait(30);

  auto* toolbar = overlay.findChild<ui::ToolbarWidget*>();
  QVERIFY(toolbar != nullptr);
  QVERIFY(toolbar->isVisible());

  const QPixmap shot = overlay.grab();
  QVERIFY(!shot.isNull());
  QVERIFY(shot.save(QStringLiteral("/tmp/stickysnap-overlay.png")));

  QTest::keyClick(&overlay, Qt::Key_Return);
  QTest::qWait(10);

  QVERIFY(closed);
  QCOMPARE(committed.size(), QSize(400, 300));
  QCOMPARE(committedAt, QPoint(100, 100));
}

void UiRenderTest::pinWindowRendersAtLogicalSize() {
  QTemporaryDir dir;
  QVERIFY(dir.isValid());

  QImage image(200, 100, QImage::Format_ARGB32_Premultiplied);
  image.fill(QColor(0x2F, 0x6F, 0xED));

  core::PinSession state;
  state.imagePath = dir.filePath(QStringLiteral("pin.png"));
  state.position = QPoint(50, 60);
  state.zoom = 1.0;
  state.alpha = 255;
  state.dpr = 1.0;

  ui::PinWindow pin(image, state.imagePath, state);
  pin.show();
  QTest::qWait(30);

  QCOMPARE(pin.size(), QSize(200, 100));
  const QPixmap shot = pin.grab();
  QVERIFY(!shot.isNull());
  QVERIFY(shot.save(QStringLiteral("/tmp/stickysnap-pin.png")));
  pin.close();
}

QTEST_MAIN(UiRenderTest)
#include "ui_render_test.moc"
