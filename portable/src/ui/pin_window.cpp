#include "ui/pin_window.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QColorDialog>
#include <QContextMenuEvent>
#include <QEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QMovie>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPointer>
#include <QScreen>
#include <QTextDocument>
#include <QTransform>
#include <QWheelEvent>

#include <algorithm>
#include <utility>

#include "core/annotation_style.h"
#include "core/constants.h"
#include "core/settings.h"
#include "platform/platform.h"
#include "ui/toolbar_widget.h"

namespace sticky::ui {
namespace {

constexpr double kZoomStep = 1.1;
constexpr int kAlphaStep = 16;
constexpr int kColorPinDefaultSize = 96;
constexpr int kTextPinDefaultFontSize = 24;
constexpr int kTextPinMaxWidth = 540;

// 首次创建贴图时从 config.ini 载入边框与强调色，重启后沿用用户设置。
void loadPinAppearanceOnce() {
  static const bool loaded = [] {
    const core::Settings settings =
        core::SettingsStore(platform::settingsPath()).load();
    core::setPinAppearance(core::PinAppearance{settings.pinBorder,
                                               settings.accent});
    return true;
  }();
  (void)loaded;
}

QJsonObject readSidecar(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) return {};
  return QJsonDocument::fromJson(file.readAll()).object();
}

bool writeSidecar(const QString& path, const QJsonObject& object) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  return file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) > 0;
}

}  // namespace

PinWindow::PinWindow(QImage image, QString imagePath,
                     const sticky::core::PinSession& state, QWidget* parent)
    : QWidget(parent),
      image_(std::move(image)),
      imagePath_(std::move(imagePath)),
      kind_(state.kind),
      dpr_(state.dpr > 0.0 ? state.dpr : image_.devicePixelRatio()) {
  if (dpr_ <= 0.0) dpr_ = 1.0;
  image_.setDevicePixelRatio(dpr_);

  zoom_ = std::clamp(state.zoom, core::kZoomMin, core::kZoomMax);
  alpha_ = std::clamp(state.alpha, core::kAlphaMin, core::kAlphaMax);
  passThrough_ = state.passThrough;
  groupId_ = state.groupId;

  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                 Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_ShowWithoutActivating);
  setFocusPolicy(Qt::NoFocus);

  switch (kind_) {
    case core::PinKind::Animated:
      setupAnimatedContent();
      break;
    case core::PinKind::Text:
      setupTextContent();
      break;
    case core::PinKind::Color:
      setupColorContent();
      break;
    case core::PinKind::Image:
      logicalSize_ = QSizeF(image_.width() / dpr_, image_.height() / dpr_);
      break;
  }

  loadPinAppearanceOnce();
  move(state.position);
  applyZoom(false);
  setAlpha(alpha_);
  setPassThrough(passThrough_);
  refreshDisplayImage();
}

void PinWindow::setupAnimatedContent() {
  // 原始动画文件按原样保存（保真），播放时逐帧绘制。
  auto* movie = new QMovie(imagePath_, QByteArray(), this);
  if (!movie->isValid()) {
    // 动画解码失败：退化为静态占位图，避免窗口尺寸为 0。
    image_ = QImage(160, 120, QImage::Format_ARGB32_Premultiplied);
    image_.fill(QColor(0x16, 0x1A, 0x22));
    image_.setDevicePixelRatio(dpr_);
    logicalSize_ = QSizeF(image_.width() / dpr_, image_.height() / dpr_);
    movie->deleteLater();
    return;
  }
  movie->jumpToFrame(0);
  const QImage frame = movie->currentImage();
  image_ = frame.isNull() ? image_ : frame;
  logicalSize_ = QSizeF(image_.width() / dpr_, image_.height() / dpr_);
  connect(movie, &QMovie::frameChanged, this, [this] { update(); });
  movie->start();
}

