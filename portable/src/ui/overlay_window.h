#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QWidget>

#include <functional>

#include "core/annotation_model.h"
#include "core/selection_model.h"
#include "core/settings.h"
#include "platform/platform.h"

class QCloseEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QShowEvent;

namespace sticky::ui {

class ToolbarWidget;

// One frozen display with the selection, magnifier, and annotation toolbar.
// The session creates one overlay per display and closes them together.
class OverlayWindow : public QWidget {
  Q_OBJECT

 public:
  using CommitFn = std::function<void(const QImage&, const QPoint&)>;
  using ClosedFn = std::function<void()>;

  OverlayWindow(platform::DisplayCapture capture, const core::Settings& settings,
                CommitFn onCommit, ClosedFn onClosed,
                QWidget* parent = nullptr);
  ~OverlayWindow() override;

 protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void showEvent(QShowEvent* event) override;
  void closeEvent(QCloseEvent* event) override;

 private:
  QRect selectionImageRect() const;
  QPoint toImagePoint(const QPoint& widgetPoint) const;
  core::Shape makeShape(core::ShapeType type, const QPoint& imagePoint) const;

  void ensureAnnotationCache();
  void showToolbar();
  void hideToolbar();
  void updateHover(const QPoint& widgetPoint);

  void commitSelection();
  void copySelection();
  void saveSelection();
  void undoLastShape();
  void finishSession();

  void drawMagnifier(QPainter& painter);
  void drawHint(QPainter& painter);
  void drawSelectionLabel(QPainter& painter);
  void drawPreview(QPainter& painter);
  void drawHover(QPainter& painter);
  void drawLabel(QPainter& painter, const QPoint& topLeft, const QString& text,
                 const QColor& background, const QColor& textColor,
                 int pointSize = 11) const;

  platform::DisplayCapture capture_;
  core::Settings settings_;
  core::SelectionModel selection_;
  core::AnnotationModel annotations_;
  QImage annotatedCache_;
  bool cacheDirty_ = true;

  QPoint cursor_;
  bool cursorValid_ = false;
  QRect hoverRect_;
  QPoint pressPos_;
  bool pressing_ = false;
  bool moved_ = false;
  bool drawing_ = false;
  core::Shape preview_;

  ToolbarWidget* toolbar_ = nullptr;
  CommitFn onCommit_;
  ClosedFn onClosed_;
  bool committed_ = false;
  bool closedNotified_ = false;
  bool nativeConfigured_ = false;
};

}  // namespace sticky::ui
