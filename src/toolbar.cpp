#include "toolbar.h"

#include <objbase.h>
#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include <vector>

#include "config.h"
#include "util.h"

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

namespace {

constexpr int kBtn = 26;       // 按钮边长（落地页 .xbtn 26x26）
constexpr int kGap = 3;        // flex gap
constexpr int kSepW = 9;       // 分隔线占位（1px + 两侧 4px margin）
constexpr int kPad = 4;        // .xrow padding（4.5 取整）
constexpr int kRow1Y = 4;      // 第一行 y
constexpr int kSwatch = 13;    // 色板圆直径（落地页 .cdot 13x13）
constexpr int kWidthBtn = 22;  // 粗细档命中区

constexpr COLORREF kBg = RGB(0x16, 0x1A, 0x22);      // 落地页 .xrow bg #161A22
constexpr COLORREF kBorder = RGB(43, 46, 53);        // 白 9% 线（.xrow border）
constexpr COLORREF kBtnBg = RGB(44, 44, 46);
constexpr COLORREF kBtnHover = RGB(36, 39, 45);       // 白 8% 悬停
constexpr COLORREF kGlyph = RGB(240, 240, 240);
constexpr COLORREF kDivider = RGB(64, 64, 64);

// ---------- Lucide 图标条（RCDATA 201=常态灰 / 202=高亮白，各 11 个 32x32 单元） ----------
// 图标 24px 绘制区居中于 32px 单元（四周 4px 留白），预乘 ARGB
HBITMAP g_strip_normal = nullptr;   // 灰色（常态）
HBITMAP g_strip_active = nullptr;   // 白色（悬停/激活）

int IconCellForId(int id) {
  if (id >= 0 && id < TOOL_COUNT) return id;
  switch (id) {
    case ACT_UNDO: return 6;
    case ACT_PIN: return 7;
    case ACT_COPY: return 8;
    case ACT_SAVE: return 9;
    case ACT_CANCEL: return 10;
    default: return -1;
  }
}

HBITMAP LoadStripResource(HINSTANCE inst, UINT resId) {
  HRSRC rc = FindResourceW(inst, MAKEINTRESOURCEW(resId), (LPCWSTR)RT_RCDATA);
  if (!rc) return nullptr;
  HGLOBAL res = LoadResource(inst, rc);
  const void* p = LockResource(res);
  const SIZE_T sz = SizeofResource(inst, rc);
  if (!p || !sz) return nullptr;
  HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, sz);
  if (!mem) return nullptr;
  memcpy(GlobalLock(mem), p, sz);
  GlobalUnlock(mem);
  IStream* stream = nullptr;
  if (FAILED(CreateStreamOnHGlobal(mem, TRUE, &stream))) {
    GlobalFree(mem);
    return nullptr;
  }
  std::unique_ptr<Gdiplus::Bitmap> bmp(Gdiplus::Bitmap::FromStream(stream));
  stream->Release();  // GDI+ 持有 stream 引用；Bitmap 析构后内存释放
  if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok) return nullptr;

  const int w = static_cast<int>(bmp->GetWidth());
  const int h = static_cast<int>(bmp->GetHeight());
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib || !bits) return nullptr;

  Gdiplus::BitmapData bd{};
  Gdiplus::Rect rect(0, 0, w, h);
  if (bmp->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) !=
      Gdiplus::Ok) {
    DeleteObject(dib);
    return nullptr;
  }
  // ARGB → 预乘 ARGB
  for (int y = 0; y < h; ++y) {
    const uint8_t* src = static_cast<const uint8_t*>(bd.Scan0) + y * bd.Stride;
    uint8_t* dst = static_cast<uint8_t*>(bits) + y * w * 4;
    for (int x = 0; x < w; ++x) {
      const uint8_t a = src[3];
      dst[0] = static_cast<uint8_t>(src[0] * a / 255);
      dst[1] = static_cast<uint8_t>(src[1] * a / 255);
      dst[2] = static_cast<uint8_t>(src[2] * a / 255);
      dst[3] = a;
      src += 4;
      dst += 4;
    }
  }
  bmp->UnlockBits(&bd);
  return dib;
}

void LoadIconStrips(HINSTANCE inst) {
  if (!g_strip_normal) g_strip_normal = LoadStripResource(inst, 201);
  if (!g_strip_active) g_strip_active = LoadStripResource(inst, 202);
}

