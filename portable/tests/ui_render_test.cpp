#include <QApplication>
#include <QTemporaryDir>
#include <QtTest>

#include "core/session_store.h"
#include "ui/overlay_window.h"
#include "ui/pin_window.h"
#include "ui/toolbar_widget.h"
#include "ui/tray_icon.h"

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

bool isAnnotationBlue(const QColor& color) {
  return color.blue() > 180 && color.red() < 120 && color.green() < 170;
}

QImage renderToolOnCapture(int tool, const QImage& base, qreal dpr = 1.0,
                           const QPoint& start = QPoint(150, 150),
                           const QPoint& move = QPoint(250, 200),
                           const QPoint& end = QPoint(400, 350)) {
  platform::DisplayCapture capture;
  capture.geometry = QRect(0, 0, 900, 600);
  capture.dpr = dpr;
  capture.image = base;

  QImage committed;
  ui::OverlayWindow overlay(
      capture, core::Settings{},
      [&](const QImage& image, const QPoint&) { committed = image; },
      [] {});

  overlay.show();
  QTest::qWait(10);

  QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
  QTest::mouseMove(&overlay, QPoint(500, 400));
  QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier,
                      QPoint(500, 400));
  QTest::qWait(10);

  auto* toolbar = overlay.findChild<ui::ToolbarWidget*>();
  if (!toolbar) return {};
  QTest::mouseClick(toolbar, Qt::LeftButton, Qt::NoModifier,
                    QPoint(qRound(18.5 + 29.0 * tool), 19));
  QTest::qWait(5);

  QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, start);
  QTest::mouseMove(&overlay, move);
  QTest::mouseMove(&overlay, end);
  QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier, end);
  QTest::qWait(10);

  QTest::keyClick(&overlay, Qt::Key_Return);
  QTest::qWait(10);
  return committed;
}

QImage renderShapeTool(int tool) {
  QImage base(900, 600, QImage::Format_ARGB32_Premultiplied);
  base.fill(Qt::black);
  return renderToolOnCapture(tool, base);
}

}  // namespace

class UiRenderTest : public QObject {
  Q_OBJECT

