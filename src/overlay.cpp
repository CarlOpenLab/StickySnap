#include "overlay.h"

#include <commdlg.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "clipboard.h"
#include "config.h"
#include "gdiutil.h"
#include "util.h"

OverlayWindow* OverlayWindow::s_active_ = nullptr;

// ---------- 标注绘制 ----------

void Shape::Offset(int dx, int dy) {
  for (auto& p : pts) {
    p.x += dx;
    p.y += dy;
  }
}

namespace {

// 选区边框固定高对比红，与品牌强调色分离
constexpr COLORREF kSelAccent = config::kSelection;
constexpr COLORREF kSelAccentDark = config::kSelectionDark;
constexpr COLORREF kHoverAccent = RGB(0x38, 0xE1, 0xCF);  // 落地页 --accent-2 青（与选区红/蓝区分语义）

constexpr DWORD kMagic = 0x00FF00FF;  // ARGB 画布透明键（品红，不在调色板中）

// 实心绘制单个形状（画布合成 / 预览共用；mosaic 仅画预览圈）
void DrawShapeSolid(HDC dc, const Shape& s) {
  if (s.pts.empty()) return;
  int w = s.width;
  if (s.type == ShapeType::kMarker) w = s.width * 4;
  if (w < 1) w = 1;

  LOGBRUSH lb{BS_SOLID, s.color, 0};
  HPEN pen = ExtCreatePen(PS_GEOMETRIC | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
                          static_cast<DWORD>(w), &lb, 0, nullptr);
  if (!pen) return;
  HGDIOBJ open_ = SelectObject(dc, pen);
  HGDIOBJ obr = SelectObject(dc, GetStockObject(NULL_BRUSH));

  const POINT& a = s.pts[0];
  const POINT* b = s.pts.size() >= 2 ? &s.pts[1] : nullptr;

  switch (s.type) {
    case ShapeType::kRect:
      if (b) Rectangle(dc, a.x < b->x ? a.x : b->x, a.y < b->y ? a.y : b->y,
                       (a.x < b->x ? b->x : a.x) + 1, (a.y < b->y ? b->y : a.y) + 1);
      break;
    case ShapeType::kEllipse:
      if (b) Ellipse(dc, a.x < b->x ? a.x : b->x, a.y < b->y ? a.y : b->y,
                     (a.x < b->x ? b->x : a.x) + 1, (a.y < b->y ? b->y : a.y) + 1);
      break;
    case ShapeType::kArrow:
      if (b) {
        MoveToEx(dc, a.x, a.y, nullptr);
        LineTo(dc, b->x, b->y);
        const double ang = atan2(static_cast<double>(b->y - a.y),
                                 static_cast<double>(b->x - a.x));
        const double hl = 3.0 * w + 4.0;
        const int h1x = b->x - static_cast<int>(hl * cos(ang - 0.45));
        const int h1y = b->y - static_cast<int>(hl * sin(ang - 0.45));
        const int h2x = b->x - static_cast<int>(hl * cos(ang + 0.45));
        const int h2y = b->y - static_cast<int>(hl * sin(ang + 0.45));
        MoveToEx(dc, b->x, b->y, nullptr);
        LineTo(dc, h1x, h1y);
        MoveToEx(dc, b->x, b->y, nullptr);
        LineTo(dc, h2x, h2y);
      }
      break;
    case ShapeType::kPen:
    case ShapeType::kMarker:
      if (s.pts.size() >= 2) {
        Polyline(dc, s.pts.data(), static_cast<int>(s.pts.size()));
      } else {
        const int r = w / 2 + 1;
        Ellipse(dc, a.x - r, a.y - r, a.x + r, a.y + r);
      }
      break;
    case ShapeType::kMosaic: {
      // 仅预览：灰色圆圈示意笔刷
      HPEN gp = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
      HGDIOBJ g = SelectObject(dc, gp);
      const int d = 8 + s.width * 3;
      for (const auto& pt : s.pts)
        Ellipse(dc, pt.x - d, pt.y - d, pt.x + d, pt.y + d);
      SelectObject(dc, g);
      DeleteObject(gp);
      break;
    }
  }
  SelectObject(dc, obr);
  SelectObject(dc, open_);
  DeleteObject(pen);
}

// 马赛克：沿笔画圆点并集裁剪，贴像素化图
void DrawMosaicShape(HDC dst, const Shape& s, POINT offset, HDC mosaic_dc,
                     RECT dst_rect) {
  if (s.pts.empty()) return;
  const int d = 8 + s.width * 3;
  HRGN rgn = CreateRectRgn(0, 0, 0, 0);
  for (const auto& pt : s.pts) {
    const int x = pt.x - offset.x;
    const int y = pt.y - offset.y;
    HRGN c = CreateEllipticRgn(x - d, y - d, x + d, y + d);
    CombineRgn(rgn, rgn, c, RGN_OR);
    DeleteObject(c);
  }
  SelectClipRgn(dst, rgn);
  BitBlt(dst, dst_rect.left, dst_rect.top,
         dst_rect.right - dst_rect.left, dst_rect.bottom - dst_rect.top,
         mosaic_dc, 0, 0, SRCCOPY);
  SelectClipRgn(dst, nullptr);
  DeleteObject(rgn);
}

// ARGB 形状图层：GDI 画实心 → 后处理写 alpha（magic 键 = 透明）
// marker_layer: 整层统一 110/255 半透明（荧光笔）
HBITMAP MakeShapeLayer(const std::vector<Shape>& shapes, SIZE size,
                       POINT origin, bool marker_layer) {
  if (size.cx <= 0 || size.cy <= 0) return nullptr;
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = size.cx;
  bi.bmiHeader.biHeight = -size.cy;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!bmp || !bits) return nullptr;

