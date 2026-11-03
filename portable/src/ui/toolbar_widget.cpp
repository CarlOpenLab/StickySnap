#include "ui/toolbar_widget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QToolTip>

#include <algorithm>

#include "core/annotation_style.h"

namespace sticky::ui {
namespace {

constexpr int kPadding = 8;
constexpr int kButton = 28;
constexpr int kGap = 4;
constexpr int kRowGap = 6;
constexpr int kColorButton = 20;
constexpr int kWidthButton = 24;
constexpr int kSecondRowY = kPadding + kButton + kRowGap;

}  // namespace

ToolbarWidget::ToolbarWidget(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setMouseTracking(true);
  setFocusPolicy(Qt::NoFocus);

  const int firstRowWidth = kPadding + 6 * (kButton + kGap) + 8 +
                            5 * (kButton + kGap) - kGap + kPadding;
  const int secondRowWidth =
      kPadding * 2 + 9 * kColorButton + 8 * 5 + 8 + 3 * kWidthButton + 2 * 5;
  setFixedSize(std::max(firstRowWidth, secondRowWidth), kSecondRowY +
                                                           kWidthButton +
                                                           kPadding);
  relayout();
}

void ToolbarWidget::relayout() {
  buttons_.clear();

  auto add = [this](const QRect& rect, Kind kind, int id,
                    const QString& tooltip) {
    buttons_.push_back(Button{rect, kind, id, tooltip});
  };

  int x = kPadding;
  const QStringList toolNames = {
      tr("矩形"), tr("椭圆"), tr("箭头"), tr("画笔"), tr("荧光笔"), tr("马赛克")};
  for (int i = 0; i < 6; ++i) {
    add(QRect(x, kPadding, kButton, kButton), Kind::Tool, i,
        toolNames.value(i));
    x += kButton + kGap;
  }

  x += 8;
  add(QRect(x, kPadding, kButton, kButton), Kind::Action, kUndo, tr("撤销"));
  x += kButton + kGap;
  add(QRect(x, kPadding, kButton, kButton), Kind::Action, kPin, tr("贴图"));
  x += kButton + kGap;
  add(QRect(x, kPadding, kButton, kButton), Kind::Action, kCopy, tr("复制"));
  x += kButton + kGap;
  add(QRect(x, kPadding, kButton, kButton), Kind::Action, kSave, tr("保存"));
  x += kButton + kGap;
  add(QRect(x, kPadding, kButton, kButton), Kind::Action, kCancel, tr("取消"));

  x = kPadding;
  for (int i = 0; i < 9; ++i) {
    add(QRect(x, kSecondRowY, kColorButton, kWidthButton), Kind::Color, i,
        tr("颜色 %1").arg(i + 1));
    x += kColorButton + 5;
  }

  x += 8;
  for (int i = 0; i < 3; ++i) {
    add(QRect(x, kSecondRowY, kWidthButton, kWidthButton), Kind::Width, i,
        tr("粗细 %1").arg(i + 1));
    x += kWidthButton + 5;
  }
}

int ToolbarWidget::hitTest(const QPoint& position) const {
  for (qsizetype i = 0; i < buttons_.size(); ++i) {
    if (buttons_.at(i).rect.contains(position)) return static_cast<int>(i);
  }
  return -1;
}

void ToolbarWidget::deactivateTool() {
  if (activeTool_ == -1) return;
  activeTool_ = -1;
  update();
  emit toolChanged(-1);
}

void ToolbarWidget::setAccent(const QColor& accent) {
  accent_ = accent;
  update();
}

void ToolbarWidget::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  const int index = hitTest(event->pos());
  if (index < 0) {
    event->ignore();
    return;
  }

  const Button& button = buttons_.at(index);
  switch (button.kind) {
    case Kind::Tool:
      activeTool_ = activeTool_ == button.id ? -1 : button.id;
      emit toolChanged(activeTool_);
      break;
    case Kind::Color:
      colorIndex_ = button.id;
      emit colorChanged(colorIndex_);
      break;
    case Kind::Width:
      widthIndex_ = button.id;
      emit widthChanged(widthIndex_);
      break;
    case Kind::Action:
      emit actionTriggered(button.id);
      break;
  }
  update();
  event->accept();
}