void DrawIconCell(HDC dc, int cell, int cx, int cy, bool bright) {
  HBITMAP strip = bright ? g_strip_active : g_strip_normal;
  if (!strip || cell < 0) return;
  HDC mem = CreateCompatibleDC(dc);
  HGDIOBJ old = SelectObject(mem, strip);
  BLENDFUNCTION bf{AC_SRC_OVER, 0, static_cast<BYTE>(bright ? 235 : 255), AC_SRC_ALPHA};
  AlphaBlend(dc, cx - 16, cy - 16, 32, 32, mem, cell * 32, 0, 32, 32, bf);
  SelectObject(mem, old);
  DeleteDC(mem);
}

}  // namespace

bool ToolbarWindow::RegisterClass(HINSTANCE inst) {
  static bool done = false;
  if (done) return true;
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProcStatic;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.lpszClassName = config::kToolbarClass;
  if (!RegisterClassExW(&wc)) return false;
  done = true;
  return true;
}

ToolbarWindow* ToolbarWindow::Create(HINSTANCE inst, ToolbarState* state,
                                     Fn on_tool, Fn on_action, Fn on_color,
                                     Fn on_width) {
  if (!RegisterClass(inst)) return nullptr;
  auto* tb = new ToolbarWindow();
  tb->inst_ = inst;
  tb->state_ = state;
  tb->on_tool_ = std::move(on_tool);
  tb->on_action_ = std::move(on_action);
  tb->on_color_ = std::move(on_color);
  tb->on_width_ = std::move(on_width);
  LoadIconStrips(inst);
  tb->Layout();
  tb->hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                              config::kToolbarClass, L"", WS_POPUP, 0, 0,
                              tb->size_.cx, tb->size_.cy, nullptr, nullptr,
                              inst, tb);
  if (!tb->hwnd_) {
    delete tb;
    return nullptr;
  }
  return tb;
}

void ToolbarWindow::Layout() {
  ids_.clear();
  rects_.clear();
  int x = kPad;
  // 第一行：工具（6）— 分隔 — 撤销 — 分隔 — 动作（4），分隔线位置按空隙绘制
  for (int i = 0; i < TOOL_COUNT; ++i) {
    ids_.push_back(i);
    rects_.push_back(RECT{x, kRow1Y, x + kBtn, kRow1Y + kBtn});
    x += kBtn + kGap;
  }
  x += kSepW - kGap;
  ids_.push_back(ACT_UNDO);
  rects_.push_back(RECT{x, kRow1Y, x + kBtn, kRow1Y + kBtn});
  x += kBtn + kSepW;
  for (int id = ACT_PIN; id <= ACT_CANCEL; ++id) {
    ids_.push_back(id);
    rects_.push_back(RECT{x, kRow1Y, x + kBtn, kRow1Y + kBtn});
    x += kBtn + kGap;
  }
  const int row1_right = x - kGap;
  const int row1_w = row1_right + kPad;

  // 第二行：色板（13px 圆，左对齐）+ 粗细点（右对齐）
  const int row2_y = kRow1Y + kBtn + 5;  // .xbar gap 5
  int x2 = kPad + 3;                     // .cdot margin 2.5
  for (int i = 0; i < kColorCount; ++i) {
    ids_.push_back(200 + i);
    rects_.push_back(RECT{x2, row2_y, x2 + kSwatch, row2_y + kSwatch});
    x2 += kSwatch + 5;
  }
  int wr = row1_right;
  for (int i = kWidthCount - 1; i >= 0; --i) {
    const int d = kDotDiam[i];
    wr -= d;
    ids_.push_back(300 + i);
    rects_.push_back(RECT{wr - 3, row2_y + (kSwatch - d) / 2, wr + 3,
                          row2_y + (kSwatch - d) / 2 + d + 3});
    wr -= 7;
  }

  size_.cx = row1_w;
  size_.cy = row2_y + kSwatch + 5;
}

int ToolbarWindow::HitTest(POINT p) const {
  for (size_t i = 0; i < rects_.size(); ++i) {
    if (PtInRect(&rects_[i], p)) return ids_[i];
  }
  return -1;
}

void ToolbarWindow::ShowAt(int x, int y) {
  if (!hwnd_) return;
  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, size_.cx, size_.cy,
               SWP_SHOWWINDOW | SWP_NOACTIVATE);
  visible_ = true;
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void ToolbarWindow::Hide() {
  if (hwnd_ && visible_) ShowWindow(hwnd_, SW_HIDE);
  visible_ = false;
}

void ToolbarWindow::Destroy() {
  if (hwnd_) DestroyWindow(hwnd_);
}

