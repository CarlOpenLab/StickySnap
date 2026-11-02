#include "pin.h"

#include <commdlg.h>
#include <windowsx.h>

#include "clipboard.h"
#include "config.h"
#include "gdiutil.h"
#include "pinmanager.h"
#include "util.h"

#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(w) ((short)HIWORD(w))
#endif

namespace {
constexpr int kAlphaMax = 255;
}  // namespace

void PinWindow::RegisterClass(HINSTANCE inst) {
  static bool done = false;
  if (done) return;
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_DBLCLKS;
  wc.lpfnWndProc = WndProcStatic;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.lpszClassName = config::kPinClass;
  RegisterClassExW(&wc);
  done = true;
}

PinWindow::PinWindow(PinManager* mgr, HBITMAP bmp, std::wstring img_path)
    : mgr_(mgr), bmp_(bmp), img_path_(std::move(img_path)) {}

PinWindow::~PinWindow() {
  if (bmp_) DeleteObject(bmp_);
}

PinWindow* PinWindow::Create(HINSTANCE inst, PinManager* mgr, HBITMAP bmp,
                             const std::wstring& img_path, int x, int y,
                             const PinSession* init) {
  RegisterClass(inst);
  auto* p = new PinWindow(mgr, bmp, img_path);
  BITMAP bm{};
  if (!GetObjectW(bmp, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0) {
    delete p;
    return nullptr;
  }
  p->bw_ = bm.bmWidth;
  p->bh_ = bm.bmHeight;

  p->hwnd_ = CreateWindowExW(
      WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
      config::kPinClass, L"", WS_POPUP, x, y, p->bw_, p->bh_, nullptr, nullptr,
      inst, p);
  if (!p->hwnd_) {
    delete p;
    return nullptr;
  }
  util::ExcludeFromCapture(p->hwnd_);  // 贴图不出现在后续截图里
  SetLayeredWindowAttributes(p->hwnd_, 0, static_cast<BYTE>(p->alpha_), LWA_ALPHA);

  if (init) {
    p->zoom_ = util::Clamp(init->zoom, config::kZoomMin, config::kZoomMax);
    p->alpha_ = util::Clamp(init->alpha, config::kAlphaMin, kAlphaMax);
    SetLayeredWindowAttributes(p->hwnd_, 0, static_cast<BYTE>(p->alpha_), LWA_ALPHA);
    if (init->pass) p->SetPass(true);
    p->ApplyZoom();
    SetWindowPos(p->hwnd_, nullptr, init->x, init->y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
  }
  ShowWindow(p->hwnd_, SW_SHOWNOACTIVATE);
  return p;
}

void PinWindow::Close() {
  if (hwnd_) DestroyWindow(hwnd_);
}

PinSession PinWindow::Session() const {
  PinSession s;
  s.img = img_path_;
  RECT r{};
  if (hwnd_) {
    GetWindowRect(hwnd_, &r);
    s.x = r.left;
    s.y = r.top;
  }
  s.zoom = zoom_;
  s.alpha = alpha_;
  s.pass = pass_;
  return s;
}

LRESULT CALLBACK PinWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wp,
                                          LPARAM lp) {
  if (msg == WM_NCCREATE) {
    auto* p = static_cast<PinWindow*>(
        reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(p));
  }
  auto* p = reinterpret_cast<PinWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!p) return DefWindowProcW(hwnd, msg, wp, lp);
  return p->Handle(hwnd, msg, wp, lp);
}