void ToolbarWidget::mouseMoveEvent(QMouseEvent* event) {
  const int index = hitTest(event->pos());
  if (index != hoveredButton_) {
    hoveredButton_ = index;
    update();
  }
  if (index >= 0) {
    setCursor(Qt::PointingHandCursor);
    QToolTip::showText(event->globalPosition().toPoint(),
                       buttons_.at(index).tooltip, this);
  } else {
    unsetCursor();
    QToolTip::hideText();
  }
  QWidget::mouseMoveEvent(event);
}

void ToolbarWidget::leaveEvent(QEvent* event) {
  hoveredButton_ = -1;
  unsetCursor();
  QToolTip::hideText();
  update();
  QWidget::leaveEvent(event);
}

void ToolbarWidget::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRectF background = rect().adjusted(0, 0, -1, -1);
  painter.setPen(QPen(QColor(58, 61, 66), 1));
  painter.setBrush(QColor(30, 31, 34, 242));
  painter.drawRoundedRect(background, 10, 10);

  for (qsizetype i = 0; i < buttons_.size(); ++i) {
    paintButton(painter, buttons_.at(i), static_cast<int>(i) == hoveredButton_);
  }
}

void ToolbarWidget::paintButton(QPainter& painter, const Button& button,
                                bool hovered) const {
  const bool selected =
      (button.kind == Kind::Tool && button.id == activeTool_) ||
      (button.kind == Kind::Color && button.id == colorIndex_) ||
      (button.kind == Kind::Width && button.id == widthIndex_);

  if (button.kind == Kind::Color) {
    if (selected) {
      painter.setPen(QPen(accent_, 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(button.rect.center(), 9, 9);
    }
    const QColor color = core::paletteColor(button.id);
    painter.setPen(color == QColor(Qt::white) ? QColor(120, 120, 120)
                                              : Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(button.rect.center(), 7, 7);
    return;
  }

  if (button.kind == Kind::Width) {
    if (selected) {
      painter.setPen(QPen(accent_, 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(button.rect.center(), 9, 9);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(230, 230, 230));
    const int diameter = core::kDotDiameters[button.id];
    painter.drawEllipse(button.rect.center(), diameter / 2, diameter / 2);
    return;
  }

  if (selected || hovered) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(selected ? accent_ : QColor(46, 48, 53));
    painter.drawRoundedRect(button.rect.adjusted(1, 1, -1, -1), 6, 6);
  }

  const QColor iconColor = selected ? QColor(Qt::white) : QColor(230, 230, 230);
  const QRect iconRect = button.rect.adjusted(6, 6, -6, -6);

  if (button.kind == Kind::Tool) {
    paintToolIcon(painter, iconRect, button.id, iconColor);
  } else {
    paintActionIcon(painter, iconRect, button.id, iconColor);
  }
}

void ToolbarWidget::paintToolIcon(QPainter& painter, const QRect& rect, int tool,
                                  const QColor& color) const {
  painter.save();
  painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);

  switch (tool) {
    case 0:  // rectangle
      painter.drawRect(rect.adjusted(1, 2, -1, -2));
      break;
    case 1:  // ellipse
      painter.drawEllipse(rect.adjusted(1, 2, -1, -2));
      break;
    case 2: {  // arrow
      const QPoint start(rect.left() + 1, rect.bottom() - 1);
      const QPoint end(rect.right() - 1, rect.top() + 1);
      painter.drawLine(start, end);
      painter.drawLine(end, QPoint(end.x() - 5, end.y() + 2));
      painter.drawLine(end, QPoint(end.x() - 2, end.y() + 5));
      break;
    }
    case 3: {  // pen
      painter.drawLine(rect.bottomLeft() + QPoint(2, -2),
                       rect.topRight() + QPoint(-2, 2));
      painter.setBrush(color);
      painter.drawEllipse(QPointF(rect.topRight() + QPoint(-2, 2)), 1.8, 1.8);
      break;
    }
    case 4: {  // highlighter
      QColor marker = color;
      marker.setAlpha(150);
      painter.setPen(QPen(marker, static_cast<int>(rect.height() * 0.45),
                          Qt::SolidLine, Qt::RoundCap));
      painter.drawLine(rect.bottomLeft() + QPoint(3, -3),
                       rect.topRight() + QPoint(-3, 3));
      break;
    }
    case 5: {  // mosaic
      const int cell = std::max(3, rect.width() / 3);
      for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
          if ((row + col) % 2 != 0) continue;
          painter.fillRect(QRect(rect.left() + col * cell,
                                 rect.top() + row * cell, cell - 1, cell - 1),
                           color);
        }
      }
      break;
    }
  }
  painter.restore();
}

void ToolbarWidget::paintActionIcon(QPainter& painter, const QRect& rect,
                                    int action, const QColor& color) const {
  switch (action) {
    case kUndo:
      paintUndoIcon(painter, rect, color);
      break;
    case kPin:
      paintPinIcon(painter, rect, color);
      break;
    case kCopy:
      paintCopyIcon(painter, rect, color);
      break;
    case kSave:
      paintSaveIcon(painter, rect, color);
      break;
    case kCancel: {
      painter.save();
      painter.setPen(
          QPen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
      painter.drawLine(rect.topLeft() + QPoint(2, 2),
                       rect.bottomRight() - QPoint(2, 2));
      painter.drawLine(rect.topRight() + QPoint(-2, 2),
                       rect.bottomLeft() + QPoint(2, -2));
      painter.restore();
      break;
    }
  }
}

void ToolbarWidget::paintUndoIcon(QPainter& painter, const QRect& rect,
                                  const QColor& color) const {
  painter.save();
  painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);
  QPainterPath path;
  path.moveTo(rect.right() - 1, rect.bottom() - 1);
  path.cubicTo(rect.right() - 1, rect.top() + 2, rect.left() + 5,
               rect.top() + 2, rect.left() + 4, rect.center().y() + 2);
  painter.drawPath(path);
  const QPoint tip(rect.left() + 4, rect.center().y() + 2);
  painter.drawLine(tip, tip + QPoint(4, -4));
  painter.drawLine(tip, tip + QPoint(2, 5));
  painter.restore();
}

void ToolbarWidget::paintPinIcon(QPainter& painter, const QRect& rect,
                                 const QColor& color) const {
  painter.save();
  painter.setPen(QPen(color, 1.6, Qt::SolidLine, Qt::RoundCap));
  painter.setBrush(color);
  const QPoint center = rect.center();
  painter.drawEllipse(QPointF(center + QPoint(0, -3)), 3.6, 3.6);
  painter.drawLine(center + QPoint(0, 1), center + QPoint(0, 6));
  painter.drawLine(center + QPoint(-4, 6), center + QPoint(4, 6));
  painter.restore();
}

void ToolbarWidget::paintCopyIcon(QPainter& painter, const QRect& rect,
                                  const QColor& color) const {
  painter.save();
  painter.setPen(QPen(color, 1.4));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(rect.adjusted(0, 0, -4, -4), 2, 2);
  painter.drawRoundedRect(rect.adjusted(4, 4, 0, 0), 2, 2);
  painter.restore();
}

void ToolbarWidget::paintSaveIcon(QPainter& painter, const QRect& rect,
                                  const QColor& color) const {
  painter.save();
  painter.setPen(QPen(color, 1.4));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 2, 2);
  painter.drawRect(QRect(rect.left() + 4, rect.top() + 1, rect.width() - 8, 5));
  painter.drawRect(
      QRect(rect.left() + 3, rect.bottom() - 6, rect.width() - 6, 5));
  painter.restore();
}

}  // namespace sticky::ui