  const size_t n = static_cast<size_t>(size.cx) * size.cy;
  DWORD* p = static_cast<DWORD*>(bits);
  for (size_t i = 0; i < n; ++i) p[i] = kMagic;

  HDC dc = CreateCompatibleDC(nullptr);
  HGDIOBJ old = SelectObject(dc, bmp);
  for (const auto& s : shapes) {
    Shape t = s;
    t.Offset(-origin.x, -origin.y);
    DrawShapeSolid(dc, t);
  }
  SelectObject(dc, old);
  DeleteDC(dc);

  const BYTE alpha = marker_layer ? 110 : 255;
  for (size_t i = 0; i < n; ++i) {
    BYTE* px = reinterpret_cast<BYTE*>(p + i);
    if (px[0] == 255 && px[1] == 0 && px[2] == 255) {
      px[3] = 0;  // magic 键 → 透明
    } else {
      px[0] = static_cast<BYTE>(px[0] * alpha / 255);
      px[1] = static_cast<BYTE>(px[1] * alpha / 255);
      px[2] = static_cast<BYTE>(px[2] * alpha / 255);
      px[3] = alpha;
    }
  }
  return bmp;
}

void DrawLayer(HDC dst, HBITMAP layer, HDC layer_dc, int x, int y, int w, int h) {
  if (!layer) return;
  HGDIOBJ old = SelectObject(layer_dc, layer);
  BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  AlphaBlend(dst, x, y, w, h, layer_dc, 0, 0, w, h, bf);
  SelectObject(layer_dc, old);
}

// 像素级精确的矩形环：在 r 外侧画 [inner, outer) 像素宽的环带（FillRect 条带，无画笔居中问题）
void FillRing(HDC dc, const RECT& r, int inner, int outer, COLORREF c) {
  const RECT full{r.left - outer, r.top - outer, r.right + outer, r.bottom + outer};
  const RECT innerR{r.left - inner, r.top - inner, r.right + inner, r.bottom + inner};
  HBRUSH br = CreateSolidBrush(c);
  RECT bar{full.left, full.top, full.right, innerR.top};            // 上
  FillRect(dc, &bar, br);
  bar = RECT{full.left, innerR.bottom, full.right, full.bottom};    // 下
  FillRect(dc, &bar, br);
  bar = RECT{full.left, innerR.top, innerR.left, innerR.bottom};    // 左
  FillRect(dc, &bar, br);
  bar = RECT{innerR.right, innerR.top, full.right, innerR.bottom};  // 右
  FillRect(dc, &bar, br);
  DeleteObject(br);
}

// ---------- 窗口检测（z-order 枚举） ----------

struct HitCtx {
  POINT pt;
  HWND self;
  HWND result;
};

BOOL CALLBACK HitEnumProc(HWND h, LPARAM lp) {
  auto* c = reinterpret_cast<HitCtx*>(lp);
  if (c->result) return TRUE;
  if (h == c->self) return TRUE;
  if (!IsWindowVisible(h) || IsIconic(h)) return TRUE;
  LONG_PTR ex = GetWindowLongPtrW(h, GWL_EXSTYLE);
  if (ex & WS_EX_TRANSPARENT) return TRUE;

  wchar_t cls[64];
  if (!GetClassNameW(h, cls, 64)) return TRUE;
  if (!wcscmp(cls, L"Progman") || !wcscmp(cls, L"WorkerW") ||
      !wcscmp(cls, config::kPinClass) || !wcscmp(cls, config::kOverlayClass) ||
      !wcscmp(cls, config::kToolbarClass))
    return TRUE;

  DWORD cloaked = 0;
  if (SUCCEEDED(DwmGetWindowAttribute(h, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
      cloaked)
    return TRUE;
  RECT r{};
  if (FAILED(DwmGetWindowAttribute(h, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r))))
    GetWindowRect(h, &r);
  if (!PtInRect(&r, c->pt)) return TRUE;

  c->result = GetAncestor(h, GA_ROOT);
  return FALSE;
}

}  // namespace

bool OverlayWindow::RegisterClass(HINSTANCE inst) {
  static bool done = false;
  if (done) return true;
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProcStatic;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
  wc.lpszClassName = config::kOverlayClass;
  if (!RegisterClassExW(&wc)) return false;
  done = true;
  return true;
}

