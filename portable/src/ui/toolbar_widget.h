#pragma once

#include <QColor>
#include <QRect>
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

  struct Button {
    QRect rect;
    Kind kind = Kind::Tool;
    int id = 0;
    QString tooltip;
  };

  void relayout();
  int hitTest(const QPoint& position) const;
  void paintButton(QPainter& painter, const Button& button,
                   bool hovered) const;
  void paintToolIcon(QPainter& painter, const QRect& rect, int tool,
                     const QColor& color) const;
  void paintActionIcon(QPainter& painter, const QRect& rect, int action,
                       const QColor& color) const;
  void paintUndoIcon(QPainter& painter, const QRect& rect,
                     const QColor& color) const;
  void paintPinIcon(QPainter& painter, const QRect& rect,
                    const QColor& color) const;
  void paintCopyIcon(QPainter& painter, const QRect& rect,
                     const QColor& color) const;
  void paintSaveIcon(QPainter& painter, const QRect& rect,
                     const QColor& color) const;

  QVector<Button> buttons_;
  int activeTool_ = -1;
  int colorIndex_ = 4;
  int widthIndex_ = 1;
  int hoveredButton_ = -1;
  QColor accent_ = QColor(QStringLiteral("#FF4B3E"));
};

}  // namespace sticky::ui
