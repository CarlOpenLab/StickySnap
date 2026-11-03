#include "ui/toolbar_widget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QToolTip>

#include "core/annotation_style.h"

namespace sticky::ui {
namespace {

constexpr qreal kPanelWidth = 351.0;
constexpr qreal kColorPanelWidth = 290.0;
constexpr qreal kRowHeight = 37.0;
constexpr qreal kRowGap = 5.0;
constexpr qreal kRowTwoTop = kRowHeight + kRowGap;
constexpr qreal kToolbarWidth = kPanelWidth;
constexpr qreal kToolbarHeight = kRowTwoTop + kRowHeight;

constexpr qreal kBorder = 1.0;
constexpr qreal kPadding = 4.5;
constexpr qreal kButton = 26.0;
constexpr qreal kIconSize = 14.0;
constexpr qreal kGap = 3.0;
constexpr qreal kColorButton = 18.0;
constexpr qreal kWidthButton = 24.0;
constexpr qreal kColorDot = 13.0;
constexpr qreal kSeparatorWidth = 1.0;
constexpr qreal kSeparatorMargin = 4.0;
constexpr qreal kSeparatorInset = 2.0;
constexpr qreal kPanelRadius = 9.0;
constexpr qreal kButtonRadius = 6.0;

const QColor kPanelColor(0x16, 0x1A, 0x22);
const QColor kPanelBorder(255, 255, 255, 23);
const QColor kSeparatorColor(255, 255, 255, 31);
const QColor kIconColor(0xC7, 0xCD, 0xD8);
const QColor kHoverColor(255, 255, 255, 16);

}  // namespace

ToolbarWidget::ToolbarWidget(QWidget* parent) : QWidget(parent) {
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_NoSystemBackground);
  setMouseTracking(true);
  setFocusPolicy(Qt::NoFocus);

  setFixedSize(static_cast<int>(kToolbarWidth),
               static_cast<int>(kToolbarHeight));
  relayout();
}

void ToolbarWidget::relayout() {
  buttons_.clear();
  separators_.clear();

  auto add = [this](const QRectF& rect, Kind kind, int id,
                    const QString& tooltip) {
    buttons_.push_back(Button{rect, kind, id, tooltip});
  };

  const qreal contentLeft = kBorder + kPadding;
  const qreal contentTop = kBorder + kPadding;
  const qreal separatorTop = contentTop + kSeparatorInset;
  const qreal separatorHeight = kButton - 2 * kSeparatorInset;
  const auto addSeparator = [this, separatorHeight](qreal x, qreal top) {
    separators_.push_back(QRectF(x + kSeparatorMargin, top, kSeparatorWidth,
                                 separatorHeight));
  };

  const QStringList toolNames = {
      tr("矩形"), tr("椭圆"), tr("箭头"), tr("画笔"), tr("荧光笔"), tr("马赛克")};
  qreal x = contentLeft;
  for (int i = 0; i < 6; ++i) {
    add(QRectF(x, contentTop, kButton, kButton), Kind::Tool, i,
        toolNames.value(i));
    x += kButton;
    if (i < 5) x += kGap;
  }

  x += kGap;
  addSeparator(x, separatorTop);
  x += kSeparatorMargin * 2 + kSeparatorWidth + kGap;

  add(QRectF(x, contentTop, kButton, kButton), Kind::Action, kUndo,
      tr("撤销"));
  x += kButton;

  x += kGap;
  addSeparator(x, separatorTop);
  x += kSeparatorMargin * 2 + kSeparatorWidth + kGap;

  const int actionIds[] = {kPin, kCopy, kSave, kCancel};
  const QStringList actionNames = {tr("贴图"), tr("复制"), tr("保存"),
                                   tr("取消")};
  for (int i = 0; i < 4; ++i) {
    add(QRectF(x, contentTop, kButton, kButton), Kind::Action, actionIds[i],
        actionNames.value(i));
    x += kButton;
    if (i < 3) x += kGap;
  }

  const qreal rowTwoContentTop = kRowTwoTop + kBorder + kPadding;
  const qreal rowTwoSeparatorTop = rowTwoContentTop + kSeparatorInset;
  x = contentLeft;
  for (int i = 0; i < 9; ++i) {
    add(QRectF(x, rowTwoContentTop, kColorButton, kButton), Kind::Color, i,
        tr("颜色 %1").arg(i + 1));
    x += kColorButton;
    if (i < 8) x += kGap;
  }

  x += kGap;
  addSeparator(x, rowTwoSeparatorTop);
  x += kSeparatorMargin * 2 + kSeparatorWidth + kGap;

  for (int i = 0; i < 3; ++i) {
    add(QRectF(x, rowTwoContentTop, kWidthButton, kButton), Kind::Width, i,
        tr("粗细 %1").arg(i + 1));
    x += kWidthButton;
    if (i < 2) x += kGap;
  }
}