OverlayWindow* OverlayWindow::Open(HINSTANCE inst, CapturedScreen* screen,
                                   const Settings& settings,
                                   FinishFn on_finish, ClosedFn on_closed) {
  if (s_active_) {
    delete screen;
    return s_active_;
  }
  if (!screen || !screen->Valid()) {
    delete screen;
    return nullptr;
  }
  auto* ov = new OverlayWindow();
  ov->inst_ = inst;
  ov->screen_ = screen;
  ov->st_ = settings;
  ov->on_finish_ = std::move(on_finish);
  ov->on_closed_ = std::move(on_closed);
  if (!RegisterClass(inst)) {
    delete ov;
    return nullptr;
  }
  const RECT b = screen->bounds();
  ov->hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                              config::kOverlayClass, L"", WS_POPUP,
                              b.left, b.top, screen->width(), screen->height(),
                              nullptr, nullptr, inst, ov);
  if (!ov->hwnd_) {
    delete ov;
    return nullptr;
  }
  util::ExcludeFromCapture(ov->hwnd_);
  ov->BuildDimLayer();
  ShowWindow(ov->hwnd_, SW_SHOW);
  SetForegroundWindow(ov->hwnd_);
  SetFocus(ov->hwnd_);
  s_active_ = ov;
  return ov;
}

void OverlayWindow::Destroy() {
  if (hwnd_) DestroyWindow(hwnd_);
}

bool OverlayWindow::EnsureBackBuffer(HDC ref, int w, int h) {
  if (bb_dc_) return true;
  bb_dc_ = CreateCompatibleDC(ref);
  bb_ = CreateCompatibleBitmap(ref, w, h);
  if (!bb_dc_ || !bb_) return false;
  SelectObject(bb_dc_, bb_);
  return true;
}

OverlayWindow::~OverlayWindow() {
  if (toolbar_) {
    toolbar_->Destroy();
    toolbar_ = nullptr;
  }
  ResetEdit();
  if (canvas_dc_) DeleteDC(canvas_dc_);
  if (canvas_m_dc_) DeleteDC(canvas_m_dc_);
  if (mosaic_dc_) DeleteDC(mosaic_dc_);
  if (bb_dc_) DeleteDC(bb_dc_);
  if (bb_) DeleteObject(bb_);
  if (dim_dc_) DeleteDC(dim_dc_);
  if (dim_bmp_) DeleteObject(dim_bmp_);
  delete screen_;
  if (s_active_ == this) s_active_ = nullptr;
  if (on_closed_ && !closed_called_) {
    closed_called_ = true;
    on_closed_();
  }
}

LRESULT CALLBACK OverlayWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wp,
                                              LPARAM lp) {
  if (msg == WM_NCCREATE) {
    auto* ov = static_cast<OverlayWindow*>(
        reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ov));
  }
  auto* ov = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!ov) return DefWindowProcW(hwnd, msg, wp, lp);
  return ov->Handle(hwnd, msg, wp, lp);
}

void OverlayWindow::BuildDimLayer() {
  const int w = screen_->width();
  const int h = screen_->height();
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  dim_bmp_ = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dim_bmp_ || !bits) return;
  dim_dc_ = CreateCompatibleDC(nullptr);
  SelectObject(dim_dc_, dim_bmp_);

  const size_t n = static_cast<size_t>(w) * h * 4;
  uint8_t* d = static_cast<uint8_t*>(bits);
  const uint8_t* s = screen_->bits();
  if (!s) return;
  const int keep = 100 - st_.dim_percent;  // 遮罩后的亮度百分比
  for (size_t i = 0; i < n; i += 4) {
    d[i] = static_cast<uint8_t>(s[i] * keep / 100);
    d[i + 1] = static_cast<uint8_t>(s[i + 1] * keep / 100);
    d[i + 2] = static_cast<uint8_t>(s[i + 2] * keep / 100);
    d[i + 3] = 255;
  }
}

