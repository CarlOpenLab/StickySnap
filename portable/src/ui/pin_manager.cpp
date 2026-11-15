#include "ui/pin_manager.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScreen>
#include <QStandardPaths>

#include <algorithm>

#include "core/constants.h"
#include "core/session_store.h"
#include "platform/platform.h"

namespace sticky::ui {
namespace {

constexpr qsizetype kMaxPins = 200;
constexpr int kSaveDelayMs = 800;

}  // namespace

PinManager::PinManager(QObject* parent) : QObject(parent) {
  saveTimer_.setSingleShot(true);
  connect(&saveTimer_, &QTimer::timeout, this, &PinManager::saveNow);
}

PinManager::~PinManager() {
  saveTimer_.stop();
  saveNow();
}

bool PinManager::atCapacity() {
  if (pins_.size() < kMaxPins) return false;
  qWarning("StickySnap: pin limit reached");
  emit limitReached();
  return true;
}

PinWindow* PinManager::createPin(const QImage& image,
                                 const QPoint& globalPosition) {
  if (image.isNull() || atCapacity()) return nullptr;

  const QString path = newImagePath(QStringLiteral("png"));
  if (path.isEmpty() || !image.save(path, "PNG")) {
    qWarning("StickySnap: failed to persist pin image");
    emit persistFailed();
  }

  core::PinSession state;
  state.imagePath = path;
  state.position = clampPosition(globalPosition);
  state.zoom = 1.0;
  state.alpha = core::kAlphaMax;
  state.passThrough = false;
  state.dpr = image.devicePixelRatio() > 0 ? image.devicePixelRatio() : 1.0;
  state.kind = core::PinKind::Image;

  auto* pin = new PinWindow(image, path, state);
  addPinWindow(pin);
  markDirty();
  return pin;
}

PinWindow* PinManager::createAnimatedPin(const QByteArray& bytes,
                                         const QString& suffix,
                                         const QPoint& globalPosition) {
  if (bytes.isEmpty() || suffix.isEmpty() || atCapacity()) return nullptr;

  const QString path = newImagePath(suffix);
  if (path.isEmpty() || !writeBytes(path, bytes)) {
    qWarning("StickySnap: failed to persist animated pin");
    emit persistFailed();
    return nullptr;
  }

  core::PinSession state;
  state.imagePath = path;
  state.position = clampPosition(globalPosition);
  state.kind = core::PinKind::Animated;

  auto* pin = new PinWindow(QImage(), path, state);
  addPinWindow(pin);
  markDirty();
  return pin;
}

PinWindow* PinManager::createTextPin(const QString& text,
                                     const QPoint& globalPosition) {
  if (text.trimmed().isEmpty() || atCapacity()) return nullptr;

  const QString path = newImagePath(QStringLiteral("text.json"));
  if (path.isEmpty()) return nullptr;

  QJsonObject sidecar;
  sidecar.insert(QLatin1String("text"), text);
  sidecar.insert(QLatin1String("fontSize"), 24);
  if (!writeJson(path, sidecar)) {
    qWarning("StickySnap: failed to persist text pin");
    emit persistFailed();
    return nullptr;
  }

  core::PinSession state;
  state.imagePath = path;
  state.position = clampPosition(globalPosition);
  state.kind = core::PinKind::Text;

  auto* pin = new PinWindow(QImage(), path, state);
  addPinWindow(pin);
  markDirty();
  return pin;
}

PinWindow* PinManager::createColorPin(const QColor& color,
                                      const QPoint& globalPosition) {
  if (!color.isValid() || atCapacity()) return nullptr;

  const QString path = newImagePath(QStringLiteral("color.json"));
  if (path.isEmpty()) return nullptr;

  QJsonObject sidecar;
  sidecar.insert(QLatin1String("color"), color.name(QColor::HexRgb));
  sidecar.insert(QLatin1String("size"), 96);
  if (!writeJson(path, sidecar)) {
    qWarning("StickySnap: failed to persist color pin");
    emit persistFailed();
    return nullptr;
  }

  core::PinSession state;
  state.imagePath = path;
  state.position = clampPosition(globalPosition);
  state.kind = core::PinKind::Color;

  auto* pin = new PinWindow(QImage(), path, state);
  addPinWindow(pin);
  markDirty();
  return pin;
}

bool PinManager::writeBytes(const QString& path, const QByteArray& bytes) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  return file.write(bytes) == bytes.size();
}

bool PinManager::writeJson(const QString& path, const QJsonObject& object) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly)) return false;
  return file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)) > 0;
}

void PinManager::setNotifySink(std::function<void(const QString&)> sink) {
  notifySink_ = std::move(sink);
  for (PinWindow* pin : pins_) pin->setNotify(notifySink_);
}

