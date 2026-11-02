#include "clipboard.h"

#include <objbase.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cstring>
#include <memory>
#include <vector>

namespace {

UINT PngFormat() {
  static UINT f = RegisterClipboardFormatW(L"PNG");
  return f;
}

// HBITMAP → 32bpp top-down 像素缓冲（DDB/DIB 通吃）
bool ExtractPixels(HBITMAP hbmp, std::vector<uint8_t>* out, int* w, int* h) {
  BITMAP bm{};
  if (GetObjectW(hbmp, sizeof(bm), &bm) == 0) return false;
  *w = bm.bmWidth;
  *h = bm.bmHeight;
  if (*w <= 0 || *h <= 0) return false;

  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = *w;
  bi.bmiHeader.biHeight = -*h;  // top-down
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  out->resize(static_cast<size_t>(*w) * *h * 4);
  HDC dc = GetDC(nullptr);
  int rows = GetDIBits(dc, hbmp, 0, *h, out->data(), &bi, DIB_RGB_COLORS);
  ReleaseDC(nullptr, dc);
  return rows == *h;
}

// HBITMAP → PNG 内存块（HGLOBAL），供 SetClipboardData
bool SavePngToHglobal(HBITMAP hbmp, HGLOBAL* out) {
  *out = nullptr;
  static const CLSID kPng = {0x557CF406, 0x1A04, 0x11D3,
                             {0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E}};
  std::unique_ptr<Gdiplus::Bitmap> bmp(Gdiplus::Bitmap::FromHBITMAP(hbmp, nullptr));
  if (!bmp) return false;
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, 0);
  if (!mem) return false;
  IStream* stream = nullptr;
  if (FAILED(CreateStreamOnHGlobal(mem, FALSE, &stream))) {
    GlobalFree(mem);
    return false;
  }
  const bool ok = bmp->Save(stream, &kPng, nullptr) == Gdiplus::Ok;
  stream->Release();  // fDeleteOnRelease=FALSE，mem 不释放
  if (!ok) {
    GlobalFree(mem);
    return false;
  }
  *out = mem;
  return true;
}

}  // namespace

bool clipboard::CopyImage(HBITMAP hbmp) {
  std::vector<uint8_t> px;
  int w = 0, h = 0;
  if (!ExtractPixels(hbmp, &px, &w, &h)) return false;

  BITMAPINFOHEADER bih{};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = w;
  bih.biHeight = -h;  // top-down 的 CF_DIB（主流软件均接受）
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = static_cast<DWORD>(px.size());

  HGLOBAL dib = GlobalAlloc(GMEM_MOVEABLE, sizeof(bih) + px.size());
  if (!dib) return false;
  {
    void* p = GlobalLock(dib);
    memcpy(p, &bih, sizeof(bih));
    memcpy(static_cast<uint8_t*>(p) + sizeof(bih), px.data(), px.size());
    GlobalUnlock(dib);
  }

  HGLOBAL png = nullptr;
  SavePngToHglobal(hbmp, &png);

  if (!OpenClipboard(nullptr)) {
    GlobalFree(dib);
    if (png) GlobalFree(png);
    return false;
  }
  EmptyClipboard();
  SetClipboardData(CF_DIB, dib);  // 剪贴板接管内存
  if (png) SetClipboardData(PngFormat(), png);
  CloseClipboard();
  return true;
}

HBITMAP clipboard::GetImage() {
  HBITMAP result = nullptr;
  if (!OpenClipboard(nullptr)) return nullptr;

  // 1) PNG 格式（保留 alpha、无损）
  UINT png = PngFormat();
  if (png && IsClipboardFormatAvailable(png)) {
    HANDLE h = GetClipboardData(png);
    if (h) {
      IStream* stream = nullptr;
      if (SUCCEEDED(CreateStreamOnHGlobal(h, FALSE, &stream))) {
        std::unique_ptr<Gdiplus::Bitmap> bmp(Gdiplus::Bitmap::FromStream(stream));
        if (bmp && bmp->GetLastStatus() == Gdiplus::Ok)
          bmp->GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &result);
        stream->Release();
      }
    }
  }

  // 2) CF_DIB（24/32bpp BI_RGB）
  if (!result && IsClipboardFormatAvailable(CF_DIB)) {
    HGLOBAL h = GetClipboardData(CF_DIB);
    if (h) {
      const BITMAPINFO* bi = static_cast<const BITMAPINFO*>(GlobalLock(h));
      SIZE_T sz = GlobalSize(h);
      if (bi && sz > sizeof(BITMAPINFOHEADER)) {
        const BITMAPINFOHEADER& ih = bi->bmiHeader;
        const LONG hgt = ih.biHeight < 0 ? -ih.biHeight : ih.biHeight;
        const size_t bytes = static_cast<size_t>(hgt) * ih.biWidth * (ih.biBitCount / 8);
        if (ih.biCompression == BI_RGB && (ih.biBitCount == 24 || ih.biBitCount == 32) &&
            ih.biWidth > 0 && hgt > 0 &&
            static_cast<size_t>(ih.biSize) + bytes <= sz &&
            bytes < 0x40000000ULL) {
          BITMAPINFO di{};
          di.bmiHeader = ih;
          di.bmiHeader.biHeight = -hgt;  // 目标 top-down
          di.bmiHeader.biBitCount = 32;
          void* bits = nullptr;
          HDC dc = GetDC(nullptr);
          HBITMAP bmp = CreateDIBSection(dc, &di, DIB_RGB_COLORS, &bits, nullptr, 0);
          if (bmp) {
            HDC mem = CreateCompatibleDC(dc);
            HGDIOBJ old = SelectObject(mem, bmp);
            // 源仍用原始 bi（保留行序），由 GDI 完成转换
            StretchDIBits(mem, 0, 0, ih.biWidth, hgt, 0, 0, ih.biWidth, hgt,
                          reinterpret_cast<const uint8_t*>(bi) + ih.biSize, bi,
                          DIB_RGB_COLORS, SRCCOPY);
            SelectObject(mem, old);
            DeleteDC(mem);
            result = bmp;
          }
          ReleaseDC(nullptr, dc);
        }
      }
      if (bi) GlobalUnlock(h);
    }
  }

  CloseClipboard();
  return result;
}