LRESULT OverlayWindow::Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd_, &ps);
      const int w = screen_->width();
      const int h = screen_->height();
      if (!EnsureBackBuffer(dc, w, h)) {
        EndPaint(hwnd_, &ps);
        return 0;
      }
      // 整帧先合成进后台缓冲（GDI 裁剪到本次脏区），再一次上屏 → 无中间态闪烁
      const int saved_bb = SaveDC(bb_dc_);
      IntersectClipRect(bb_dc_, ps.rcPaint.left, ps.rcPaint.top,
                        ps.rcPaint.right, ps.rcPaint.bottom);
      HDC comp = bb_dc_;
      BitBlt(comp, 0, 0, w, h, dim_dc_, 0, 0, SRCCOPY);
      if (HasSel()) {
        const int w2 = sel_.right - sel_.left;
        const int h2 = sel_.bottom - sel_.top;
        // 1) 选区内恢复原图
        BitBlt(comp, sel_.left, sel_.top, w2, h2, screen_->mem_dc(), sel_.left,
               sel_.top, SRCCOPY);
        // 2) 马赛克层（仅存在马赛克笔画时才构建）
        bool has_mosaic = false, has_marker = false, has_solid = false;
        for (const auto& s : shapes_) {
          if (s.type == ShapeType::kMosaic)
            has_mosaic = true;
          else if (s.type == ShapeType::kMarker)
            has_marker = true;
          else
            has_solid = true;
        }
        if (has_mosaic && EnsureMosaic()) {
          const RECT dst{sel_.left, sel_.top, sel_.right, sel_.bottom};
          for (const auto& s : shapes_)
            if (s.type == ShapeType::kMosaic)
              DrawMosaicShape(comp, s, POINT{0, 0}, mosaic_dc_, dst);
        }
        // 3) 标注层（荧光笔在下、实心形状在上）
        if (EnsureCanvases()) {
          if (has_marker)
            DrawLayer(comp, canvas_m_, canvas_m_dc_, sel_.left, sel_.top, w2, h2);
          if (has_solid)
            DrawLayer(comp, canvas_, canvas_dc_, sel_.left, sel_.top, w2, h2);
        }
        // 4) 进行中的笔画（裁剪进选区）
        if (drawing_) {
          const int sc = SaveDC(comp);
          IntersectClipRect(comp, sel_.left, sel_.top, sel_.right, sel_.bottom);
          DrawShapeSolid(comp, preview_);
          RestoreDC(comp, sc);
        }
        // 5) 选区边框：2px 单色细线（贴内容外沿）+ 8 个调节圆点（四角 + 四边中点）
        FillRing(comp, sel_, 0, 2, kSelAccent);
        {
          const POINT dots[8] = {
              {sel_.left, sel_.top},
              {(sel_.left + sel_.right) / 2, sel_.top},
              {sel_.right, sel_.top},
              {sel_.left, (sel_.top + sel_.bottom) / 2},
              {sel_.right, (sel_.top + sel_.bottom) / 2},
              {sel_.left, sel_.bottom},
              {(sel_.left + sel_.right) / 2, sel_.bottom},
              {sel_.right, sel_.bottom},
          };
          HBRUSH dotBr = CreateSolidBrush(kSelAccent);
          HPEN dotPen = CreatePen(PS_SOLID, 1, kSelAccentDark);
          HGDIOBJ db = SelectObject(comp, dotBr);
          HGDIOBJ dp = SelectObject(comp, dotPen);
          for (const auto& pt : dots)
            Ellipse(comp, pt.x - 4, pt.y - 4, pt.x + 4, pt.y + 4);
          SelectObject(comp, db);
          SelectObject(comp, dp);
          DeleteObject(dotBr);
          DeleteObject(dotPen);
        }
        if (selecting_) DrawSizeLabel(comp);
      } else if (!selecting_ && hover_valid_) {
        FillRing(comp, hover_rect_, 0, 2, kHoverAccent);
      }
      if (!selecting_ && !HasSel()) DrawMagnifier(comp);
      RestoreDC(bb_dc_, saved_bb);
      // 一次性上屏（自动裁剪到 ps.rcPaint）
      BitBlt(dc, 0, 0, w, h, bb_dc_, 0, 0, SRCCOPY);
      EndPaint(hwnd_, &ps);
      return 0;
    }

    case WM_MOUSEMOVE: {
      POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      if (drawing_) {
        cursor_ = p;
        if (preview_.type == ShapeType::kPen ||
            preview_.type == ShapeType::kMarker ||
            preview_.type == ShapeType::kMosaic) {
          if (preview_.pts.empty()) {
            preview_.pts.push_back(p);
          } else {
            const POINT& last = preview_.pts.back();
            const int dx = p.x - last.x, dy = p.y - last.y;
            if (dx * dx + dy * dy >= 9) preview_.pts.push_back(p);
          }
        } else if (preview_.pts.size() >= 2) {
          preview_.pts[1] = p;
        }
        RECT r = sel_;
        InflateRect(&r, 6, 6);
        InvalidateRect(hwnd_, &r, FALSE);
        return 0;
      }
      const RECT old_sel = sel_;
      OnMouseMove(p);
      if (selecting_) {
        // 只重绘新旧选区覆盖的范围（含边框与尺寸标签）
        RECT u;
        UnionRect(&u, &old_sel, &sel_);
        InflateRect(&u, 6, 46);
        InvalidateRect(hwnd_, &u, FALSE);
      } else if (HasSel()) {
        // 选区已定：鼠标移动不需要重绘（防闪烁关键）
      } else {
        InvalidateRect(hwnd_, nullptr, FALSE);  // 探索态：放大镜 / 窗口高亮
      }
      return 0;
    }

    case WM_LBUTTONDOWN: {
      POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
      cursor_ = p;
      // 工具激活 + 点在选区内 → 开始笔画
      if (HasSel() && tstate_.active_tool >= 0 && PtInRect(&sel_, p)) {
        drawing_ = true;
        SetCapture(hwnd_);
        preview_ = MakePreview();
        preview_.pts.assign(1, p);
        if (preview_.type == ShapeType::kRect ||
            preview_.type == ShapeType::kEllipse ||
            preview_.type == ShapeType::kArrow)
          preview_.pts.push_back(p);
        return 0;
      }
      // 已有选区且未激活工具时，选区内部点击不重选（防误触丢标注）
      if (HasSel() && tstate_.active_tool < 0 && PtInRect(&sel_, p)) return 0;
      // 选区外 → 重新框选
      if (toolbar_ && toolbar_->visible()) toolbar_->Hide();
      ResetEdit();
      SetCapture(hwnd_);
      selecting_ = true;
      sel_start_ = cursor_;
      sel_ = {cursor_.x, cursor_.y, cursor_.x, cursor_.y};
      hover_valid_ = false;
      InvalidateRect(hwnd_, nullptr, FALSE);
      return 0;
    }

    case WM_LBUTTONUP: {
      if (drawing_) {
        drawing_ = false;
        ReleaseCapture();
        bool valid = false;
        const int dx = preview_.pts.size() >= 2 ? preview_.pts[1].x - preview_.pts[0].x : 0;
        const int dy = preview_.pts.size() >= 2 ? preview_.pts[1].y - preview_.pts[0].y : 0;
        switch (preview_.type) {
          case ShapeType::kRect:
          case ShapeType::kEllipse:
            valid = (dx > 1 || dx < -1) && (dy > 1 || dy < -1);
            break;
          case ShapeType::kArrow:
            valid = dx * dx + dy * dy > 36;
            break;
          case ShapeType::kPen:
          case ShapeType::kMarker:
            valid = preview_.pts.size() >= 2;
            break;
          case ShapeType::kMosaic:
            valid = !preview_.pts.empty();
            break;
        }
        if (valid) {
          shapes_.push_back(preview_);
          shapes_dirty_ = true;
        }
        preview_ = Shape{};
        RECT r = sel_;
        InflateRect(&r, 6, 6);
        InvalidateRect(hwnd_, &r, FALSE);
        return 0;
      }
      if (!selecting_) return 0;
      ReleaseCapture();
      selecting_ = false;
      if (!HasSel()) {
        sel_ = hover_valid_ ? hover_rect_
                            : RECT{cursor_.x, cursor_.y, cursor_.x, cursor_.y};
      }
      if (HasSel()) ShowToolbar();
      RECT r2 = sel_;
      InflateRect(&r2, 4, 44);
      InvalidateRect(hwnd_, &r2, FALSE);
      return 0;
    }

    case WM_LBUTTONDBLCLK:
      if (HasSel()) Commit();
      return 0;

    case WM_KEYDOWN:
      switch (wp) {
        case VK_ESCAPE:
          Cancel();
          return 0;
        case VK_RETURN:
          if (HasSel()) Commit();
          return 0;
        case 'C':
          if (GetKeyState(VK_CONTROL) < 0 && HasSel()) CopySel();
          return 0;
        case 'Z':
          if (GetKeyState(VK_CONTROL) < 0) OnAction(ACT_UNDO);
          return 0;
        default:
          break;
      }
      return 0;

    case WM_NCDESTROY: {
      if (dim_dc_) {
        DeleteDC(dim_dc_);
        dim_dc_ = nullptr;
      }
      if (dim_bmp_) {
        DeleteObject(dim_bmp_);
        dim_bmp_ = nullptr;
      }
      if (s_active_ == this) s_active_ = nullptr;
      delete this;  // 自管生命周期，此后不得访问成员
      return 0;
    }

    default:
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

