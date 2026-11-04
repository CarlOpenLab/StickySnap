#include "ui/tray_icon.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPixmap>

namespace sticky::ui {

QIcon makeTrayIcon() {
#ifdef Q_OS_MACOS
  QPixmap pixmap(44, 44);
  pixmap.fill(Qt::transparent);
  pixmap.setDevicePixelRatio(2.0);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);

  // Snapshot frame, matching the layered logo mark.
  painter.setPen(QPen(Qt::black, 2.2, Qt::SolidLine, Qt::RoundCap,
                      Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);
  painter.drawRoundedRect(QRectF(3.5, 8.0, 14.5, 11.0), 2.6, 2.6);

  // Mountain inside the frame.
  QPainterPath mountain;
  mountain.moveTo(6.4, 16.6);
  mountain.lineTo(9.6, 12.9);
  mountain.lineTo(12.0, 15.3);
  mountain.lineTo(13.8, 13.7);
  mountain.lineTo(16.4, 16.6);
  mountain.closeSubpath();
  painter.setPen(Qt::NoPen);
  painter.setBrush(Qt::black);
  painter.drawPath(mountain);

  // Two sparkle strokes at the top right.
  painter.setPen(QPen(Qt::black, 2.0, Qt::SolidLine, Qt::RoundCap));
  painter.drawLine(QPointF(15.8, 3.0), QPointF(16.6, 5.4));
  painter.drawLine(QPointF(19.2, 6.2), QPointF(20.1, 8.6));

  QIcon icon(pixmap);
  icon.setIsMask(true);
  return icon;
#else
  return QIcon(QStringLiteral(":/icons/app-icon.png"));
#endif
}

}  // namespace sticky::ui
