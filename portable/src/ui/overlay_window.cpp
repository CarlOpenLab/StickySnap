#include "ui/overlay_window.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

#include "core/annotation_style.h"
#include "ui/toolbar_widget.h"

namespace sticky::ui {
namespace {

constexpr int kMagnifierSamples = 9;
constexpr int kMagnifierScale = 12;
constexpr int kMagnifierSize = kMagnifierSamples * kMagnifierScale;

QColor hoverAccent() { return QColor(0x38, 0xE1, 0xCF); }

}  // namespace

OverlayWindow::OverlayWindow(platform::DisplayCapture capture,
                             const core::Settings& settings,
                             CommitFn onCommit, ClosedFn onClosed,
                             QWidget* parent)
    : QWidget(parent),
      capture_(std::move(capture)),
      settings_(settings),
      onCommit_(std::move(onCommit)),
      onClosed_(std::move(onClosed)) {
  setWindowFlags(Qt::Window | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
  setAttribute(Qt::WA_DeleteOnClose, false);
  setMouseTracking(true);
  setFocusPolicy(Qt::StrongFocus);
  setCursor(Qt::CrossCursor);
  setGeometry(capture_.geometry);

  toolbar_ = new ToolbarWidget(this);
  toolbar_->setAccent(settings_.accent);
  toolbar_->hide();

  connect(toolbar_, &ToolbarWidget::toolChanged, this, [this](int tool) {
    if (tool < 0) {
      drawing_ = false;
      preview_ = {};
    }
    update();
  });
  connect(toolbar_, &ToolbarWidget::colorChanged, this,
          [this](int) { update(); });
  connect(toolbar_, &ToolbarWidget::widthChanged, this,
          [this](int) { update(); });
  connect(toolbar_, &ToolbarWidget::actionTriggered, this, [this](int action) {
    switch (action) {
      case ToolbarWidget::kUndo:
        undoLastShape();
        break;
      case ToolbarWidget::kPin:
        commitSelection();
        break;
      case ToolbarWidget::kCopy:
        copySelection();
        break;
      case ToolbarWidget::kSave:
        saveSelection();
        break;
      case ToolbarWidget::kCancel:
        finishSession();
        break;
    }
  });
}

OverlayWindow::~OverlayWindow() = default;

void OverlayWindow::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (nativeConfigured_) return;
  nativeConfigured_ = true;
  platform::configureOverlayWindow(this);
  activateWindow();
  setFocus(Qt::ActiveWindowFocusReason);
}

void OverlayWindow::closeEvent(QCloseEvent* event) {
  if (!closedNotified_) {
    closedNotified_ = true;
    if (onClosed_) onClosed_();
  }
  QWidget::closeEvent(event);
}

QRect OverlayWindow::selectionImageRect() const {
  const QRect selection = selection_.rect();
  if (selection.isEmpty()) return {};
  const qreal dpr = capture_.dpr > 0 ? capture_.dpr : 1.0;
  const QRect imageRect(qRound(selection.x() * dpr),
                        qRound(selection.y() * dpr),
                        qRound(selection.width() * dpr),
                        qRound(selection.height() * dpr));
  return imageRect.intersected(capture_.image.rect());
}

QPoint OverlayWindow::toImagePoint(const QPoint& widgetPoint) const {
  const qreal dpr = capture_.dpr > 0 ? capture_.dpr : 1.0;
  QPoint point(qRound(widgetPoint.x() * dpr), qRound(widgetPoint.y() * dpr));
  point.setX(std::clamp(point.x(), 0, capture_.image.width() - 1));
  point.setY(std::clamp(point.y(), 0, capture_.image.height() - 1));
  return point;
}

core::Shape OverlayWindow::makeShape(core::ShapeType type,
                                     const QPoint& imagePoint) const {
  core::Shape shape;
  shape.type = type;
  shape.color = core::paletteColor(toolbar_->colorIndex());
  shape.width = core::kLineWidths[std::clamp(toolbar_->widthIndex(), 0, 2)];
  shape.points = {imagePoint};
  return shape;
}

void OverlayWindow::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
  painter.drawImage(rect(), capture_.image);

  const int dimAlpha = std::clamp(settings_.dimPercent, 10, 90) * 255 / 100;
  painter.fillRect(rect(), QColor(0, 0, 0, dimAlpha));