void OverlayWindow::OnMouseMove(const POINT& pt) {
  cursor_ = pt;
  if (selecting_) {
    sel_.left = sel_start_.x < cursor_.x ? sel_start_.x : cursor_.x;
    sel_.top = sel_start_.y < cursor_.y ? sel_start_.y : cursor_.y;
    sel_.right = sel_start_.x < cursor_.x ? cursor_.x : sel_start_.x;
    sel_.bottom = sel_start_.y < cursor_.y ? cursor_.y : sel_start_.y;
  } else if (!HasSel()) {
    UpdateHover();
  }
}

void OverlayWindow::UpdateHover() {
  hover_valid_ = false;
  const RECT b = screen_->bounds();
  HitCtx ctx{POINT{cursor_.x + b.left, cursor_.y + b.top}, hwnd_, nullptr};
  EnumWindows(HitEnumProc, reinterpret_cast<LPARAM>(&ctx));
  if (!ctx.result || ctx.result == hwnd_) return;

  RECT r{};
  if (FAILED(DwmGetWindowAttribute(ctx.result, DWMWA_EXTENDED_FRAME_BOUNDS, &r,
                                   sizeof(r))))
    GetWindowRect(ctx.result, &r);
  OffsetRect(&r, -b.left, -b.top);
  RECT client{0, 0, screen_->width(), screen_->height()};
  RECT out{};
  if (IntersectRect(&out, &r, &client) && out.right - out.left > 2 &&
      out.bottom - out.top > 2) {
    hover_rect_ = out;
    hover_valid_ = true;
  }
}

bool OverlayWindow::HasSel() const {
  return sel_.right - sel_.left >= 3 && sel_.bottom - sel_.top >= 3;
}

