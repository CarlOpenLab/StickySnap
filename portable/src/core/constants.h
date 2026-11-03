#pragma once

namespace sticky::core {

// Bounds shared by the pin model and the persisted session format.
inline constexpr double kZoomMin = 0.1;
inline constexpr double kZoomMax = 8.0;
inline constexpr int kAlphaMin = 32;
inline constexpr int kAlphaMax = 255;

// A drag must cover at least this many pixels to count as a selection.
inline constexpr int kMinSelectionPx = 3;

}  // namespace sticky::core
