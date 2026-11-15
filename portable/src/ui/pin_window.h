#pragma once

#include <QImage>
#include <QPoint>
#include <QSizeF>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

#include "core/annotation_model.h"
#include "core/session_store.h"

class QEnterEvent;

namespace sticky::ui {

class ToolbarWidget;

// A borderless always-on-top image pinned to the desktop.
// kind 决定内容的来源：Image 用 image_ 原图；Animated 由 imagePath_ 播放
// QMovie；Text/Color 由 imagePath_ 指向的侧车 JSON 渲染一次生成 image_。
class PinWindow : public QWidget {
  Q_OBJECT

 public:
  PinWindow(QImage image, QString imagePath,
            const sticky::core::PinSession& state,
            QWidget* parent = nullptr);

  sticky::core::PinSession session() const;

  void setPassThrough(bool enabled);
  bool passThrough() const { return passThrough_; }

  // During a capture the pinned windows are raised above the frozen overlay
  // and made click-through; this restores their normal state afterwards.
  void setOverlayMode(bool active);

  int alphaValue() const { return alpha_; }
  double zoom() const { return zoom_; }

  sticky::core::PinKind kind() const { return kind_; }

  // 贴图分组（由 PinManager 提供候选并监听变更）。
  QString groupId() const { return groupId_; }
  void setGroupId(const QString& groupId);
  // 右键菜单「分组」的候选列表；由 PinManager 注入，避免反向依赖。
  void setGroupSource(std::function<QStringList()> source);

  // 贴图标注：恢复会话时从侧车 JSON 载入已有形状；文件不存在时为空。
  bool loadAnnotations();
  // 复制/另存为导出的成品图（原图 + 标注扁平化）。
  QImage exportImage() const { return displayImage_.isNull() ? image_ : displayImage_; }

  // 托盘消息回调（由 AppController 注入）；OCR 等操作用它反馈结果。
  void setNotify(std::function<void(const QString&)> notify);

 signals:
  void changed();
  void closed(PinWindow* self);
  void groupChanged();
  void soloRequested(PinWindow* self);

 protected:
  void paintEvent(QPaintEvent* event) override;
  void enterEvent(QEnterEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void mouseDoubleClickEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void contextMenuEvent(QContextMenuEvent* event) override;
  void closeEvent(QCloseEvent* event) override;
  void showEvent(QShowEvent* event) override;

 private:
  void applyZoom(bool notify);
  void setAlpha(int alpha);
  void copyToClipboard();
  void saveAs();
  void ocrCopy();

  void setupAnimatedContent();
  void setupTextContent();
  void setupColorContent();
  void setEditMode(bool enabled);
  void refreshDisplayImage();
  QString annotationsPath() const;
  void saveAnnotations();
  void positionToolbar();
  QPoint toImagePoint(const QPoint& widgetPosition) const;

  QImage image_;  // 原图（base），标注扁平化的底图，从不修改
  QString imagePath_;
  sticky::core::PinKind kind_ = sticky::core::PinKind::Image;
  qreal dpr_ = 1.0;
  QSizeF logicalSize_;
  double zoom_ = 1.0;
  int alpha_ = 255;
  bool passThrough_ = false;
  bool overlayMode_ = false;
  bool nativeConfigured_ = false;
  bool dragging_ = false;
  bool hovered_ = false;
  QPoint dragOffset_;

  // 贴图标注（仅 Image/Animated）
  QImage displayImage_;
  sticky::core::AnnotationModel annotations_;
  core::Shape liveShape_;
  bool editMode_ = false;
  bool drawing_ = false;
  bool passThroughBeforeEdit_ = false;
  int activeTool_ = -1;
  int activeColorIndex_ = 4;
  int activeWidthIndex_ = 1;
  ToolbarWidget* toolbar_ = nullptr;

  QString groupId_;
  std::function<QStringList()> groupSource_;
  std::function<void(const QString&)> notify_;
  bool desktopBound_ = false;  // 仅当前桌面显示（macOS 生效；不持久化）
};

}  // namespace sticky::ui