void PinWindow::setupTextContent() {
  const QJsonObject sidecar = readSidecar(imagePath_);
  const QString text = sidecar.value(QLatin1String("text")).toString();
  const QColor color(sidecar.value(QLatin1String("color"))
                         .toString(QLatin1String("#161A22")));
  const int fontSize = sidecar.value(QLatin1String("fontSize"))
                           .toInt(kTextPinDefaultFontSize);

  QTextDocument document;
  QFont font = QApplication::font();
  font.setPointSize(qMax(8, fontSize));
  document.setDefaultFont(font);
  // QTextDocument 没有默认文字颜色 API，用 HTML 包一层（文本需转义）。
  const QColor resolved = color.isValid() ? color : QColor(0x16, 0x1A, 0x22);
  const QString escaped = (text.isEmpty() ? QStringLiteral(" ") : text)
                              .toHtmlEscaped()
                              .replace(QLatin1Char('\n'), QStringLiteral("<br/>"));
  document.setHtml(QStringLiteral("<span style=\"color: %1\">%2</span>")
                       .arg(resolved.name(QColor::HexRgb), escaped));
  document.setTextWidth(kTextPinMaxWidth * 2);
  // idealWidth 是实际内容宽（长行在 540 逻辑宽处折行）。
  const QSize contentSize(qRound(document.idealWidth()),
                          qRound(document.size().height()));
  image_ = QImage(contentSize, QImage::Format_ARGB32_Premultiplied);
  image_.fill(Qt::transparent);
  QPainter painter(&image_);
  document.drawContents(&painter);
  painter.end();
  image_.setDevicePixelRatio(dpr_);
  logicalSize_ = QSizeF(image_.width() / dpr_, image_.height() / dpr_);
}

void PinWindow::setupColorContent() {
  const QJsonObject sidecar = readSidecar(imagePath_);
  const QColor color(sidecar.value(QLatin1String("color"))
                         .toString(QLatin1String("#2F6FED")));
  const int size = qMax(32, sidecar.value(QLatin1String("size"))
                                  .toInt(kColorPinDefaultSize));

  image_ = QImage(size, size, QImage::Format_ARGB32_Premultiplied);
  image_.fill(Qt::transparent);
  QPainter painter(&image_);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(QPen(QColor(255, 255, 255, 51), 2));
  painter.setBrush(color.isValid() ? color : QColor(0x2F, 0x6F, 0xED));
  painter.drawRoundedRect(QRectF(2, 2, size - 4.0, size - 4.0), 12, 12);
  const QColor swatch = color.isValid() ? color : QColor(0x2F, 0x6F, 0xED);
  painter.setPen(swatch.lightness() > 160 ? QColor(0x16, 0x1A, 0x22)
                                          : QColor(Qt::white));
  painter.setBrush(Qt::NoBrush);
  painter.setFont(QApplication::font());
  painter.drawText(QRectF(0, 0, size, size), Qt::AlignCenter,
                   swatch.name(QColor::HexRgb).toUpper().mid(1));
  painter.end();
  image_.setDevicePixelRatio(dpr_);
  logicalSize_ = QSizeF(image_.width() / dpr_, image_.height() / dpr_);
}

void PinWindow::applyZoom(bool notify) {
  const int width =
      std::max(1, qRound(logicalSize_.width() * zoom_));
  const int height =
      std::max(1, qRound(logicalSize_.height() * zoom_));
  resize(width, height);
  update();
  if (notify) emit changed();
}

void PinWindow::setAlpha(int alpha) {
  alpha_ = std::clamp(alpha, core::kAlphaMin, core::kAlphaMax);
  setWindowOpacity(alpha_ / 255.0);
}

void PinWindow::setPassThrough(bool enabled) {
  passThrough_ = enabled;
  if (!overlayMode_) platform::setClickThrough(this, enabled);
}

void PinWindow::setOverlayMode(bool active) {
  if (overlayMode_ == active) return;
  overlayMode_ = active;
  platform::setPinRaisedAboveOverlay(this, active);
  platform::setClickThrough(this, active ? true : passThrough_);
}

void PinWindow::setGroupId(const QString& groupId) {
  if (groupId_ == groupId) return;
  groupId_ = groupId;
  emit groupChanged();
}

void PinWindow::setGroupSource(std::function<QStringList()> source) {
  groupSource_ = std::move(source);
}