LRESULT PinWindow::Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_ERASEBKGND:
      return 1;

    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd_, &ps);
      HDC mem = CreateCompatibleDC(dc);
      HGDIOBJ old = SelectObject(mem, bmp_);
      const int w = static_cast<int>(bw_ * zoom_ + 0.5);
      const int h = static_cast<int>(bh_ * zoom_ + 0.5);
      SetStretchBltMode(dc, zoom_ < 1.0 ? COLORONCOLOR : HALFTONE);
      SetBrushOrgEx(dc, 0, 0, nullptr);
      if (w == bw_ && h == bh_) {
        BitBlt(dc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
      } else {
        StretchBlt(dc, 0, 0, w, h, mem, 0, 0, bw_, bh_, SRCCOPY);
      }
      SelectObject(mem, old);
      DeleteDC(mem);
      EndPaint(hwnd_, &ps);
      return 0;
    }

    case WM_NCHITTEST:
      return HTCLIENT;  // 整个窗口都是客户区，禁掉系统拖拽/双击行为

    case WM_LBUTTONDOWN:
      SetCapture(hwnd_);
      dragging_ = true;
      {
        POINT sp{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ClientToScreen(hwnd_, &sp);
        RECT r{};
        GetWindowRect(hwnd_, &r);
        drag_off_ = POINT{sp.x - r.left, sp.y - r.top};
      }
      return 0;

    case WM_MOUSEMOVE:
      if (dragging_) {
        POINT sp{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
        ClientToScreen(hwnd_, &sp);
        SetWindowPos(hwnd_, nullptr, sp.x - drag_off_.x, sp.y - drag_off_.y, 0,
                     0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      }
      return 0;

    case WM_LBUTTONUP:
      if (dragging_) {
        dragging_ = false;
        ReleaseCapture();
        mgr_->MarkDirty();
      }
      return 0;

    case WM_LBUTTONDBLCLK:
      mgr_->ClosePin(this);
      return 0;

    case WM_MOUSEWHEEL: {
      const short delta = GET_WHEEL_DELTA_WPARAM(wp);
      if (GetKeyState(VK_CONTROL) < 0) {
        alpha_ += delta > 0 ? config::kAlphaStep : -config::kAlphaStep;
        alpha_ = util::Clamp(alpha_, config::kAlphaMin, kAlphaMax);
        SetLayeredWindowAttributes(hwnd_, 0, static_cast<BYTE>(alpha_), LWA_ALPHA);
      } else {
        zoom_ = util::Clamp(delta > 0 ? zoom_ * config::kZoomStep
                                      : zoom_ / config::kZoomStep,
                            config::kZoomMin, config::kZoomMax);
        ApplyZoom();
      }
      mgr_->MarkDirty();
      return 0;
    }

    case WM_CONTEXTMENU: {
      int x = GET_X_LPARAM(lp);
      int y = GET_Y_LPARAM(lp);
      if (x == -1 && y == -1) {
        POINT p{};
        GetCursorPos(&p);
        x = p.x;
        y = p.y;
      }
      ShowMenu(x, y);
      return 0;
    }

    case WM_NCDESTROY:
      hwnd_ = nullptr;
      return 0;

    default:
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

void PinWindow::ApplyZoom() {
  int w = static_cast<int>(bw_ * zoom_ + 0.5);
  int h = static_cast<int>(bh_ * zoom_ + 0.5);
  if (w < 1) w = 1;
  if (h < 1) h = 1;
  SetWindowPos(hwnd_, nullptr, 0, 0, w, h,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void PinWindow::RestoreExStyle() {
  SetPass(pass_);
}

void PinWindow::SetPass(bool on) {
  pass_ = on;
  LONG_PTR ex = GetWindowLongPtrW(hwnd_, GWL_EXSTYLE);
  if (pass_)
    ex |= WS_EX_TRANSPARENT;
  else
    ex &= ~WS_EX_TRANSPARENT;
  SetWindowLongPtrW(hwnd_, GWL_EXSTYLE, ex);
}

void PinWindow::ShowMenu(int x, int y) {
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  const UINT passFlags = MF_STRING | (pass_ ? MF_CHECKED : MF_UNCHECKED);
  AppendMenuW(menu, passFlags, config::kPinMenuPass, L"鼠标穿透(&T)");
  AppendMenuW(menu, MF_STRING, config::kPinMenuCopy, L"复制(&C)");
  AppendMenuW(menu, MF_STRING, config::kPinMenuSave, L"另存为(&S)...");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, config::kPinMenuClose, L"关闭(&X)\t双击");

  SetForegroundWindow(hwnd_);  // 让菜单在点击别处时正常消失
  WORD cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                            x, y, 0, hwnd_, nullptr);
  DestroyMenu(menu);
  PostMessageW(hwnd_, WM_NULL, 0, 0);

  switch (cmd) {
    case config::kPinMenuPass:
      SetPass(!pass_);
      mgr_->MarkDirty();
      break;
    case config::kPinMenuCopy:
      clipboard::CopyImage(bmp_);
      break;
    case config::kPinMenuSave:
      SaveAs();
      break;
    case config::kPinMenuClose:
      mgr_->ClosePin(this);  // 之后 this 已释放，不得再访问成员
      return;
    default:
      break;
  }
}

void PinWindow::SaveAs() {
  wchar_t file[MAX_PATH] = L"";
  std::wstring name = img_path_.substr(img_path_.find_last_of(L"\\/") + 1);
  wcsncpy_s(file, name.c_str(), _TRUNCATE);
  OPENFILENAMEW ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = hwnd_;
  ofn.lpstrFilter = L"PNG 图片 (*.png)\0*.png\0JPEG 图片 (*.jpg)\0*.jpg\0所有文件 (*.*)\0*.*\0";
  ofn.lpstrFile = file;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrDefExt = L"png";
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
  if (GetSaveFileNameW(&ofn)) {
    gdiutil::SaveImage(bmp_, file);
  }
}