HBITMAP OverlayWindow::Crop() const {
  const int w = sel_.right - sel_.left;
  const int h = sel_.bottom - sel_.top;
  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC dst = CreateCompatibleDC(screen_->mem_dc());
  HBITMAP out = CreateDIBSection(dst, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (out) {
    HGDIOBJ old = SelectObject(dst, out);
    BitBlt(dst, 0, 0, w, h, screen_->mem_dc(), sel_.left, sel_.top, SRCCOPY);
    SelectObject(dst, old);
  }
  DeleteDC(dst);
  return out;
}

HBITMAP OverlayWindow::FlattenSelection() const {
  HBITMAP out = Crop();
  if (!out) return nullptr;
  const int w = sel_.right - sel_.left;
  const int h = sel_.bottom - sel_.top;
  HDC dst = CreateCompatibleDC(screen_->mem_dc());
  HGDIOBJ old = SelectObject(dst, out);

  // 马赛克
  if (EnsureMosaic()) {
    const RECT dst_rect{0, 0, w, h};
    for (const auto& s : shapes_)
      if (s.type == ShapeType::kMosaic)
        DrawMosaicShape(dst, s, POINT{sel_.left, sel_.top}, mosaic_dc_, dst_rect);
  }
  // 形状层
  std::vector<Shape> solid, marker;
  for (const auto& s : shapes_) {
    if (s.type == ShapeType::kMosaic) continue;
    (s.type == ShapeType::kMarker ? marker : solid).push_back(s);
  }
  if (!marker.empty()) {
    HBITMAP l = MakeShapeLayer(marker, SIZE{w, h}, POINT{sel_.left, sel_.top}, true);
    HDC ldc = CreateCompatibleDC(nullptr);
    DrawLayer(dst, l, ldc, 0, 0, w, h);
    DeleteDC(ldc);
    if (l) DeleteObject(l);
  }
  if (!solid.empty()) {
    HBITMAP l = MakeShapeLayer(solid, SIZE{w, h}, POINT{sel_.left, sel_.top}, false);
    HDC ldc = CreateCompatibleDC(nullptr);
    DrawLayer(dst, l, ldc, 0, 0, w, h);
    DeleteDC(ldc);
    if (l) DeleteObject(l);
  }
  SelectObject(dst, old);
  DeleteDC(dst);
  return out;
}

void OverlayWindow::Commit() {
  OverlayResult r;
  r.cropped = FlattenSelection();
  r.origin = POINT{screen_->bounds().left + sel_.left,
                   screen_->bounds().top + sel_.top};
  if (!r.cropped) return;
  if (on_finish_) on_finish_(r);
  DestroyWindow(hwnd_);
}

void OverlayWindow::CopySel() {
  HBITMAP bmp = FlattenSelection();
  if (bmp) {
    clipboard::CopyImage(bmp);
    DeleteObject(bmp);
  }
  DestroyWindow(hwnd_);
}

void OverlayWindow::DoSave() {
  HBITMAP bmp = FlattenSelection();
  if (!bmp) return;
  wchar_t file[MAX_PATH] = L"";
  SYSTEMTIME st{};
  GetLocalTime(&st);
  swprintf_s(file, L"StickySnap_%04u%02u%02u_%02u%02u%02u.png", st.wYear,
             st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd_;
  ofn.lpstrFilter =
      L"PNG 图片 (*.png)\0*.png\0JPEG 图片 (*.jpg)\0*.jpg\0所有文件 (*.*)\0*.*\0";
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrDefExt = L"png";
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
  const bool saved = GetSaveFileNameW(&ofn) && gdiutil::SaveImage(bmp, file);
  DeleteObject(bmp);
  if (saved) DestroyWindow(hwnd_);
}

void OverlayWindow::Cancel() {
  DestroyWindow(hwnd_);
}

void OverlayWindow::OnToolChanged(int tool) {
  tstate_.active_tool = tool;
  if (drawing_) {  // 切换工具时放弃进行中的笔画
    drawing_ = false;
    if (GetCapture() == hwnd_) ReleaseCapture();
    preview_ = Shape{};
    RECT r = sel_;
    InflateRect(&r, 6, 6);
    InvalidateRect(hwnd_, &r, FALSE);
  }
}

void OverlayWindow::OnAction(int id) {
  switch (id) {
    case ACT_UNDO:
      if (!shapes_.empty()) {
        shapes_.pop_back();
        shapes_dirty_ = true;
        RECT r = sel_;
        InflateRect(&r, 6, 6);
        InvalidateRect(hwnd_, &r, FALSE);
      }
      break;
    case ACT_PIN:
      Commit();
      break;
    case ACT_COPY:
      CopySel();
      break;
    case ACT_SAVE:
      DoSave();
      break;
    case ACT_CANCEL:
      Cancel();
      break;
    default:
      break;
  }
}

void OverlayWindow::ResetEdit() {
  shapes_.clear();
  shapes_dirty_ = true;
  if (canvas_) {
    SelectObject(canvas_dc_, GetStockObject(NULL_BRUSH));
    DeleteObject(canvas_);
    canvas_ = nullptr;
  }
  if (canvas_m_) {
    SelectObject(canvas_m_dc_, GetStockObject(NULL_BRUSH));
    DeleteObject(canvas_m_);
    canvas_m_ = nullptr;
  }
  canvas_size_ = SIZE{0, 0};
  if (mosaic_) {
    SelectObject(mosaic_dc_, GetStockObject(NULL_BRUSH));
    DeleteObject(mosaic_);
    mosaic_ = nullptr;
  }
  preview_ = Shape{};
}

Shape OverlayWindow::MakePreview() const {
  Shape s;
  s.type = static_cast<ShapeType>(util::Clamp(tstate_.active_tool, 0,
                                              static_cast<int>(TOOL_COUNT) - 1));
  s.color = kPalette[util::Clamp(tstate_.color, 0, kColorCount - 1)];
  s.width = kWidths[util::Clamp(tstate_.width, 0, kWidthCount - 1)];
  return s;
}

bool OverlayWindow::EnsureCanvases() const {
  const int w = sel_.right - sel_.left;
  const int h = sel_.bottom - sel_.top;
  if (w <= 0 || h <= 0) return false;
  if (!canvas_dc_) {
    canvas_dc_ = CreateCompatibleDC(nullptr);
    canvas_m_dc_ = CreateCompatibleDC(nullptr);
  }
  if (canvas_size_.cx != w || canvas_size_.cy != h) {
    canvas_size_ = SIZE{w, h};
    shapes_dirty_ = true;
  }
  if (!shapes_dirty_) return true;

  std::vector<Shape> solid, marker;
  for (const auto& s : shapes_) {
    if (s.type == ShapeType::kMosaic) continue;
    (s.type == ShapeType::kMarker ? marker : solid).push_back(s);
  }
  if (canvas_) {
    SelectObject(canvas_dc_, GetStockObject(NULL_BRUSH));  // 先解除选中再删，否则静默失败泄漏
    DeleteObject(canvas_);
    canvas_ = nullptr;
  }
  if (canvas_m_) {
    SelectObject(canvas_m_dc_, GetStockObject(NULL_BRUSH));
    DeleteObject(canvas_m_);
    canvas_m_ = nullptr;
  }
  // 形状是客户区坐标，画布原点 = 选区左上角
  canvas_ = MakeShapeLayer(solid, SIZE{w, h}, POINT{sel_.left, sel_.top}, false);
  canvas_m_ = MakeShapeLayer(marker, SIZE{w, h}, POINT{sel_.left, sel_.top}, true);
  if (canvas_) SelectObject(canvas_dc_, canvas_);
  if (canvas_m_) SelectObject(canvas_m_dc_, canvas_m_);
  shapes_dirty_ = false;
  return true;
}

bool OverlayWindow::EnsureMosaic() const {
  if (mosaic_) return true;
  const int w = sel_.right - sel_.left;
  const int h = sel_.bottom - sel_.top;
  if (w <= 0 || h <= 0 || !screen_->bits()) return false;

  BITMAPINFO bi{};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  mosaic_ = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!mosaic_ || !bits) return false;
  if (!mosaic_dc_) mosaic_dc_ = CreateCompatibleDC(nullptr);
  SelectObject(mosaic_dc_, mosaic_);

  // 选区内容按 10px 块取平均 → 像素化
  constexpr int kBlock = 10;
  const uint8_t* src = screen_->bits();
  const int sw = screen_->width();
  for (int by = 0; by < h; by += kBlock) {
    for (int bx = 0; bx < w; bx += kBlock) {
      const int yend = by + kBlock < h ? by + kBlock : h;
      const int xend = bx + kBlock < w ? bx + kBlock : w;
      int sr = 0, sg = 0, sb = 0, cnt = 0;
      for (int y = by; y < yend; ++y) {
        for (int x = bx; x < xend; ++x) {
          const uint8_t* p = src + (static_cast<size_t>(y) * sw + x) * 4;
          sb += p[0];
          sg += p[1];
          sr += p[2];
          ++cnt;
        }
      }
      if (!cnt) continue;
      sr /= cnt;
      sg /= cnt;
      sb /= cnt;
      for (int y = by; y < yend; ++y) {
        for (int x = bx; x < xend; ++x) {
          uint8_t* q = static_cast<uint8_t*>(bits) +
                       (static_cast<size_t>(y) * w + x) * 4;
          q[0] = static_cast<uint8_t>(sb);
          q[1] = static_cast<uint8_t>(sg);
          q[2] = static_cast<uint8_t>(sr);
          q[3] = 255;
        }
      }
    }
  }
  return true;
}

void OverlayWindow::ShowToolbar() {
  tstate_.accent = st_.accent;
  tstate_.accent_dark = st_.accent_dark;
  if (!toolbar_) {
    toolbar_ = ToolbarWindow::Create(
        inst_, &tstate_, [this](int t) { OnToolChanged(t); },
        [this](int a) { OnAction(a); }, [this](int) {}, [this](int) {});
    if (!toolbar_) return;
  }
  const RECT b = screen_->bounds();
  const SIZE ts = toolbar_->size();
  const int sw = screen_->width();
  const int sh = screen_->height();
  int tx = sel_.right - ts.cx;
  if (tx < 0) tx = 0;
  if (tx + ts.cx > sw) tx = sw - ts.cx;
  int ty = sel_.bottom + 10;
  if (ty + ts.cy > sh) ty = sel_.top - ts.cy - 10;
  if (ty < 0) ty = sel_.bottom - ts.cy - 10;
  if (ty < 0) ty = 0;
  toolbar_->ShowAt(b.left + tx, b.top + ty);
}

SIZE OverlayWindow::MeasureLabel(HDC dc, const std::wstring& text) const {
  SIZE sz{};
  HFONT f = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  HGDIOBJ old = SelectObject(dc, f);
  GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &sz);
  SelectObject(dc, old);
  return sz;
}