void PinWindow::setNotify(std::function<void(const QString&)> notify) {
  notify_ = std::move(notify);
}

bool PinWindow::loadAnnotations() {
  QFile file(annotationsPath());
  if (!file.exists()) return false;
  if (!file.open(QIODevice::ReadOnly)) return false;
  core::AnnotationModel loaded;
  if (!core::annotationFromJson(
          QJsonDocument::fromJson(file.readAll()), &loaded)) {
    return false;
  }
  annotations_ = std::move(loaded);
  refreshDisplayImage();
  return true;
}

QString PinWindow::annotationsPath() const {
  return imagePath_ + QStringLiteral(".annotations.json");
}

void PinWindow::refreshDisplayImage() {
  displayImage_ = annotations_.empty()
                      ? QImage()
                      : annotations_.flatten(image_);
  update();
}

void PinWindow::saveAnnotations() {
  if (annotations_.empty()) {
    QFile::remove(annotationsPath());
    return;
  }
  writeSidecar(annotationsPath(),
               core::annotationToJson(annotations_).object());
}

void PinWindow::positionToolbar() {
  if (!toolbar_) return;
  toolbar_->adjustSize();
  const int toolbarHeight = toolbar_->height();

  const QPoint globalBottom = mapToGlobal(QPoint(0, height()));
  QScreen* screen = QGuiApplication::screenAt(globalBottom);
  if (!screen) screen = QGuiApplication::primaryScreen();
  bool fitsBelow = true;
  if (screen) {
    fitsBelow = globalBottom.y() + toolbarHeight + 8 <= screen->geometry().bottom();
  }
  toolbar_->move(0, fitsBelow ? height() : -toolbarHeight);
  toolbar_->raise();
}

void PinWindow::setEditMode(bool enabled) {
  if (editMode_ == enabled) return;
  editMode_ = enabled;
  drawing_ = false;

  if (enabled) {
    passThroughBeforeEdit_ = passThrough_;
    if (passThrough_) setPassThrough(false);
    if (!toolbar_) {
      toolbar_ = new ToolbarWidget(ToolbarWidget::Pin, this);
      connect(toolbar_, &ToolbarWidget::toolChanged, this,
              [this](int tool) { activeTool_ = tool; });
      connect(toolbar_, &ToolbarWidget::colorChanged, this,
              [this](int index) { activeColorIndex_ = index; });
      connect(toolbar_, &ToolbarWidget::widthChanged, this,
              [this](int index) { activeWidthIndex_ = index; });
      connect(toolbar_, &ToolbarWidget::actionTriggered, this,
              [this](int action) {
                if (action == ToolbarWidget::kUndo) {
                  annotations_.undo();
                  refreshDisplayImage();
                  saveAnnotations();
                } else if (action == ToolbarWidget::kDone) {
                  setEditMode(false);
                }
              });
    }
    toolbar_->show();
    positionToolbar();
  } else {
    if (toolbar_) toolbar_->hide();
    if (passThroughBeforeEdit_ && !passThrough_) setPassThrough(true);
    saveAnnotations();
  }
  update();
}

core::PinSession PinWindow::session() const {
  core::PinSession state;
  state.imagePath = imagePath_;
  state.position = pos();
  state.zoom = zoom_;
  state.alpha = alpha_;
  state.passThrough = passThrough_;
  state.dpr = dpr_;
  state.kind = kind_;
  state.groupId = groupId_;
  return state;
}

void PinWindow::showEvent(QShowEvent* event) {
  QWidget::showEvent(event);
  if (nativeConfigured_) return;
  nativeConfigured_ = true;
  platform::configurePinWindow(this);
  platform::setClickThrough(this, passThrough_);
}

