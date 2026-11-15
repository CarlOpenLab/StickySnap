#pragma once

#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QObject>
#include <QPoint>
#include <QStringList>
#include <QTimer>
#include <QVector>

#include <functional>

#include "ui/pin_window.h"

class QJsonObject;

namespace sticky::ui {

// Owns every pinned window, persists their state, and restores them at startup.
class PinManager : public QObject {
  Q_OBJECT

 public:
  explicit PinManager(QObject* parent = nullptr);
  ~PinManager() override;

  // 静态图片贴图（截图 / 剪贴板）。
  PinWindow* createPin(const QImage& image, const QPoint& globalPosition);
  // GIF/APNG 动图贴图：按原始字节保存，QMovie 播放。
  PinWindow* createAnimatedPin(const QByteArray& bytes, const QString& suffix,
                               const QPoint& globalPosition);
  // 文本 / 颜色贴图：侧车 JSON 存储，PinWindow 内部渲染。
  PinWindow* createTextPin(const QString& text, const QPoint& globalPosition);
  PinWindow* createColorPin(const QColor& color, const QPoint& globalPosition);

  void restore();
  void closePin(PinWindow* pin);
  void closeAll();
  void saveNow();

  // Capture overlay: pins stay visible above the frozen image but ignore input.
  void setOverlayActive(bool active);
  bool overlayActive() const { return overlayActive_; }

  const QVector<PinWindow*>& pins() const { return pins_; }

  // 贴图分组与批量操作。
  QStringList groups() const;
  void setGroupVisible(const QString& group, bool visible);
  void closeGroup(const QString& group);
  void setSolo(PinWindow* soloPin);  // 单独显示：其余贴图全部隐藏
  void showAllPins();
  void hideAllPins();

  // 注入到每个贴图窗的消息回调（托盘通知）。
  void setNotifySink(std::function<void(const QString&)> sink);

 signals:
  void pinsChanged();
  void limitReached();
  void persistFailed();

 private:
  bool atCapacity();
  static bool writeBytes(const QString& path, const QByteArray& bytes);
  static bool writeJson(const QString& path, const QJsonObject& object);
  void addPinWindow(PinWindow* pin);
  void markDirty();
  QRect virtualDesktop() const;
  QPoint clampPosition(const QPoint& position) const;
  QString newImagePath(const QString& suffix) const;

  QVector<PinWindow*> pins_;
  QTimer saveTimer_;
  bool overlayActive_ = false;
  std::function<void(const QString&)> notifySink_;
};

}  // namespace sticky::ui