void OverlayWindow::DrawLabel(HDC dc, const std::wstring& text, int x, int y) const {
  HFONT f = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  HGDIOBJ old = SelectObject(dc, f);
  SIZE sz{};
  GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &sz);
  RECT box{x, y, x + sz.cx + 12, y + sz.cy + 6};
  FillRect(dc, &box, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(255, 255, 255));
  RECT tbox{x + 6, y + 3, box.right, box.bottom};
  DrawTextW(dc, text.c_str(), -1, &tbox, DT_LEFT | DT_TOP | DT_SINGLELINE);
  SelectObject(dc, old);
}

void OverlayWindow::DrawSizeLabel(HDC dc) {
  wchar_t t[32];
  swprintf_s(t, L"%d × %d", sel_.right - sel_.left, sel_.bottom - sel_.top);
  SIZE sz = MeasureLabel(dc, t);
  const int w = screen_->width();
  const int h = screen_->height();
  int x = sel_.right - sz.cx - 12;
  if (x < 0) x = 0;
  int y = sel_.bottom + 4;
  if (y + sz.cy + 6 > h) y = sel_.top - sz.cy - 10;
  if (y < 0) y = sel_.top + 4;
  DrawLabel(dc, t, x, y);
}

void OverlayWindow::DrawHintLabel(HDC dc) {
  const std::wstring hint = L"Enter 贴图 · Ctrl+C 复制 · Ctrl+Z 撤销 · Esc 取消";
  SIZE sz = MeasureLabel(dc, hint);
  const int w = screen_->width();
  const int h = screen_->height();
  int x = (sel_.left + sel_.right) / 2 - (sz.cx + 12) / 2;
  x = util::Clamp(x, 0, w - sz.cx - 12);
  int y = sel_.bottom + 30;
  if (y + sz.cy + 6 > h) y = sel_.top - sz.cy - 36;
  if (y < 0) y = sel_.top + 30;
  DrawLabel(dc, hint, x, y);
}