void PinWindow::paintEvent(QPaintEvent*) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::SmoothPixmapTransform, zoom_ != 1.0);
  const QRectF target(0, 0, width(), height());

  if (kind_ == core::PinKind::Animated) {
    QMovie* movie = findChild<QMovie*>();
    const QImage frame = (movie && movie->isValid()) ? movie->currentImage()
                                                     : image_;
    if (frame.isNull()) {
      painter.drawImage(target, image_);
    } else {
      painter.drawImage(target, frame);
    }
  } else {
    painter.drawImage(target, exportImage());
  }

  // 正在绘制中的笔迹：与 overlay 一致的实时预览。
  if (drawing_ && !liveShape_.points.isEmpty()) {
    const QImage& base = displayImage_.isNull() ? image_ : displayImage_;
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    const qreal sx = target.width() / qMax(1, base.width());
    const qreal sy = target.height() / qMax(1, base.height());
    painter.setTransform(QTransform(sx, 0, 0, sy, 0, 0));
    core::paintShape(painter, liveShape_);
    painter.restore();
  }

  const core::PinAppearance look = core::pinAppearance();
  if (!look.border || width() < 4 || height() < 4) return;

  // 内容区内侧画 1px 黑 + 1px 白/强调色，不改变窗口几何、不裁切图片。
  painter.setRenderHint(QPainter::Antialiasing, false);
  const QRectF outer(0.5, 0.5, width() - 1.0, height() - 1.0);
  const QRectF inner(1.5, 1.5, width() - 3.0, height() - 3.0);
  painter.setPen(QPen(QColor(Qt::black), 1.0));
  painter.drawRect(outer);
  painter.setPen(QPen(hovered_ ? look.accent : QColor(Qt::white), 1.0));
  painter.drawRect(inner);
}

void PinWindow::enterEvent(QEnterEvent* event) {
  QWidget::enterEvent(event);
  if (hovered_) return;
  hovered_ = true;
  update();
}

void PinWindow::leaveEvent(QEvent* event) {
  QWidget::leaveEvent(event);
  if (!hovered_) return;
  hovered_ = false;
  update();
}

QPoint PinWindow::toImagePoint(const QPoint& widgetPosition) const {
  // 窗口逻辑尺寸 = 图像设备像素 / dpr × zoom，反推回图像像素坐标。
  const qreal imageX = widgetPosition.x() * dpr_ / zoom_;
  const qreal imageY = widgetPosition.y() * dpr_ / zoom_;
  return QPoint(qBound(0, qRound(imageX), image_.width() - 1),
                qBound(0, qRound(imageY), image_.height() - 1));
}

void PinWindow::mousePressEvent(QMouseEvent* event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  // 标注模式下选中了工具 → 开始绘制；未选工具 → 维持拖动移动。
  if (editMode_ && activeTool_ >= 0) {
    drawing_ = true;
    liveShape_ = core::Shape{};
    liveShape_.type = static_cast<core::ShapeType>(activeTool_);
    liveShape_.color = core::paletteColor(activeColorIndex_);
    liveShape_.width = core::kLineWidths[activeWidthIndex_];
    liveShape_.scale = dpr_;
    liveShape_.points.push_back(toImagePoint(event->pos()));
    update();
    event->accept();
    return;
  }

  dragging_ = true;
  dragOffset_ = event->globalPosition().toPoint() - pos();
  raise();
  event->accept();
}

void PinWindow::mouseMoveEvent(QMouseEvent* event) {
  if (!hovered_) {
    hovered_ = true;
    update();
  }

  if (drawing_) {
    const QPoint imagePoint = toImagePoint(event->pos());
    switch (liveShape_.type) {
      case core::ShapeType::Rect:
      case core::ShapeType::Ellipse:
      case core::ShapeType::Arrow:
        // 两点形状：起点 + 当前点。
        if (liveShape_.points.size() >= 2) liveShape_.points.removeLast();
        liveShape_.points.push_back(imagePoint);
        break;
      case core::ShapeType::Pen:
      case core::ShapeType::Marker:
      case core::ShapeType::Mosaic:
        liveShape_.points.push_back(imagePoint);
        break;
    }
    update();
    event->accept();
    return;
  }

  if (!dragging_) {
    QWidget::mouseMoveEvent(event);
    return;
  }
  move(event->globalPosition().toPoint() - dragOffset_);
  if (toolbar_ && toolbar_->isVisible()) positionToolbar();
  event->accept();
}

