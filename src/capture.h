#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>

// 整个虚拟屏幕的一次性捕获。
// 坐标一律使用物理像素（进程为 Per-Monitor-V2 DPI aware）。
class CapturedScreen {
 public:
  CapturedScreen() = default;
  ~CapturedScreen() { Free(); }
  CapturedScreen(const CapturedScreen&) = delete;
  CapturedScreen& operator=(const CapturedScreen&) = delete;

  bool Capture();
  void Free();
  bool Valid() const { return bitmap_ != nullptr; }

  // 屏幕物理坐标取像素颜色（BGR 的 COLORREF）
  COLORREF PixelAt(int x, int y) const;

  HBITMAP bitmap() const { return bitmap_; }
  HDC mem_dc() const { return mem_dc_; }  // 内部已选中 bitmap_
  const uint8_t* bits() const { return bits_; }
  RECT bounds() const { return bounds_; }  // 虚拟屏幕矩形（物理坐标）
  int width() const { return width_; }
  int height() const { return height_; }

 private:
  HBITMAP bitmap_ = nullptr;
  HDC mem_dc_ = nullptr;
  uint8_t* bits_ = nullptr;  // DIB section 内存（top-down 32bpp）
  int width_ = 0;
  int height_ = 0;
  RECT bounds_{};
};