 private slots:
  void overlaySelectionShowsToolbarAndCommits();
  void trayIconRendersMark();
  void pinWindowRendersAtLogicalSize();
  void toolbarUsesLandingPageLayout();
  void toolbarHitTargetsFollowLandingPageLayout();
  void rectEndsAtReleasePoint();
  void ellipseEndsAtReleasePoint();
  void arrowEndsAtReleasePoint();
  void penFollowsEveryPoint();
  void highlighterFollowsEveryPoint();
  void mosaicPixelatesAlongStroke();
  void penKeepsLogicalThicknessOnRetina();
  void mosaicKeepsLogicalBlockSizeOnRetina();
  void previewStaysInsideSelection();
  void mosaicPreviewsPixelsWhileDragging();
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

void UiRenderTest::toolbarUsesLandingPageLayout() {
  ui::ToolbarWidget toolbar;
  QCOMPARE(toolbar.size(), QSize(351, 79));

  const QPixmap shot = toolbar.grab();
  QVERIFY(!shot.isNull());
  QVERIFY(shot.save(QStringLiteral("/tmp/stickysnap-toolbar.png")));
}

void UiRenderTest::toolbarHitTargetsFollowLandingPageLayout() {
  ui::ToolbarWidget toolbar;
  toolbar.show();
  QTest::qWait(10);

  QSignalSpy toolSpy(&toolbar, &ui::ToolbarWidget::toolChanged);
  QSignalSpy colorSpy(&toolbar, &ui::ToolbarWidget::colorChanged);
  QSignalSpy widthSpy(&toolbar, &ui::ToolbarWidget::widthChanged);

  QTest::mouseClick(&toolbar, Qt::LeftButton, Qt::NoModifier, QPoint(19, 19));
  QCOMPARE(toolbar.activeTool(), 0);
  QCOMPARE(toolSpy.count(), 1);

  QTest::mouseClick(&toolbar, Qt::LeftButton, Qt::NoModifier, QPoint(15, 61));
  QCOMPARE(toolbar.colorIndex(), 0);
  QCOMPARE(colorSpy.count(), 1);

  QTest::mouseClick(&toolbar, Qt::LeftButton, Qt::NoModifier, QPoint(219, 61));
  QCOMPARE(toolbar.widthIndex(), 0);
  QCOMPARE(widthSpy.count(), 1);

  toolbar.close();
}

void UiRenderTest::rectEndsAtReleasePoint() {
  const QImage committed = renderShapeTool(0);
  QVERIFY(!committed.isNull());
  QCOMPARE(committed.size(), QSize(400, 300));
  QVERIFY(committed.save(QStringLiteral("/tmp/stickysnap-rect.png")));
  QVERIFY2(isAnnotationBlue(committed.pixelColor(300, 150)),
           "rect right edge should be at the release point");
}

void UiRenderTest::ellipseEndsAtReleasePoint() {
  const QImage committed = renderShapeTool(1);
  QVERIFY(!committed.isNull());
  QCOMPARE(committed.size(), QSize(400, 300));
  QVERIFY(committed.save(QStringLiteral("/tmp/stickysnap-ellipse.png")));
  QVERIFY2(isAnnotationBlue(committed.pixelColor(300, 150)),
           "ellipse right edge should be at the release point");
}

void UiRenderTest::arrowEndsAtReleasePoint() {
  const QImage committed = renderShapeTool(2);
  QVERIFY(!committed.isNull());
  QCOMPARE(committed.size(), QSize(400, 300));
  QVERIFY(committed.save(QStringLiteral("/tmp/stickysnap-arrow.png")));
  QVERIFY2(isAnnotationBlue(committed.pixelColor(175, 150)),
           "arrow shaft should reach the release point");
}

void UiRenderTest::penFollowsEveryPoint() {
  const QImage committed = renderShapeTool(3);
  QVERIFY(!committed.isNull());
  QCOMPARE(committed.size(), QSize(400, 300));
  QVERIFY(committed.save(QStringLiteral("/tmp/stickysnap-pen.png")));
  QVERIFY2(isAnnotationBlue(committed.pixelColor(150, 100)),
           "pen should pass through every drag point");
  QVERIFY2(isAnnotationBlue(committed.pixelColor(225, 175)),
           "pen should reach the release point");
}

void UiRenderTest::highlighterFollowsEveryPoint() {
  const QImage committed = renderShapeTool(4);
  QVERIFY(!committed.isNull());
  QCOMPARE(committed.size(), QSize(400, 300));
  QVERIFY(committed.save(QStringLiteral("/tmp/stickysnap-highlighter.png")));
  const QColor color = committed.pixelColor(150, 100);
  QVERIFY2(color.blue() > 60 && color.red() < 50,
           "highlighter should pass through every drag point");
  const QColor end = committed.pixelColor(225, 175);
  QVERIFY2(end.blue() > 60 && end.red() < 50,
           "highlighter should reach the release point");
}

void UiRenderTest::mosaicPixelatesAlongStroke() {
  const platform::DisplayCapture capture = makeCapture(900, 600);
  const QImage committed = renderToolOnCapture(5, capture.image);
  QVERIFY(!committed.isNull());
  QCOMPARE(committed.size(), QSize(400, 300));
  QVERIFY(committed.save(QStringLiteral("/tmp/stickysnap-mosaic.png")));

  const QColor blockA = committed.pixelColor(151, 101);
  const QColor blockB = committed.pixelColor(152, 102);
  QCOMPARE(blockA, blockB);

  const QColor untouched = committed.pixelColor(10, 280);
  QCOMPARE(untouched, capture.image.pixelColor(110, 380));
}

void UiRenderTest::penKeepsLogicalThicknessOnRetina() {
  QImage base(1800, 1200, QImage::Format_ARGB32_Premultiplied);
  base.fill(Qt::black);
  const QImage committed =
      renderToolOnCapture(3, base, 2.0, QPoint(150, 200), QPoint(450, 200),
                          QPoint(450, 200));
  QVERIFY(!committed.isNull());
  QCOMPARE(committed.size(), QSize(800, 600));
  QVERIFY(committed.save(QStringLiteral("/tmp/stickysnap-pen-retina.png")));

  int thickness = 0;
  for (int y = 150; y < 250; ++y) {
    if (isAnnotationBlue(committed.pixelColor(400, y))) ++thickness;
  }
  QVERIFY2(thickness >= 7 && thickness <= 9,
           qPrintable(QStringLiteral("4 logical px at 2x should be ~8 image px, "
                                     "got %1")
                          .arg(thickness)));
}

void UiRenderTest::mosaicKeepsLogicalBlockSizeOnRetina() {
  QImage base(1800, 1200, QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < base.height(); ++y) {
    for (int x = 0; x < base.width(); ++x) {
      base.setPixelColor(x, y, QColor((x * 10) % 256, (y * 10) % 256, 64));
    }
  }

  const QImage committed =
      renderToolOnCapture(5, base, 2.0, QPoint(150, 300), QPoint(450, 300),
                          QPoint(450, 300));
  QVERIFY(!committed.isNull());
  QCOMPARE(committed.size(), QSize(800, 600));
  QVERIFY(committed.save(QStringLiteral("/tmp/stickysnap-mosaic-retina.png")));

  const QColor blockA = committed.pixelColor(105, 400);
  const QColor blockB = committed.pixelColor(115, 400);
  QCOMPARE(blockA, blockB);
}

void UiRenderTest::previewStaysInsideSelection() {
  platform::DisplayCapture capture;
  capture.geometry = QRect(0, 0, 900, 600);
  capture.dpr = 1.0;
  capture.image = QImage(900, 600, QImage::Format_ARGB32_Premultiplied);
  capture.image.fill(Qt::black);

  ui::OverlayWindow overlay(
      capture, core::Settings{}, [](const QImage&, const QPoint&) {}, [] {});
  overlay.show();
  QTest::qWait(10);

  QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
  QTest::mouseMove(&overlay, QPoint(500, 400));
  QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier,
                      QPoint(500, 400));
  QTest::qWait(10);