void OverlayWindow::DrawMagnifier(HDC dc) {
  constexpr int kSrc = 9;
  constexpr int kScale = 12;
  constexpr int kBox = kSrc * kScale;
  constexpr int kMargin = kSrc / 2;
  const int w = screen_->width();
  const int h = screen_->height();
  if (cursor_.x - kMargin < 0 || cursor_.y - kMargin < 0) return;

  int bx = cursor_.x + 24;
  int by = cursor_.y + 24;
  if (bx + kBox > w) bx = cursor_.x - 24 - kBox;
  if (by + kBox > h) by = cursor_.y - 24 - kBox;
  if (bx < 0 || by < 0) return;

  const RECT b = screen_->bounds();
  SetStretchBltMode(dc, COLORONCOLOR);
  StretchBlt(dc, bx, by, kBox, kBox, screen_->mem_dc(),
             cursor_.x + b.left - kMargin, cursor_.y + b.top - kMargin,
             kSrc, kSrc, SRCCOPY);

  HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
  HGDIOBJ open_ = SelectObject(dc, pen);
  HGDIOBJ obr = SelectObject(dc, GetStockObject(NULL_BRUSH));
  Rectangle(dc, bx, by, bx + kBox, by + kBox);
  SelectObject(dc, obr);
  SelectObject(dc, open_);
  DeleteObject(pen);

  const int cx = bx + kBox / 2;
  const int cy = by + kBox / 2;
  HPEN cross = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
  open_ = SelectObject(dc, cross);
  SetROP2(dc, R2_NOTXORPEN);
  MoveToEx(dc, cx - 8, cy, nullptr);
  LineTo(dc, cx + 8, cy);
  MoveToEx(dc, cx, cy - 8, nullptr);
  LineTo(dc, cx, cy + 8);
  SetROP2(dc, R2_COPYPEN);
  SelectObject(dc, open_);
  DeleteObject(cross);

  const COLORREF c = screen_->PixelAt(cursor_.x + b.left, cursor_.y + b.top);
  wchar_t t[16];
  swprintf_s(t, L"#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
  DrawLabel(dc, t, bx, by + kBox + 4);
}
