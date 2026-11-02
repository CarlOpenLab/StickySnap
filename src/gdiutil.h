#pragma once
// GDI+ 封装：PNG 读写（header-only）。调用前需已 GdiplusStartup。
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <gdiplus.h>
#include <cwctype>
#include <memory>
#include <string>

namespace gdiutil {

inline const CLSID& EncoderPng() {
  static CLSID c = {0x557CF406, 0x1A04, 0x11D3, {0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E}};
  return c;
}

inline const CLSID& EncoderJpeg() {
  static CLSID c = {0x557CF401, 0x1A04, 0x11D3, {0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E}};
  return c;
}

// 按扩展名选编码器保存（.png / .jpg），成功返回 true
inline bool SaveImage(HBITMAP hbmp, const std::wstring& path) {
  if (!hbmp || path.empty()) return false;
  std::unique_ptr<Gdiplus::Bitmap> bmp(Gdiplus::Bitmap::FromHBITMAP(hbmp, nullptr));
  if (!bmp) return false;
  std::wstring lower = path;
  for (auto& c : lower) c = static_cast<wchar_t>(towlower(c));
  const CLSID& cls = (lower.size() >= 4 && lower.compare(lower.size() - 4, 4, L".jpg") == 0)
                         ? EncoderJpeg() : EncoderPng();
  return bmp->Save(path.c_str(), &cls, nullptr) == Gdiplus::Ok;
}

// 从图片文件加载 HBITMAP（GDI+ 支持的格式均可）
inline HBITMAP LoadImageFile(const std::wstring& path) {
  std::unique_ptr<Gdiplus::Bitmap> bmp(Gdiplus::Bitmap::FromFile(path.c_str()));
  if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok) return nullptr;
  HBITMAP out = nullptr;
  if (bmp->GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &out) != Gdiplus::Ok) return nullptr;
  return out;
}

}  // namespace gdiutil
