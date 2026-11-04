#pragma once

#include <QColor>
#include <QRectF>
#include <QString>
#include <QVector>
#include <QWidget>

class QMouseEvent;
class QPaintEvent;

namespace sticky::ui {

// Floating Snipaste-style toolbar drawn as a child of the capture overlay.
// It owns the current tool/color/width selection.
class ToolbarWidget : public QWidget {
  Q_OBJECT

 public:
  enum Action {
    kUndo = 100,
    kPin,
    kCopy,
    kSave,
    kCancel,
  };

  explicit ToolbarWidget(QWidget* parent = nullptr);

  int activeTool() const { return activeTool_; }
  int colorIndex() const { return colorIndex_; }
  int widthIndex() const { return widthIndex_; }

  void deactivateTool();
  void setAccent(const QColor& accent);

 signals:
  void toolChanged(int tool);
  void colorChanged(int index);
  void widthChanged(int index);
  void actionTriggered(int action);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void leaveEvent(QEvent* event) override;

 private:
  enum class Kind { Tool, Color, Width, Action };
  enum class Icon {
    Rect,
    Ellipse,
    Arrow,
    Pen,
    Highlighter,
    Mosaic,
    Undo,
    Pin,
    Copy,
    Save,
    Cancel,
  };

  struct Button {
    QRectF rect;
    Kind kind = Kind::Tool;
    int id = 0;
    QString tooltip;
  };

  void relayout();
  int hitTest(const QPointF& position) const;
  void paintButton(QPainter& painter, const Button& button,
                   bool hovered) const;
  void paintIcon(QPainter& painter, const QRectF& rect, Icon icon,
                 const QColor& color) const;
  static Icon iconFor(Kind kind, int id);

  QVector<Button> buttons_;
  QVector<QRectF> separators_;
  int activeTool_ = -1;
  int colorIndex_ = 4;
  int widthIndex_ = 1;
  int hoveredButton_ = -1;
  QColor accent_ = QColor(QStringLiteral("#2F6FED"));
};

}  // namespace sticky::ui