void PinManager::addPinWindow(PinWindow* pin) {
  pins_.push_back(pin);
  pin->setGroupSource([this] { return groups(); });
  if (notifySink_) pin->setNotify(notifySink_);
  connect(pin, &PinWindow::changed, this, &PinManager::markDirty);
  connect(pin, &PinWindow::groupChanged, this, &PinManager::markDirty);
  connect(pin, &PinWindow::soloRequested, this,
          [this](PinWindow* solo) { setSolo(solo); });
  connect(pin, &PinWindow::closed, this, [this](PinWindow* closed) {
    pins_.removeAll(closed);
    closed->deleteLater();
    markDirty();
    emit pinsChanged();
  });
  pin->show();
  if (overlayActive_) pin->setOverlayMode(true);
  emit pinsChanged();
}

void PinManager::restore() {
  core::SessionStore store(platform::sessionIndexPath());
  QString error;
  const auto sessions = store.load(&error);
  if (!error.isEmpty() && sessions.isEmpty()) return;

  qsizetype restored = 0;
  for (const auto& session : sessions) {
    if (restored >= kMaxPins) break;
    core::PinSession state = session;
    state.position = clampPosition(session.position);

    if (session.kind == core::PinKind::Text ||
        session.kind == core::PinKind::Color) {
      // 文本/颜色贴图的 imagePath 指向侧车 JSON，内容缺失即丢弃。
      if (!QFileInfo::exists(session.imagePath)) continue;
      auto* pin = new PinWindow(QImage(), session.imagePath, state);
      addPinWindow(pin);
      ++restored;
      continue;
    }

    QImage image(session.imagePath);
    if (image.isNull()) continue;  // image file moved or deleted

    image.setDevicePixelRatio(session.dpr);
    auto* pin = new PinWindow(image, session.imagePath, state);
    pin->loadAnnotations();
    addPinWindow(pin);
    ++restored;
  }
  if (restored > 0) qInfo("StickySnap: restored %lld pins",
                          static_cast<long long>(restored));
}

QStringList PinManager::groups() const {
  QStringList names;
  for (PinWindow* pin : pins_) {
    const QString& group = pin->groupId();
    if (!group.isEmpty() && !names.contains(group)) names.append(group);
  }
  return names;
}

void PinManager::setGroupVisible(const QString& group, bool visible) {
  for (PinWindow* pin : pins_) {
    if (pin->groupId() == group) pin->setVisible(visible);
  }
}

void PinManager::closeGroup(const QString& group) {
  const QVector<PinWindow*> copy = pins_;
  for (PinWindow* pin : copy) {
    if (pin->groupId() == group) pin->close();
  }
}

void PinManager::setSolo(PinWindow* soloPin) {
  for (PinWindow* pin : pins_) {
    pin->setVisible(pin == soloPin);
  }
  if (soloPin) {
    soloPin->setVisible(true);
    soloPin->raise();
  }
}

void PinManager::showAllPins() {
  for (PinWindow* pin : pins_) pin->setVisible(true);
}

void PinManager::hideAllPins() {
  for (PinWindow* pin : pins_) pin->setVisible(false);
}

void PinManager::closePin(PinWindow* pin) {
  if (!pin) return;
  pin->close();
}

void PinManager::closeAll() {
  const QVector<PinWindow*> copy = pins_;
  for (PinWindow* pin : copy) pin->close();
  pins_.clear();
  saveNow();
}

void PinManager::setOverlayActive(bool active) {
  overlayActive_ = active;
  for (PinWindow* pin : pins_) pin->setOverlayMode(active);
}

void PinManager::markDirty() {
  saveTimer_.stop();
  saveTimer_.start(kSaveDelayMs);
}

void PinManager::saveNow() {
  saveTimer_.stop();
  QVector<core::PinSession> sessions;
  sessions.reserve(pins_.size());
  for (PinWindow* pin : pins_) sessions.push_back(pin->session());

  core::SessionStore store(platform::sessionIndexPath());
  QString error;
  if (!store.save(sessions, &error)) {
    qWarning("StickySnap: failed to save session: %s", qPrintable(error));
  }
}

QRect PinManager::virtualDesktop() const {
  QRect desktop;
  for (QScreen* screen : QGuiApplication::screens()) {
    desktop = desktop.united(screen->geometry());
  }
  return desktop;
}

QPoint PinManager::clampPosition(const QPoint& position) const {
  const QRect desktop = virtualDesktop();
  if (desktop.isNull()) return position;
  const int maxX = desktop.right() - 40;
  const int maxY = desktop.bottom() - 40;
  return QPoint(std::clamp(position.x(), desktop.left(), std::max(desktop.left(), maxX)),
                std::clamp(position.y(), desktop.top(), std::max(desktop.top(), maxY)));
}

QString PinManager::newImagePath(const QString& suffix) const {
  const QString directory = platform::pinImageDirectory();
  if (directory.isEmpty()) return {};

  const QString stamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
  for (int index = 0; index < 1000; ++index) {
    const QString candidate =
        directory + QStringLiteral("/%1-%2.%3").arg(stamp).arg(index).arg(suffix);
    if (!QFileInfo::exists(candidate)) return candidate;
  }
  return {};
}

}  // namespace sticky::ui