void PinWindow::mouseReleaseEvent(QMouseEvent* event) {
  if (drawing_ && event->button() == Qt::LeftButton) {
    drawing_ = false;
    annotations_.addShape(liveShape_);
    refreshDisplayImage();
    saveAnnotations();
    event->accept();
    return;
  }
  if (dragging_ && event->button() == Qt::LeftButton) {
    dragging_ = false;
    emit changed();
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

void PinWindow::mouseDoubleClickEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && !editMode_) {
    close();
    event->accept();
    return;
  }
  QWidget::mouseDoubleClickEvent(event);
}

void PinWindow::wheelEvent(QWheelEvent* event) {
  const int delta = event->angleDelta().y();
  if (delta == 0) {
    event->ignore();
    return;
  }

  const Qt::KeyboardModifiers modifiers = event->modifiers();
  if (modifiers.testFlag(Qt::ControlModifier) ||
      modifiers.testFlag(Qt::MetaModifier)) {
    setAlpha(alpha_ + (delta > 0 ? kAlphaStep : -kAlphaStep));
  } else {
    zoom_ = std::clamp(delta > 0 ? zoom_ * kZoomStep : zoom_ / kZoomStep,
                       core::kZoomMin, core::kZoomMax);
    applyZoom(false);
  }
  emit changed();
  event->accept();
}

void PinWindow::contextMenuEvent(QContextMenuEvent* event) {
  QMenu menu(this);

  if (editMode_) {
    QAction* done = menu.addAction(QObject::tr("完成标注"));
    QAction* undo = menu.addAction(QObject::tr("撤销上一笔"));
    QAction* chosen = menu.exec(event->globalPos());
    if (chosen == done) {
      setEditMode(false);
    } else if (chosen == undo) {
      annotations_.undo();
      refreshDisplayImage();
      saveAnnotations();
    }
    return;
  }

  QAction* pass = menu.addAction(QObject::tr("鼠标穿透"));
  pass->setCheckable(true);
  pass->setChecked(passThrough_);
  QAction* annotate = nullptr;
  if (kind_ == core::PinKind::Image) {
    annotate = menu.addAction(QObject::tr("标注"));
  }
  QAction* editContent = nullptr;
  if (kind_ == core::PinKind::Text) {
    editContent = menu.addAction(QObject::tr("编辑文本"));
  }
  QAction* changeColor = nullptr;
  if (kind_ == core::PinKind::Color) {
    changeColor = menu.addAction(QObject::tr("更换颜色"));
  }
  menu.addSeparator();
  QAction* copy = menu.addAction(QObject::tr("复制"));
  QAction* ocr = nullptr;
  if (kind_ == core::PinKind::Image) {
    ocr = menu.addAction(QObject::tr("复制文字 (OCR)"));
  }
  QAction* save = menu.addAction(QObject::tr("另存为…"));

  // 分组子菜单（候选列表由 PinManager 注入，避免反向依赖）。
  QMenu* groupMenu = menu.addMenu(QObject::tr("分组"));
  QAction* newGroup = groupMenu->addAction(QObject::tr("新建分组…"));
  QAction* leaveGroup = nullptr;
  if (!groupId_.isEmpty()) {
    groupMenu->addSeparator();
    leaveGroup = groupMenu->addAction(QObject::tr("移出分组"));
  }
  if (groupSource_) {
    const QStringList groups = groupSource_();
    if (!groups.isEmpty()) groupMenu->addSeparator();
    for (const QString& group : groups) {
      if (group == groupId_) continue;
      QAction* pick = groupMenu->addAction(group);
      connect(pick, &QAction::triggered, this, [this, group] {
        setGroupId(group);
      });
    }
  }
  QAction* solo = menu.addAction(QObject::tr("单独显示"));
  QAction* bindDesktop = menu.addAction(QObject::tr("仅在当前桌面显示"));
  bindDesktop->setCheckable(true);
  bindDesktop->setChecked(desktopBound_);

  menu.addSeparator();
  QAction* closeAction = menu.addAction(QObject::tr("关闭"));
  closeAction->setShortcut(QKeySequence(Qt::Key_Backspace));

  QAction* chosen = menu.exec(event->globalPos());
  if (chosen == pass) {
    setPassThrough(!passThrough_);
    emit changed();
  } else if (chosen == annotate) {
    setEditMode(true);
  } else if (chosen == editContent) {
    const QJsonObject sidecar = readSidecar(imagePath_);
    const QString text = QInputDialog::getMultiLineText(
        this, QObject::tr("编辑文本"), QObject::tr("文本内容："),
        sidecar.value(QLatin1String("text")).toString());
    if (text.isEmpty() && sidecar.value(QLatin1String("text")).toString().isEmpty()) {
      return;
    }
    QJsonObject updated = sidecar;
    updated.insert(QLatin1String("text"), text);
    writeSidecar(imagePath_, updated);
    setupTextContent();
    applyZoom(false);
    refreshDisplayImage();
  } else if (chosen == changeColor) {
    const QJsonObject sidecar = readSidecar(imagePath_);
    const QColor picked = QColorDialog::getColor(
        QColor(sidecar.value(QLatin1String("color")).toString()), this,
        QObject::tr("更换颜色"));
    if (!picked.isValid()) return;
    QJsonObject updated = sidecar;
    updated.insert(QLatin1String("color"), picked.name(QColor::HexRgb));
    writeSidecar(imagePath_, updated);
    setupColorContent();
    applyZoom(false);
    refreshDisplayImage();
  } else if (chosen == copy) {
    copyToClipboard();
  } else if (chosen == ocr) {
    ocrCopy();
  } else if (chosen == save) {
    saveAs();
  } else if (chosen == newGroup) {
    const QString name = QInputDialog::getText(
        this, QObject::tr("新建分组"), QObject::tr("分组名称："));
    if (!name.trimmed().isEmpty()) setGroupId(name.trimmed());
  } else if (chosen == leaveGroup) {
    setGroupId(QString());
  } else if (chosen == solo) {
    emit soloRequested(this);
  } else if (chosen == bindDesktop) {
    desktopBound_ = !desktopBound_;
    platform::setPinBoundToDesktop(this, desktopBound_);
  } else if (chosen == closeAction) {
    close();
  }
}