int ToolbarWidget::hitTest(const QPointF& position) const {
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

  const int index = hitTest(event->position());
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
  const int index = hitTest(event->position());
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

  const auto paintPanel = [&painter](const QRectF& panel) {
    painter.setPen(QPen(kPanelBorder, 1));
    painter.setBrush(kPanelColor);
    painter.drawRoundedRect(panel, kPanelRadius, kPanelRadius);
  };
  paintPanel(QRectF(0.5, 0.5, kPanelWidth - 1.0, kRowHeight - 1.0));
  paintPanel(QRectF(0.5, kRowTwoTop + 0.5, kColorPanelWidth - 1.0,
                    kRowHeight - 1.0));

  painter.setPen(Qt::NoPen);
  painter.setBrush(kSeparatorColor);
  for (const QRectF& separator : separators_) {
    painter.drawRect(separator);
  }

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
  const QPointF center = button.rect.center();

  if (button.kind == Kind::Color) {
    const qreal radius = kColorDot / 2.0;
    if (selected) {
      painter.setPen(QPen(Qt::white, 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(center, radius + 2.5, radius + 2.5);
    }
    const QColor color = core::paletteColor(button.id);
    painter.setPen(color.lightness() > 200 ? QColor(0, 0, 0, 80) : Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(center, radius, radius);
    return;
  }

  if (button.kind == Kind::Width) {
    const qreal radius = core::kDotDiameters[button.id] / 2.0;
    if (selected) {
      painter.setPen(QPen(Qt::white, 2));
      painter.setBrush(Qt::NoBrush);
      painter.drawEllipse(center, radius + 2.5, radius + 2.5);
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(kIconColor);
    painter.drawEllipse(center, radius, radius);
    return;
  }

  if (selected || hovered) {
    painter.setPen(Qt::NoPen);
    painter.setBrush(selected ? accent_ : kHoverColor);
    painter.drawRoundedRect(button.rect, kButtonRadius, kButtonRadius);
  }

  const QColor iconColor = (selected || hovered) ? QColor(Qt::white)
                                                 : kIconColor;
  const QRectF iconRect(center.x() - kIconSize / 2.0,
                        center.y() - kIconSize / 2.0, kIconSize, kIconSize);
  paintIcon(painter, iconRect, iconFor(button.kind, button.id), iconColor);
}

ToolbarWidget::Icon ToolbarWidget::iconFor(Kind kind, int id) {
  if (kind == Kind::Tool) {
    switch (id) {
      case 0:
        return Icon::Rect;
      case 1:
        return Icon::Ellipse;
      case 2:
        return Icon::Arrow;
      case 3:
        return Icon::Pen;
      case 4:
        return Icon::Highlighter;
      case 5:
        return Icon::Mosaic;
      default:
        return Icon::Rect;
    }
  }

  switch (id) {
    case kUndo:
      return Icon::Undo;
    case kPin:
      return Icon::Pin;
    case kCopy:
      return Icon::Copy;
    case kSave:
      return Icon::Save;
    case kCancel:
      return Icon::Cancel;
    default:
      return Icon::Rect;
  }
}

void ToolbarWidget::paintIcon(QPainter& painter, const QRectF& rect, Icon icon,
                              const QColor& color) const {
  painter.save();
  painter.translate(rect.topLeft());
  painter.scale(rect.width() / 24.0, rect.height() / 24.0);
  painter.setPen(
      QPen(color, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);

  switch (icon) {
    case Icon::Rect:
      painter.drawRoundedRect(QRectF(4, 6.5, 16, 11), 1, 1);
      break;
    case Icon::Ellipse:
      painter.drawEllipse(QPointF(12, 12), 8, 5.5);
      break;
    case Icon::Arrow: {
      painter.drawLine(QPointF(6, 18), QPointF(18, 6));
      QPainterPath path;
      path.moveTo(12.5, 6);
      path.lineTo(18, 6);
      path.lineTo(18, 11.5);
      painter.drawPath(path);
      break;
    }
    case Icon::Pen: {
      QPainterPath path;
      path.moveTo(4, 20);
      path.lineTo(5.2, 15.4);
      path.lineTo(16, 4.6);
      path.quadTo(17.4, 3.2, 18.8, 4.6);
      path.lineTo(20.4, 6.2);
      path.quadTo(21.8, 7.6, 20.4, 9.0);
      path.lineTo(9.6, 19.8);
      path.closeSubpath();
      painter.drawPath(path);
      painter.drawLine(QPointF(13.5, 6.5), QPointF(17.5, 10.5));
      break;
    }
    case Icon::Highlighter: {
      painter.drawLine(QPointF(4, 20), QPointF(11, 20));
      QPainterPath path;
      path.moveTo(8, 16);
      path.lineTo(15.5, 5);
      path.lineTo(18.9, 7.3);
      path.lineTo(11.4, 18);
      path.lineTo(6.8, 18.9);
      path.closeSubpath();
      painter.drawPath(path);
      break;
    }
    case Icon::Mosaic:
      painter.drawRect(QRectF(4, 4, 7, 7));
      painter.drawRect(QRectF(13, 4, 7, 7));
      painter.drawRect(QRectF(4, 13, 7, 7));
      painter.drawRect(QRectF(13, 13, 7, 7));
      break;
    case Icon::Undo: {
      QPainterPath arrow;
      arrow.moveTo(7.5, 5);
      arrow.lineTo(3.5, 8.5);
      arrow.lineTo(7.5, 12);
      painter.drawPath(arrow);

      QPainterPath arc;
      arc.moveTo(4, 8.5);
      arc.lineTo(15, 8.5);
      arc.arcTo(QRectF(9.5, 8.5, 11, 11), 90, -180);
      arc.lineTo(8, 19.5);
      painter.drawPath(arc);
      break;
    }
    case Icon::Pin: {
      QPainterPath path;
      path.moveTo(9.5, 3.5);
      path.lineTo(14.5, 3.5);
      path.lineTo(13.8, 7.7);
      path.lineTo(16.5, 11);
      path.lineTo(7.5, 11);
      path.lineTo(10.2, 7.7);
      path.closeSubpath();
      painter.drawPath(path);
      painter.drawLine(QPointF(12, 11), QPointF(12, 20));
      break;
    }
    case Icon::Copy: {
      painter.drawRoundedRect(QRectF(9, 9, 11, 11), 2, 2);
      QPainterPath path;
      path.moveTo(5, 15);
      path.lineTo(5, 5);
      path.quadTo(5, 3, 7, 3);
      path.lineTo(17, 3);
      painter.drawPath(path);
      break;
    }
    case Icon::Save: {
      QPainterPath outline;
      outline.moveTo(5, 4);
      outline.lineTo(16, 4);
      outline.lineTo(19, 7);
      outline.lineTo(19, 20);
      outline.lineTo(5, 20);
      outline.closeSubpath();
      painter.drawPath(outline);

      QPainterPath top;
      top.moveTo(8, 4);
      top.lineTo(8, 9);
      top.lineTo(15, 9);
      top.lineTo(15, 4);
      painter.drawPath(top);

      QPainterPath bottom;
      bottom.moveTo(8, 20);
      bottom.lineTo(8, 14);
      bottom.lineTo(16, 14);
      bottom.lineTo(16, 20);
      painter.drawPath(bottom);
      break;
    }
    case Icon::Cancel:
      painter.drawLine(QPointF(6, 6), QPointF(18, 18));
      painter.drawLine(QPointF(18, 6), QPointF(6, 18));
      break;
  }

  painter.restore();
}

}  // namespace sticky::ui