  const QRect sel = selection_.rect();
  if (selection_.hasSelection() && selection_.isValid()) {
    if (annotations_.empty()) {
      const QRect source = selectionImageRect();
      if (!source.isEmpty()) {
        painter.drawImage(sel, capture_.image, source);
      }
    } else {
      if (cacheDirty_) {
        // paintEvent is non-const; the cache is rebuilt here to keep the
        // committed strokes pixel-accurate (mosaic especially).
        ensureAnnotationCache();
      }
      if (!annotatedCache_.isNull()) {
        painter.drawImage(sel, annotatedCache_);
      }
    }

    drawPreview(painter);

    painter.setPen(QPen(settings_.accent, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(sel.adjusted(0, 0, -1, -1));
    drawSelectionLabel(painter);
  } else {
    drawHover(painter);
  }

  const bool overToolbar =
      toolbar_->isVisible() && toolbar_->geometry().contains(cursor_);
  if (cursorValid_ && !overToolbar) drawMagnifier(painter);
  if (!selection_.hasSelection() && !pressing_) drawHint(painter);
}

void OverlayWindow::drawHover(QPainter& painter) {
  if (hoverRect_.isEmpty()) return;
  painter.setPen(QPen(hoverAccent(), 2));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(hoverRect_.adjusted(0, 0, -1, -1));
}

void OverlayWindow::drawPreview(QPainter& painter) {
  if (!drawing_ || preview_.points.isEmpty()) return;
  const qreal dpr = capture_.dpr > 0 ? capture_.dpr : 1.0;

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(selection_.rect().topLeft());
  painter.scale(1.0 / dpr, 1.0 / dpr);

  if (preview_.type == core::ShapeType::Mosaic) {
    painter.setPen(QPen(QColor(150, 150, 150), 1));
    painter.setBrush(Qt::NoBrush);
    const int radius = 8 + preview_.width * 3;
    for (const QPoint& point : preview_.points) {
      painter.drawEllipse(point, radius, radius);
    }
  } else {
    core::paintShape(painter, preview_);
  }

  painter.restore();
}

void OverlayWindow::drawMagnifier(QPainter& painter) {
  const QPoint imagePoint = toImagePoint(cursor_);
  if (!capture_.image.rect().contains(imagePoint)) return;

  QImage patch(kMagnifierSamples, kMagnifierSamples,
               QImage::Format_ARGB32_Premultiplied);
  for (int y = 0; y < kMagnifierSamples; ++y) {
    for (int x = 0; x < kMagnifierSamples; ++x) {
      QPoint sample(imagePoint.x() + x - kMagnifierSamples / 2,
                    imagePoint.y() + y - kMagnifierSamples / 2);
      sample.setX(std::clamp(sample.x(), 0, capture_.image.width() - 1));
      sample.setY(std::clamp(sample.y(), 0, capture_.image.height() - 1));
      patch.setPixelColor(x, y, capture_.image.pixelColor(sample));
    }
  }
  const QImage enlarged = patch.scaled(kMagnifierSize, kMagnifierSize,
                                       Qt::IgnoreAspectRatio,
                                       Qt::FastTransformation);

  int x = cursor_.x() + 16;
  int y = cursor_.y() + 16;
  if (x + kMagnifierSize > width()) x = cursor_.x() - 16 - kMagnifierSize;
  if (y + kMagnifierSize > height()) y = cursor_.y() - 16 - kMagnifierSize;
  x = std::clamp(x, 0, std::max(0, width() - kMagnifierSize));
  y = std::clamp(y, 0, std::max(0, height() - kMagnifierSize));

  painter.drawImage(x, y, enlarged);
  painter.setPen(QPen(Qt::white, 1));
  painter.setBrush(Qt::NoBrush);
  painter.drawRect(x, y, kMagnifierSize - 1, kMagnifierSize - 1);

  const int midX = x + kMagnifierSize / 2;
  const int midY = y + kMagnifierSize / 2;
  painter.setPen(QPen(QColor(0, 0, 0, 180), 2));
  painter.drawLine(midX, y, midX, y + kMagnifierSize);
  painter.drawLine(x, midY, x + kMagnifierSize, midY);
  painter.setPen(QPen(Qt::white, 1));
  painter.drawLine(midX, y, midX, y + kMagnifierSize);
  painter.drawLine(x, midY, x + kMagnifierSize, midY);

  const QColor color = capture_.image.pixelColor(imagePoint);
  const QString hex =
      QStringLiteral("#%1")
          .arg(QString::number(color.rgb() & 0xFFFFFF, 16)
                   .toUpper()
                   .rightJustified(6, QLatin1Char('0')));

  const QPoint labelPos(x, y + kMagnifierSize + 6);
  if (labelPos.y() < height() - 24) {
    drawLabel(painter, labelPos, hex, QColor(0, 0, 0, 200), Qt::white, 10);
  } else {
    drawLabel(painter, QPoint(x, y - 24), hex, QColor(0, 0, 0, 200),
              Qt::white, 10);
  }
}

void OverlayWindow::drawSelectionLabel(QPainter& painter) {
  const QRect sel = selection_.rect();
  const qreal dpr = capture_.dpr > 0 ? capture_.dpr : 1.0;
  const QString text = QStringLiteral("%1 × %2")
                           .arg(qRound(sel.width() * dpr))
                           .arg(qRound(sel.height() * dpr));

  QPoint topLeft(sel.left(), sel.top() - 26);
  if (topLeft.y() < 4) topLeft = QPoint(sel.left() + 4, sel.top() + 4);
  drawLabel(painter, topLeft, text, QColor(0, 0, 0, 190), Qt::white, 11);
}

void OverlayWindow::drawHint(QPainter& painter) {
  const QString text = tr("拖拽选择区域 · 单击选择窗口 · Esc 取消");
  QFont font = painter.font();
  font.setPointSize(12);
  font.setBold(true);
  const QFontMetrics metrics(font);
  const int width = metrics.horizontalAdvance(text) + 24;
  const int height = metrics.height() + 12;
  const QRect box(QPoint((this->width() - width) / 2, 24), QSize(width, height));

  painter.setPen(Qt::NoPen);
  painter.setBrush(QColor(0, 0, 0, 170));
  painter.drawRoundedRect(box, 8, 8);
  painter.setFont(font);
  painter.setPen(Qt::white);
  painter.drawText(box, Qt::AlignCenter, text);
}

void OverlayWindow::drawLabel(QPainter& painter, const QPoint& topLeft,
                              const QString& text, const QColor& background,
                              const QColor& textColor, int pointSize) const {
  QFont font = painter.font();
  font.setPointSize(pointSize);
  font.setBold(true);
  const QFontMetrics metrics(font);
  const QRect box(topLeft,
                  QSize(metrics.horizontalAdvance(text) + 14,
                        metrics.height() + 8));

  painter.save();
  painter.setPen(Qt::NoPen);
  painter.setBrush(background);
  painter.drawRoundedRect(box, 6, 6);
  painter.setFont(font);
  painter.setPen(textColor);
  painter.drawText(box, Qt::AlignCenter, text);
  painter.restore();
}

void OverlayWindow::ensureAnnotationCache() {
  if (!cacheDirty_) return;
  cacheDirty_ = false;
  annotatedCache_ = QImage();
  if (!selection_.isValid()) return;

  const QRect source = selectionImageRect();
  if (source.isEmpty()) return;

  const QImage crop = capture_.image.copy(source);
  annotatedCache_ = annotations_.flatten(crop);
  annotatedCache_.setDevicePixelRatio(capture_.dpr > 0 ? capture_.dpr : 1.0);
}

void OverlayWindow::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  setFocus(Qt::MouseFocusReason);
  cursor_ = event->pos();
  cursorValid_ = true;
  pressPos_ = event->pos();
  moved_ = false;

  if (selection_.hasSelection() && selection_.isValid() &&
      selection_.rect().contains(event->pos())) {
    if (toolbar_->activeTool() >= 0) {
      drawing_ = true;
      const QPoint imagePoint =
          toImagePoint(event->pos()) - selectionImageRect().topLeft();
      preview_ = makeShape(
          static_cast<core::ShapeType>(toolbar_->activeTool()), imagePoint);
    }
    // Without an active tool, a click inside the selection keeps it intact.
    update();
    event->accept();
    return;
  }

  annotations_.clear();
  cacheDirty_ = true;
  annotatedCache_ = QImage();
  drawing_ = false;
  hideToolbar();
  selection_.begin(event->pos());
  pressing_ = true;
  hoverRect_ = {};
  update();
  event->accept();
}

void OverlayWindow::mouseMoveEvent(QMouseEvent* event) {
  cursor_ = event->pos();
  cursorValid_ = true;

  if (drawing_) {
    const QPoint imagePoint =
        toImagePoint(event->pos()) - selectionImageRect().topLeft();
    if (preview_.points.isEmpty() || preview_.points.last() != imagePoint) {
      preview_.points.push_back(imagePoint);
    }
    update();
    event->accept();
    return;
  }

  if (pressing_) {
    selection_.update(event->pos());
    if ((event->pos() - pressPos_).manhattanLength() > 2) moved_ = true;
    update();
    event->accept();
    return;
  }

  if (!selection_.hasSelection()) updateHover(event->pos());
  update();
  event->accept();
}

void OverlayWindow::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  if (drawing_) {
    drawing_ = false;
    if (preview_.points.size() >= 1) {
      annotations_.addShape(preview_);
      cacheDirty_ = true;
    }
    preview_ = {};
    update();
    event->accept();
    return;
  }