void PinWindow::closeEvent(QCloseEvent* event) {
  emit closed(this);
  QWidget::closeEvent(event);
}

void PinWindow::copyToClipboard() {
  QApplication::clipboard()->setImage(exportImage());
}

void PinWindow::ocrCopy() {
  QPointer<PinWindow> guard(this);
  platform::ocrText(exportImage(), [guard, this](const QString& text,
                                                 const QString& error) {
    if (!guard) return;
    if (error.isEmpty() && !text.isEmpty()) {
      QApplication::clipboard()->setText(text);
      if (notify_) notify_(QObject::tr("已复制识别的文字（%1 个字符）。").arg(text.size()));
      return;
    }
    if (notify_) {
      notify_(error.isEmpty() ? QObject::tr("没有识别到文字。") : error);
    }
  });
}

void PinWindow::saveAs() {
  QString suggested = QFileInfo(imagePath_).fileName();
  if (kind_ == core::PinKind::Text || kind_ == core::PinKind::Color) {
    suggested = QStringLiteral("StickySnap.png");
  } else if (suggested.endsWith(QLatin1String(".annotations.json"))) {
    suggested = suggested.left(suggested.size() -
                               QStringLiteral(".annotations.json").size());
  }
  if (suggested.isEmpty()) suggested = QStringLiteral("StickySnap.png");

  const QString path = QFileDialog::getSaveFileName(
      this, QObject::tr("另存为"),
      QFileInfo(imagePath_).absolutePath() + QLatin1Char('/') + suggested,
      QObject::tr("PNG 图片 (*.png);;JPEG 图片 (*.jpg);;所有文件 (*)"));
  if (path.isEmpty()) return;

  QString target = path;
  if (QFileInfo(target).suffix().isEmpty()) target += QStringLiteral(".png");
  if (!exportImage().save(target)) {
    qWarning("StickySnap: failed to save image to %s",
             qPrintable(target));
  }
}

}  // namespace sticky::ui
