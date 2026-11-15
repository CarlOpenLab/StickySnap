#pragma once

#include <QPoint>
#include <QString>
#include <QVector>

namespace sticky::core {

// 贴图内容的类型。image/animated 是真正的图片文件；text/color 是侧车 JSON
// 描述的轻量贴图（imagePath 指向 .json）。
enum class PinKind {
  Image,
  Animated,
  Text,
  Color,
};

QString pinKindToString(PinKind kind);
// 无法识别的字符串回退 fallback（向前兼容未来的新类型）。
PinKind pinKindFromString(const QString& value, PinKind fallback);

// One pinned image as persisted between runs.
struct PinSession {
  QString imagePath;
  QPoint position;
  double zoom = 1.0;
  int alpha = 255;
  bool passThrough = false;
  qreal dpr = 1.0;  // pixels per logical point, for Retina pins
  PinKind kind = PinKind::Image;
  QString groupId;  // 空表示未分组；保存时剔除制表符/换行。
};

// Reads and writes the tab-separated session index.
//
// Line format: imagePath \t x \t y \t zoom \t alpha \t passThrough [\t dpr
// [\t kind [\t groupId]]]. Trailing fields are optional so older readers
// (the Windows native app stops after 6 fields) keep working.
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