  auto* toolbar = overlay.findChild<ui::ToolbarWidget*>();
  QVERIFY(toolbar != nullptr);
  QTest::mouseClick(toolbar, Qt::LeftButton, Qt::NoModifier, QPoint(106, 19));
  QTest::qWait(5);

  QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(400, 350));
  QTest::mouseMove(&overlay, QPoint(700, 200));
  QTest::qWait(5);

  const QPixmap shot = overlay.grab();
  QVERIFY(!shot.isNull());
  QVERIFY(shot.save(QStringLiteral("/tmp/stickysnap-preview-clip.png")));
  QVERIFY2(!isAnnotationBlue(shot.toImage().pixelColor(600, 250)),
           "live annotation preview must stay inside the selection");

  QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier,
                      QPoint(700, 200));
}

void UiRenderTest::mosaicPreviewsPixelsWhileDragging() {
  ui::OverlayWindow overlay(
      makeCapture(900, 600), core::Settings{}, [](const QImage&, const QPoint&) {},
      [] {});
  overlay.show();
  QTest::qWait(10);

  QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
  QTest::mouseMove(&overlay, QPoint(500, 400));
  QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier,
                      QPoint(500, 400));
  QTest::qWait(10);

  auto* toolbar = overlay.findChild<ui::ToolbarWidget*>();
  QVERIFY(toolbar != nullptr);
  QTest::mouseClick(toolbar, Qt::LeftButton, Qt::NoModifier, QPoint(164, 19));
  QTest::qWait(5);

  QTest::mousePress(&overlay, Qt::LeftButton, Qt::NoModifier, QPoint(200, 200));
  QTest::mouseMove(&overlay, QPoint(350, 300));
  QTest::qWait(5);

  const QPixmap shot = overlay.grab();
  QVERIFY(!shot.isNull());
  QVERIFY(shot.save(QStringLiteral("/tmp/stickysnap-mosaic-preview.png")));
  const QImage frame = shot.toImage();
  QCOMPARE(frame.pixelColor(250, 250), frame.pixelColor(255, 255));

  QTest::mouseRelease(&overlay, Qt::LeftButton, Qt::NoModifier,
                      QPoint(350, 300));
}

void UiRenderTest::trayIconRendersMark() {
  const QIcon icon = ui::makeTrayIcon();
  QVERIFY(!icon.isNull());

  const QPixmap pixmap = icon.pixmap(44, 44);
  QVERIFY(!pixmap.isNull());
  QVERIFY(pixmap.save(QStringLiteral("/tmp/stickysnap-tray-icon.png")));

  const QImage image = pixmap.toImage();
  int opaque = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixelColor(x, y).alpha() > 0) ++opaque;
    }
  }
  QVERIFY(opaque > 100);
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
