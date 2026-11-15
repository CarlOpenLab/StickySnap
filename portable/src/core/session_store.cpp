#include "core/session_store.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringList>

#include <algorithm>

#include "core/constants.h"

namespace sticky::core {
namespace {

QString clampLine(const QString& line) { return line.trimmed(); }

QString formatZoom(double zoom) { return QString::number(zoom, 'f', 3); }

// 分组名会写进 TSV，去掉分隔符与换行防止串列。
QString sanitizeGroup(QString group) {
  group.remove(QLatin1Char('\t'));
  group.remove(QLatin1Char('\n'));
  group.remove(QLatin1Char('\r'));
  return group.trimmed();
}

}  // namespace

QString pinKindToString(PinKind kind) {
  switch (kind) {
    case PinKind::Animated:
      return QStringLiteral("animated");
    case PinKind::Text:
      return QStringLiteral("text");
    case PinKind::Color:
      return QStringLiteral("color");
    case PinKind::Image:
      break;
  }
  return QStringLiteral("image");
}

PinKind pinKindFromString(const QString& value, PinKind fallback) {
  if (value == QLatin1String("animated")) return PinKind::Animated;
  if (value == QLatin1String("text")) return PinKind::Text;
  if (value == QLatin1String("color")) return PinKind::Color;
  if (value == QLatin1String("image")) return PinKind::Image;
  return fallback;
}

SessionStore::SessionStore(QString indexFilePath)
    : indexPath_(std::move(indexFilePath)) {}

QString SessionStore::serialize(const QVector<PinSession>& pins) {
  QString text;
  for (const auto& pin : pins) {
    text += pin.imagePath;
    text += QLatin1Char('\t');
    text += QString::number(pin.position.x());
    text += QLatin1Char('\t');
    text += QString::number(pin.position.y());
    text += QLatin1Char('\t');
    text += formatZoom(pin.zoom);
    text += QLatin1Char('\t');
    text += QString::number(pin.alpha);
    text += QLatin1Char('\t');
    text += pin.passThrough ? QLatin1Char('1') : QLatin1Char('0');
    text += QLatin1Char('\t');
    text += QString::number(pin.dpr, 'f', 3);
    // 尾随可选字段：旧读取器（原生版 swscanf 读 6 个）自然忽略。
    text += QLatin1Char('\t');
    text += pinKindToString(pin.kind);
    if (!pin.groupId.isEmpty()) {
      text += QLatin1Char('\t');
      text += sanitizeGroup(pin.groupId);
    }
    text += QLatin1Char('\n');
  }
  return text;
}

QVector<PinSession> SessionStore::deserialize(const QString& text) {
  QVector<PinSession> pins;

  // The Windows native writer emits a UTF-8 BOM (ccs=UTF-8 CRT flag), which
  // would otherwise corrupt the first line's imagePath.
  const QString normalized = text.startsWith(QChar(0xFEFF)) ? text.mid(1) : text;

  const auto lines = normalized.split(QLatin1Char('\n'));
  for (const QString& rawLine : lines) {
    const QString line = clampLine(rawLine);
    if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;

    const auto fields = line.split(QLatin1Char('\t'));
    if (fields.size() < 6 || fields.size() > 9) continue;

    bool xOk = false;
    bool yOk = false;
    bool zoomOk = false;
    bool alphaOk = false;
    bool passOk = false;
    if (fields.at(0).isEmpty()) continue;

    const int x = fields.at(1).toInt(&xOk);
    const int y = fields.at(2).toInt(&yOk);
    const double zoom = fields.at(3).toDouble(&zoomOk);
    const int alpha = fields.at(4).toInt(&alphaOk);
    const int pass = fields.at(5).toInt(&passOk);
    if (!xOk || !yOk || !zoomOk || !alphaOk || !passOk) continue;

    PinSession pin;
    pin.imagePath = fields.at(0);
    pin.position = QPoint(x, y);
    pin.zoom = std::clamp(zoom, kZoomMin, kZoomMax);
    pin.alpha = std::clamp(alpha, kAlphaMin, kAlphaMax);
    pin.passThrough = pass != 0;

    // dpr was added after the six-field format; older lines default to 1.
    if (fields.size() >= 7) {
      bool dprOk = false;
      const qreal dpr = fields.at(6).toDouble(&dprOk);
      if (dprOk && dpr > 0.0) {
        pin.dpr = std::clamp(dpr, 0.5, 4.0);
      }
    }

    // kind/group 是 M1 新增的尾随可选字段，缺省回退到 image/未分组。
    if (fields.size() >= 8) {
      pin.kind = pinKindFromString(fields.at(7), PinKind::Image);
    }
    if (fields.size() >= 9) {
      pin.groupId = sanitizeGroup(fields.at(8));
    }

    pins.push_back(std::move(pin));
  }

  return pins;
}

bool SessionStore::save(const QVector<PinSession>& pins,
                        QString* error) const {
  const QFileInfo info(indexPath_);
  if (!QDir().mkpath(info.absolutePath())) {
    if (error) *error = QStringLiteral("cannot create directory: %1")
                            .arg(info.absolutePath());
    return false;
  }

  QSaveFile file(indexPath_);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    if (error) *error = file.errorString();
    return false;
  }

  const QByteArray data = serialize(pins).toUtf8();
  if (file.write(data) != data.size()) {
    if (error) *error = file.errorString();
    file.cancelWriting();
    return false;
  }

  if (!file.commit()) {
    if (error) *error = file.errorString();
    return false;
  }
  return true;
}

QVector<PinSession> SessionStore::load(QString* error) const {
  QFile file(indexPath_);
  if (!file.exists()) {
    if (error) *error = QStringLiteral("session index does not exist: %1")
                            .arg(indexPath_);
    return {};
  }
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (error) *error = file.errorString();
    return {};
  }

  return deserialize(QString::fromUtf8(file.readAll()));
}

}  // namespace sticky::core
