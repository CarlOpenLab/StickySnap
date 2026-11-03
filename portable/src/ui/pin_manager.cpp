#include "ui/pin_manager.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
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

PinWindow* PinManager::createPin(const QImage& image,
                                 const QPoint& globalPosition) {
  if (image.isNull()) return nullptr;
  if (pins_.size() >= kMaxPins) {
    qWarning("StickySnap: pin limit reached");
    return nullptr;
  }

  const QString path = newImagePath();
  if (path.isEmpty() || !image.save(path, "PNG")) {
    qWarning("StickySnap: failed to persist pin image");
  }

  core::PinSession state;
  state.imagePath = path;
  state.position = clampPosition(globalPosition);
  state.zoom = 1.0;
  state.alpha = core::kAlphaMax;
  state.passThrough = false;
  state.dpr = image.devicePixelRatio() > 0 ? image.devicePixelRatio() : 1.0;

  auto* pin = new PinWindow(image, path, state);
  addPinWindow(pin);
  markDirty();
  return pin;
}

void PinManager::addPinWindow(PinWindow* pin) {
  pins_.push_back(pin);
  connect(pin, &PinWindow::changed, this, &PinManager::markDirty);
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
    QImage image(session.imagePath);
    if (image.isNull()) continue;  // image file moved or deleted

    image.setDevicePixelRatio(session.dpr);
    core::PinSession state = session;
    state.position = clampPosition(session.position);

    auto* pin = new PinWindow(image, session.imagePath, state);
    addPinWindow(pin);
    ++restored;
  }
  if (restored > 0) qInfo("StickySnap: restored %lld pins",
                          static_cast<long long>(restored));
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

QString PinManager::newImagePath() const {
  const QString directory = platform::pinImageDirectory();
  if (directory.isEmpty()) return {};

  const QString stamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
  for (int index = 0; index < 1000; ++index) {
    const QString candidate =
        directory + QStringLiteral("/%1-%2.png").arg(stamp).arg(index);
    if (!QFileInfo::exists(candidate)) return candidate;
  }
  return {};
}

}  // namespace sticky::ui