  if (!pressing_) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  pressing_ = false;
  selection_.finish();

  if (!selection_.isValid()) {
    if (!moved_) {
      const QRect window = platform::windowAt(mapToGlobal(event->pos()));
      if (!window.isNull()) {
        selection_.setRect(window.translated(-capture_.geometry.topLeft())
                               .intersected(rect()));
      } else {
        selection_.reset();
      }
    } else {
      selection_.reset();
    }
  }

  if (selection_.isValid()) {
    annotations_.clear();
    cacheDirty_ = true;
    annotatedCache_ = QImage();
    showToolbar();
  } else {
    hideToolbar();
  }
  update();
  event->accept();
}

void OverlayWindow::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && selection_.isValid() &&
      toolbar_->activeTool() < 0) {
    commitSelection();
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}

void OverlayWindow::keyPressEvent(QKeyEvent* event) {
  const bool command = event->modifiers().testFlag(Qt::ControlModifier) ||
                       event->modifiers().testFlag(Qt::MetaModifier);

  switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
      commitSelection();
      event->accept();
      return;
    case Qt::Key_Escape:
      finishSession();
      event->accept();
      return;
    case Qt::Key_C:
      if (command) {
        copySelection();
        event->accept();
        return;
      }
      break;
    case Qt::Key_Z:
      if (command) {
        undoLastShape();
        event->accept();
        return;
      }
      break;
    default:
      break;
  }
  QWidget::keyPressEvent(event);
}

