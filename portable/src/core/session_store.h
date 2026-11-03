#pragma once

#include <QPoint>
#include <QString>
#include <QVector>

namespace sticky::core {

// One pinned image as persisted between runs.
struct PinSession {
  QString imagePath;
  QPoint position;
  double zoom = 1.0;
  int alpha = 255;
  bool passThrough = false;
  qreal dpr = 1.0;  // pixels per logical point, for Retina pins
};

// Reads and writes the tab-separated session index.
//
// Line format: imagePath \t x \t y \t zoom \t alpha \t passThrough
// Unknown and malformed lines are skipped; zoom and alpha are clamped to the
// bounds in constants.h.
class SessionStore {
 public:
  explicit SessionStore(QString indexFilePath);

  bool save(const QVector<PinSession>& pins, QString* error = nullptr) const;
  QVector<PinSession> load(QString* error = nullptr) const;

  static QString serialize(const QVector<PinSession>& pins);
  static QVector<PinSession> deserialize(const QString& text);

 private:
  QString indexPath_;
};

}  // namespace sticky::core
