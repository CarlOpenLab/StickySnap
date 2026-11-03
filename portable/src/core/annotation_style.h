#pragma once

#include <QColor>
#include <QRgb>

namespace sticky::core {

// Shared annotation palette and stroke widths (M0 values).
inline constexpr QRgb kPaletteRgb[9] = {
    0xFFE5484D, 0xFFFF8D3A, 0xFFF7C948, 0xFF46A758, 0xFF2F6FED,
    0xFF7A5AF8, 0xFFE36BAA, 0xFF0EA5A5, 0xFFF2F2F2,
};

inline constexpr int kLineWidths[3] = {2, 4, 7};
inline constexpr int kDotDiameters[3] = {5, 8, 12};

inline QColor paletteColor(int index) {
  if (index < 0 || index >= 9) index = 4;
  return QColor::fromRgb(kPaletteRgb[index]);
}

}  // namespace sticky::core