void OverlayWindow::updateHover(const QPoint& widgetPoint) {
  const QRect window = platform::windowAt(mapToGlobal(widgetPoint));
  const QRect local = window.translated(-capture_.geometry.topLeft())
                          .intersected(rect());
  if (local != hoverRect_) {
    hoverRect_ = local;
    update();
  }
}

void OverlayWindow::showToolbar() {
  if (!selection_.isValid()) return;
  const QRect sel = selection_.rect();
  const int toolbarWidth = toolbar_->width();
  const int toolbarHeight = toolbar_->height();

  int x = sel.right() + 8;
  if (x + toolbarWidth > width()) x = sel.left() - toolbarWidth - 8;
  if (x < 8) x = std::clamp(sel.right() - toolbarWidth, 8,
                            std::max(8, width() - toolbarWidth - 8));

  int y = sel.bottom() + 8;
  if (y + toolbarHeight > height()) y = sel.top() - toolbarHeight - 8;
  if (y < 8) y = std::clamp(sel.bottom() - toolbarHeight, 8,
                            std::max(8, height() - toolbarHeight - 8));

  toolbar_->move(x, y);
  toolbar_->show();
  toolbar_->raise();
}

void OverlayWindow::hideToolbar() {
  if (!toolbar_) return;
  toolbar_->deactivateTool();
  toolbar_->hide();
}

void OverlayWindow::undoLastShape() {
  if (!annotations_.undo()) return;
  cacheDirty_ = true;
  update();
}

void OverlayWindow::commitSelection() {
  if (committed_ || !selection_.isValid()) return;
  const QRect source = selectionImageRect();
  if (source.isEmpty()) return;

  QImage result = annotations_.flatten(capture_.image.copy(source));
  result.setDevicePixelRatio(capture_.dpr > 0 ? capture_.dpr : 1.0);
  const QPoint globalPosition =
      capture_.geometry.topLeft() + selection_.rect().topLeft();

  committed_ = true;
  if (onCommit_) onCommit_(result, globalPosition);
  finishSession();
}

void OverlayWindow::copySelection() {
  if (committed_ || !selection_.isValid()) return;
  const QRect source = selectionImageRect();
  if (source.isEmpty()) return;

  QImage result = annotations_.flatten(capture_.image.copy(source));
  result.setDevicePixelRatio(capture_.dpr > 0 ? capture_.dpr : 1.0);
  QApplication::clipboard()->setImage(result);
  finishSession();
}

void OverlayWindow::saveSelection() {
  if (committed_ || !selection_.isValid()) return;
  const QRect source = selectionImageRect();
  if (source.isEmpty()) return;

  const QString suggested =
      QStringLiteral("StickySnap-%1.png")
          .arg(QDateTime::currentDateTime().toString(
              QStringLiteral("yyyyMMdd-HHmmss")));
  const QString path = QFileDialog::getSaveFileName(
      this, tr("保存截图"), suggested,
      tr("PNG 图片 (*.png);;JPEG 图片 (*.jpg)"));
  if (path.isEmpty()) return;

  QImage result = annotations_.flatten(capture_.image.copy(source));
  QString target = path;
  if (QFileInfo(target).suffix().isEmpty()) target += QStringLiteral(".png");
  if (!result.save(target)) {
    qWarning("StickySnap: failed to save capture to %s", qPrintable(target));
    return;
  }
  finishSession();
}

void OverlayWindow::finishSession() {
  if (closedNotified_) return;
  closedNotified_ = true;
  if (onClosed_) onClosed_();
}

}  // namespace sticky::ui
