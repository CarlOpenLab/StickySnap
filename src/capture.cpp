#include "capture.h"

bool CapturedScreen::Capture() {
  Free();
  bounds_.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  bounds_.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  width_ = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  height_ = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (width_ <= 0 || height_ <= 0) return false;

  HDC screen = GetDC(nullptr);
  mem_dc_ = CreateCompatibleDC(screen);

  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = width_;
  bi.bmiHeader.biHeight = -height_;  // 负值 = top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  bitmap_ = CreateDIBSection(mem_dc_, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!bitmap_ || !bits) {
    ReleaseDC(nullptr, screen);
    Free();
    return false;
  }
  bits_ = static_cast<uint8_t*>(bits);
  // bitmap_ 保持在 mem_dc_ 中选中，供遮罩/放大镜作 BitBlt 源
  SelectObject(mem_dc_, bitmap_);

  BOOL ok = BitBlt(mem_dc_, 0, 0, width_, height_, screen,
                   bounds_.left, bounds_.top, SRCCOPY | CAPTUREBLT);
  ReleaseDC(nullptr, screen);
  if (!ok) {
    Free();
    return false;
  }
  return true;
}

void CapturedScreen::Free() {
  if (mem_dc_) {
    DeleteDC(mem_dc_);
    mem_dc_ = nullptr;
  }
  if (bitmap_) {
    DeleteObject(bitmap_);
    bitmap_ = nullptr;
  }
  bits_ = nullptr;
  width_ = height_ = 0;
}

COLORREF CapturedScreen::PixelAt(int x, int y) const {
  x -= bounds_.left;
  y -= bounds_.top;
  if (x < 0 || y < 0 || x >= width_ || y >= height_ || !bits_) return 0;
  const uint8_t* p = bits_ + (static_cast<size_t>(y) * width_ + x) * 4;
  return RGB(p[2], p[1], p[0]);  // 内存布局 BGRA
}