void ToolbarWindow::Refresh() {
  if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

ToolbarWindow::~ToolbarWindow() = default;

LRESULT CALLBACK ToolbarWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wp,
                                              LPARAM lp) {
  if (msg == WM_NCCREATE) {
    auto* tb = static_cast<ToolbarWindow*>(
        reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(tb));
  }
  auto* tb =
      reinterpret_cast<ToolbarWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!tb) return DefWindowProcW(hwnd, msg, wp, lp);
  return tb->Handle(hwnd, msg, wp, lp);
}

LRESULT ToolbarWindow::Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd_, &ps);
      Draw(dc);
      EndPaint(hwnd_, &ps);
      return 0;
    }

    case WM_MOUSEMOVE: {
      POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      int h = HitTest(p);
      if (h != hover_) {
        hover_ = h;
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      if (!tracking_) {
        TRACKMOUSEEVENT tme{sizeof(tme), TME_LEAVE, hwnd_, 0};
        TrackMouseEvent(&tme);
        tracking_ = true;
      }
      return 0;
    }

    case WM_MOUSELEAVE:
      tracking_ = false;
      if (hover_ != -1) {
        hover_ = -1;
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      return 0;

    case WM_LBUTTONDOWN: {
      POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      const int id = HitTest(p);
      if (id < 0) return 0;
      if (id >= 0 && id < TOOL_COUNT) {
        state_->active_tool = (state_->active_tool == id) ? -1 : id;
        if (on_tool_) on_tool_(state_->active_tool);
        Refresh();
      } else if (id >= 200 && id < 200 + kColorCount) {
        state_->color = id - 200;
        if (on_color_) on_color_(state_->color);
        Refresh();
      } else if (id >= 300 && id < 300 + kWidthCount) {
        state_->width = id - 300;
        if (on_width_) on_width_(state_->width);
        Refresh();
      } else if (id >= ACT_UNDO && id <= ACT_CANCEL) {
        if (on_action_) on_action_(id);  // 动作可能销毁工具栏与遮罩，此后不得访问成员
      }
      return 0;
    }

    case WM_NCDESTROY: {
      hwnd_ = nullptr;
      delete this;  // 自管生命周期，此后不得访问成员
      return 0;
    }

    default:
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

void ToolbarWindow::Draw(HDC screen_dc) {
  RECT rc{};
  GetClientRect(hwnd_, &rc);
  // 双缓冲：先画进内存位图再一次上屏，消除按钮重绘闪烁
  HDC dc = CreateCompatibleDC(screen_dc);
  HBITMAP bmp = CreateCompatibleBitmap(screen_dc, rc.right, rc.bottom);
  HGDIOBJ obmp = SelectObject(dc, bmp);
  HBRUSH bg = CreateSolidBrush(kBg);
  FillRect(dc, &rc, bg);
  DeleteObject(bg);
  HPEN border = CreatePen(PS_SOLID, 1, kBorder);
  HGDIOBJ open_ = SelectObject(dc, border);
  HGDIOBJ obr = SelectObject(dc, GetStockObject(NULL_BRUSH));
  Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
  SelectObject(dc, obr);
  SelectObject(dc, open_);
  DeleteObject(border);

  for (size_t i = 0; i < ids_.size(); ++i) {
    const int id = ids_[i];
    const RECT& r = rects_[i];
    const bool is_tool = id >= 0 && id < TOOL_COUNT;
    const bool is_color = id >= 200 && id < 200 + kColorCount;
    const bool is_width = id >= 300 && id < 300 + kWidthCount;
    const bool active =
        (is_tool && state_->active_tool == id) ||
        (is_color && state_->color == id - 200) ||
        (is_width && state_->width == id - 300);
    const bool hover = hover_ == static_cast<int>(i);

    // 按钮底色：常态不画框（更干净），悬停浅灰、激活主题色
    if (!is_color && (active || hover)) {
      // 6px 圆角底（对齐落地页小件圆角体系）
      const COLORREF fill = active ? state_->accent : kBtnHover;
      HBRUSH b = CreateSolidBrush(fill);
      HPEN p = CreatePen(PS_SOLID, 1, fill);
      HGDIOBJ ob = SelectObject(dc, b);
      HGDIOBJ op = SelectObject(dc, p);
      RoundRect(dc, r.left, r.top, r.right + 1, r.bottom + 1, 10, 10);
      SelectObject(dc, ob);
      SelectObject(dc, op);
      DeleteObject(b);
      DeleteObject(p);
    }

    if (is_color) {
      // 圆形色板（.cdot 13px；选中 = 白圈 outline offset 1.5）
      const COLORREF col = kPalette[id - 200];
      const float ccx = (r.left + r.right) / 2.0f;
      const float ccy = (r.top + r.bottom) / 2.0f;
      Gdiplus::Graphics g(dc);
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      Gdiplus::SolidBrush br(Gdiplus::Color(255, GetRValue(col), GetGValue(col),
                                            GetBValue(col)));
      g.FillEllipse(&br, Gdiplus::RectF(ccx - 6.5f, ccy - 6.5f, 13.0f, 13.0f));
      if (active) {
        Gdiplus::Pen ring(Gdiplus::Color(255, 255, 255, 255), 2.0f);
        g.DrawEllipse(&ring, Gdiplus::RectF(ccx - 9.0f, ccy - 9.0f, 18.0f, 18.0f));
      } else if (hover) {  // 悬停：45% 白圈预览
        Gdiplus::Pen ring(Gdiplus::Color(115, 255, 255, 255), 2.0f);
        g.DrawEllipse(&ring, Gdiplus::RectF(ccx - 9.0f, ccy - 9.0f, 18.0f, 18.0f));
      }
    } else if (is_width) {
      // 粗细档位（.wdot 5/8/12；选中 = 白圈）
      const int wi = id - 300;
      const float ccx = (r.left + r.right) / 2.0f;
      const float ccy = (r.top + r.bottom) / 2.0f;
      const float d = static_cast<float>(kDotDiam[wi]);
      Gdiplus::Graphics g(dc);
      g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
      Gdiplus::SolidBrush br(Gdiplus::Color(255, 199, 205, 216));
      g.FillEllipse(&br, Gdiplus::RectF(ccx - d / 2, ccy - d / 2, d, d));
      if (active) {
        Gdiplus::Pen ring(Gdiplus::Color(255, 255, 255, 255), 2.0f);
        g.DrawEllipse(&ring, Gdiplus::RectF(ccx - d / 2 - 2.5f, ccy - d / 2 - 2.5f,
                                            d + 5.0f, d + 5.0f));
      } else if (hover) {  // 悬停：45% 白圈预览
        Gdiplus::Pen ring(Gdiplus::Color(115, 255, 255, 255), 2.0f);
        g.DrawEllipse(&ring, Gdiplus::RectF(ccx - d / 2 - 2.5f, ccy - d / 2 - 2.5f,
                                            d + 5.0f, d + 5.0f));
      }
    } else if (g_strip_normal && g_strip_active) {
      // Lucide 资源图标：常态灰色不透明，悬停/激活白色
      DrawIconCell(dc, IconCellForId(id), (r.left + r.right) / 2,
                   (r.top + r.bottom) / 2, active || hover);
    } else {
      DrawGlyph(dc, id, r);  // 资源加载失败的兜底
    }
  }

  // 分组分隔线（双分隔线：工具|撤销|动作，落地页 .xsep）
  HPEN div = CreatePen(PS_SOLID, 1, kDivider);
  open_ = SelectObject(dc, div);
  const int s1 = (rects_[TOOL_COUNT - 1].right + rects_[TOOL_COUNT].left) / 2;
  const int s2 = (rects_[TOOL_COUNT].right + rects_[TOOL_COUNT + 1].left) / 2;
  MoveToEx(dc, s1, kRow1Y + 6, nullptr);
  LineTo(dc, s1, kRow1Y + kBtn - 6);
  MoveToEx(dc, s2, kRow1Y + 6, nullptr);
  LineTo(dc, s2, kRow1Y + kBtn - 6);
  // 色板|粗细（中线）
  const int last_sw = TOOL_COUNT + 2 + (ACT_CANCEL - ACT_PIN) + kColorCount - 1;
  const int cw_r = rects_[last_sw].right;
  const int cw_l = rects_[last_sw + 1].left;
  const int d3 = (cw_r + cw_l) / 2;
  MoveToEx(dc, d3, kRow1Y + kBtn + 6, nullptr);
  LineTo(dc, d3, size_.cy - 8);
  SelectObject(dc, open_);
  DeleteObject(div);

  // 一次性上屏
  BitBlt(screen_dc, 0, 0, rc.right, rc.bottom, dc, 0, 0, SRCCOPY);
  SelectObject(dc, obmp);
  DeleteObject(bmp);
  DeleteDC(dc);
}

// 18x18 图标区绘制（r 为按钮矩形，图标内缩 7px）
void ToolbarWindow::DrawGlyph(HDC dc, int id, RECT r) const {
  const int x0 = r.left + (r.right - r.left - 18) / 2;
  const int y0 = r.top + (r.bottom - r.top - 18) / 2;
  HPEN pen = CreatePen(PS_SOLID, 2, kGlyph);
  HGDIOBJ open_ = SelectObject(dc, pen);
  HGDIOBJ obr = SelectObject(dc, GetStockObject(NULL_BRUSH));
  HGDIOBJ obf = nullptr;
  HBRUSH fill = nullptr;

  switch (id) {
    case TOOL_RECT:
      Rectangle(dc, x0 + 2, y0 + 4, x0 + 16, y0 + 14);
      break;
    case TOOL_ELLIPSE:
      Ellipse(dc, x0 + 2, y0 + 4, x0 + 16, y0 + 14);
      break;
    case TOOL_ARROW:
      MoveToEx(dc, x0 + 2, y0 + 14, nullptr);
      LineTo(dc, x0 + 15, y0 + 3);
      MoveToEx(dc, x0 + 15, y0 + 3, nullptr);
      LineTo(dc, x0 + 8, y0 + 5);
      MoveToEx(dc, x0 + 15, y0 + 3, nullptr);
      LineTo(dc, x0 + 13, y0 + 10);
      break;
    case TOOL_PEN: {
      MoveToEx(dc, x0 + 3, y0 + 15, nullptr);
      LineTo(dc, x0 + 13, y0 + 5);
      fill = CreateSolidBrush(kGlyph);
      obf = SelectObject(dc, fill);
      RECT nib{x0 + 12, y0 + 2, x0 + 17, y0 + 7};
      FillRect(dc, &nib, fill);
      SelectObject(dc, obf);
      DeleteObject(fill);
      break;
    }
    case TOOL_MARKER:
      DeleteObject(SelectObject(dc, CreatePen(PS_SOLID, 6, kGlyph)));
      MoveToEx(dc, x0 + 4, y0 + 13, nullptr);
      LineTo(dc, x0 + 14, y0 + 4);
      break;
    case TOOL_MOSAIC:
      fill = CreateSolidBrush(kGlyph);
      obf = SelectObject(dc, fill);
      for (int gy = 0; gy < 3; ++gy)
        for (int gx = 0; gx < 3; ++gx)
          Rectangle(dc, x0 + 3 + gx * 5, y0 + 3 + gy * 5, x0 + 6 + gx * 5,
                    y0 + 6 + gy * 5);
      SelectObject(dc, obf);
      DeleteObject(fill);
      break;
    case ACT_UNDO:
      Arc(dc, x0 + 3, y0 + 4, x0 + 16, y0 + 18, x0 + 16, y0 + 13, x0 + 3,
          y0 + 13);
      MoveToEx(dc, x0 + 3, y0 + 13, nullptr);
      LineTo(dc, x0 + 3, y0 + 7);
      MoveToEx(dc, x0 + 3, y0 + 13, nullptr);
      LineTo(dc, x0 + 9, y0 + 13);
      break;
    case ACT_PIN:
      Ellipse(dc, x0 + 4, y0 + 2, x0 + 15, y0 + 13);
      MoveToEx(dc, x0 + 9, y0 + 13, nullptr);
      LineTo(dc, x0 + 4, y0 + 17);
      break;
    case ACT_COPY:
      Rectangle(dc, x0 + 2, y0 + 6, x0 + 12, y0 + 16);
      Rectangle(dc, x0 + 7, y0 + 2, x0 + 17, y0 + 12);
      break;
    case ACT_SAVE:
      Rectangle(dc, x0 + 2, y0 + 2, x0 + 16, y0 + 16);
      MoveToEx(dc, x0 + 5, y0 + 2, nullptr);
      LineTo(dc, x0 + 5, y0 + 8);
      LineTo(dc, x0 + 13, y0 + 8);
      LineTo(dc, x0 + 13, y0 + 2);
      Rectangle(dc, x0 + 5, y0 + 11, x0 + 13, y0 + 16);
      break;
    case ACT_CANCEL:
      MoveToEx(dc, x0 + 4, y0 + 4, nullptr);
      LineTo(dc, x0 + 14, y0 + 14);
      MoveToEx(dc, x0 + 14, y0 + 4, nullptr);
      LineTo(dc, x0 + 4, y0 + 14);
      break;
    default:
      break;
  }
  SelectObject(dc, obr);
  SelectObject(dc, open_);
  DeleteObject(pen);
}
