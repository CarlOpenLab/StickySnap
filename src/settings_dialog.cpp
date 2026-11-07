#include "settings_dialog.h"

#include <commdlg.h>
#include <commctrl.h>

#include <string>

#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")  // msctls_updown32（InitCommonControlsEx）
#endif

namespace {

constexpr wchar_t kClassName[] = L"StickySnap.Settings";
constexpr wchar_t kTitle[] = L"StickySnap 设置";
constexpr int kClientW = 420;
constexpr int kClientH = 300;

// 控件 ID（对话框内部使用，与托盘菜单命令 ID 无关）
enum : int {
  IDC_HOT_SHOT = 1001,
  IDC_HOT_PIN = 1002,
  IDC_ACCENT = 1003,
  IDC_DIM = 1004,
  IDC_DIM_SPIN = 1005,
  IDC_PIN_BORDER = 1006,
  IDC_AUTOSTART = 1007,
};

// 对话框状态：挂在窗口 GWLP_USERDATA 上，生命周期止于 ShowSettingsDialog 返回
struct DlgState {
  Settings work;               // 工作副本：只有“保存”才写回调用方
  Settings* out = nullptr;     // 调用方指针，保存成功时整体拷回
  bool ok = false;             // 用户是否点了“保存”
  bool alive = true;           // 窗口是否仍然存在（WM_DESTROY 置 false）
  int capture = 0;             // 热键捕获目标：0=无，1=截图，2=贴图
  HWND cap_button = nullptr;   // 正在捕获的热键按钮
  HWND hot_shot = nullptr;
  HWND hot_pin = nullptr;
  HWND accent_btn = nullptr;
  HWND dim_edit = nullptr;
  HWND dim_spin = nullptr;
  HWND border_chk = nullptr;
  HWND autostart_chk = nullptr;
};

HWND g_open = nullptr;           // 同一时刻只允许存在一个设置窗
COLORREF g_custom_colors[16]{};  // ChooseColorW 自定义色板，进程内复用

// 捕获热键期间安装 WH_KEYBOARD_LL：Win+S / Win+D 这类 shell 组合会先到钩子，
// 因此能被捕获而不触发系统行为。钩子只在本进程的模态消息循环内工作，且
// 捕获结束/取消/关窗时立即卸载；Ctrl+Alt+Del 属于安全序列，任何钩子都拿不到。
constexpr UINT kCaptureMsg = WM_APP + 2;  // wParam=vk, LOWORD(lParam)=是否抬起, HIWORD(lParam)=修饰位
HHOOK g_ll_hook = nullptr;
HWND g_ll_target = nullptr;
UINT g_ll_mods = 0;  // 钩子里自行维护：被吞掉的按键不会更新 GetKeyState

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM wp, LPARAM lp) {
  if (code != HC_ACTION || !g_ll_target)
    return CallNextHookEx(g_ll_hook, code, wp, lp);
  const auto* k = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lp);
  const bool up = (k->flags & LLKHF_UP) != 0;
  const UINT vk = k->vkCode;

  UINT bit = 0;
  switch (vk) {
    case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: bit = MOD_CONTROL; break;
    case VK_MENU:    case VK_LMENU:    case VK_RMENU:    bit = MOD_ALT; break;
    case VK_SHIFT:   case VK_LSHIFT:   case VK_RSHIFT:   bit = MOD_SHIFT; break;
    case VK_LWIN:    case VK_RWIN:                       bit = MOD_WIN; break;
    default: break;
  }
  if (bit) {
    if (up) g_ll_mods &= ~bit;
    else g_ll_mods |= bit;
  }
  const LPARAM payload = (up ? 1 : 0) | (static_cast<LPARAM>(g_ll_mods) << 8);
  PostMessageW(g_ll_target, kCaptureMsg, static_cast<WPARAM>(vk), payload);
  return 1;  // 吞掉：既用于捕获，也阻止 Win/Alt 组合穿透到 shell
}

void InstallCaptureHook(HWND target) {
  g_ll_target = target;
  g_ll_mods = 0;
  if (!g_ll_hook) {
    g_ll_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc,
                                  GetModuleHandleW(nullptr), 0);
    if (!g_ll_hook) util::Debug(L"[settings] keyboard hook install failed %u", GetLastError());
  }
}

void UninstallCaptureHook() {
  if (g_ll_hook) {
    UnhookWindowsHookEx(g_ll_hook);
    g_ll_hook = nullptr;
  }
  g_ll_target = nullptr;
  g_ll_mods = 0;
}

DlgState* State(HWND hwnd) {
  return reinterpret_cast<DlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

// ---------- 控件创建 ----------

HWND MakeCtrl(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style,
              int x, int y, int width, int height, int id, DWORD exstyle = 0) {
  return CreateWindowExW(
      exstyle, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, parent,
      reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(parent, GWLP_HINSTANCE)), nullptr);
}

void MakeLabel(HWND parent, const wchar_t* text, int x, int y, int width, int height) {
  MakeCtrl(parent, L"STATIC", text, SS_LEFT, x, y, width, height, 0);
}

BOOL CALLBACK SetChildFont(HWND child, LPARAM font) {
  SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
  return TRUE;
}

void EnsureCommonControls() {
  static const bool once = [] {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_UPDOWN_CLASS;
    return InitCommonControlsEx(&icc) != FALSE;
  }();
  (void)once;  // 失败时只是没有数值微调箭头，其余功能照常
}

// ---------- 热键 ----------

void UpdateHotkeyLabels(const DlgState* st) {
  if (st->hot_shot) {
    const std::wstring t = settings::FormatHotkey(st->work.mod_shot, st->work.vk_shot);
    SetWindowTextW(st->hot_shot, t.c_str());
  }
  if (st->hot_pin) {
    const std::wstring t = settings::FormatHotkey(st->work.mod_pin, st->work.vk_pin);
    SetWindowTextW(st->hot_pin, t.c_str());
  }
}

bool IsModifierVk(UINT vk) {
  switch (vk) {
    case VK_CONTROL:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_MENU:
    case VK_LMENU:
    case VK_RMENU:
    case VK_SHIFT:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LWIN:
    case VK_RWIN:
      return true;
    default:
      return false;
  }
}

// 可作主键的虚拟键：F1-F24、A-Z、0-9 以及 FormatHotkey 能命名的常用键
bool IsMainKey(UINT vk) {
  if (vk >= VK_F1 && vk <= VK_F24) return true;
  if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return true;
  switch (vk) {
    case VK_SNAPSHOT:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:
    case VK_NEXT:
    case VK_SPACE:
    case VK_BACK:
      return true;
    default:
      return false;
  }
}

UINT CurrentMods() {
  UINT mod = 0;
  if (GetKeyState(VK_CONTROL) & 0x8000) mod |= MOD_CONTROL;
  if (GetKeyState(VK_MENU) & 0x8000) mod |= MOD_ALT;
  if (GetKeyState(VK_SHIFT) & 0x8000) mod |= MOD_SHIFT;
  if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000)) mod |= MOD_WIN;
  return mod;
}

std::wstring ModPrefix(UINT mod) {
  std::wstring out;
  if (mod & MOD_CONTROL) out += L"Ctrl+";
  if (mod & MOD_ALT) out += L"Alt+";
  if (mod & MOD_SHIFT) out += L"Shift+";
  if (mod & MOD_WIN) out += L"Win+";
  return out;
}

void CancelCapture(DlgState* st) {
  UninstallCaptureHook();
  st->capture = 0;
  st->cap_button = nullptr;
  UpdateHotkeyLabels(st);
}

void ShowCaptureHint(DlgState* st, UINT mods) {
  if (!st->cap_button) return;
  if (!mods) {
    SetWindowTextW(st->cap_button, L"按下快捷键…");
    return;
  }
  std::wstring t = ModPrefix(mods);
  t += L"…";
  SetWindowTextW(st->cap_button, t.c_str());
}

void BeginCapture(DlgState* st, int which, HWND btn) {
  UpdateHotkeyLabels(st);  // 中止上一次未完成的捕获并恢复文案
  st->capture = which;
  st->cap_button = btn;
  SetFocus(btn);
  InstallCaptureHook(GetParent(btn));
  ShowCaptureHint(st, 0);
}

// 捕获期间直接消费键盘消息：Esc 只取消捕获，不会关窗
void ProcessCaptureKey(DlgState* st, UINT message, UINT vk, UINT mods) {
  if (message == WM_KEYUP || message == WM_SYSKEYUP) {
    if (IsModifierVk(vk)) ShowCaptureHint(st, mods);
    return;
  }
  if (vk == VK_ESCAPE) {  // Esc 不作主键，仅取消
    CancelCapture(st);
    return;
  }
  if (IsModifierVk(vk)) {  // 修饰键：只更新提示，继续等主键
    ShowCaptureHint(st, mods);
    return;
  }
  if (!IsMainKey(vk)) return;  // 无效主键：忽略，保持捕获状态
  if (st->capture == 1) {
    st->work.mod_shot = mods;
    st->work.vk_shot = vk;
  } else {
    st->work.mod_pin = mods;
    st->work.vk_pin = vk;
  }
  UninstallCaptureHook();
  st->capture = 0;
  st->cap_button = nullptr;
  UpdateHotkeyLabels(st);
}

// ---------- 强调色 ----------

void DrawAccentButton(const DlgState* st, const DRAWITEMSTRUCT* dis) {
  HDC dc = dis->hDC;
  const RECT rc = dis->rcItem;
  FillRect(dc, &rc, GetSysColorBrush(COLOR_BTNFACE));

  // 左侧色块
  constexpr int kInset = 5;
  RECT sw = rc;
  sw.left += kInset;
  sw.top += kInset;
  sw.bottom -= kInset;
  sw.right = sw.left + (rc.bottom - rc.top) - kInset * 2;
  HBRUSH brush = CreateSolidBrush(st->work.accent);
  FillRect(dc, &sw, brush);
  DeleteObject(brush);
  DrawEdge(dc, &sw, EDGE_SUNKEN, BF_RECT);

  // 右侧 #RRGGBB 文案
  wchar_t text[16];
  wsprintfW(text, L"#%02X%02X%02X", GetRValue(st->work.accent),
            GetGValue(st->work.accent), GetBValue(st->work.accent));
  RECT tr = rc;
  tr.left = sw.right + 8;
  HGDIOBJ old_font = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
  const int old_mode = SetBkMode(dc, TRANSPARENT);
  const COLORREF old_color = SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
  DrawTextW(dc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
  SetTextColor(dc, old_color);
  SetBkMode(dc, old_mode);
  SelectObject(dc, old_font);

  if (dis->itemState & ODS_FOCUS) {
    RECT fr = rc;
    InflateRect(&fr, -2, -2);
    DrawFocusRect(dc, &fr);
  }
}

void PickAccent(HWND hwnd, DlgState* st) {
  CHOOSECOLORW cc{};
  cc.lStructSize = sizeof(cc);
  cc.hwndOwner = hwnd;
  cc.rgbResult = st->work.accent;
  cc.lpCustColors = g_custom_colors;
  cc.Flags = CC_FULLOPEN | CC_RGBINIT;
  if (!ChooseColorW(&cc)) return;
  st->work.accent = cc.rgbResult;
  st->work.accent_dark = settings::Darken(st->work.accent, 0.55);
  InvalidateRect(st->accent_btn, nullptr, TRUE);
}

// ---------- 保存 ----------

void SaveAndClose(HWND hwnd, DlgState* st) {
  BOOL translated = FALSE;
  UINT dim = GetDlgItemInt(hwnd, IDC_DIM, &translated, FALSE);
  if (!translated) dim = static_cast<UINT>(st->work.dim_percent);
  if (dim < 10) dim = 10;
  if (dim > 90) dim = 90;
  st->work.dim_percent = static_cast<int>(dim);
  st->work.pin_border =
      SendMessageW(st->border_chk, BM_GETCHECK, 0, 0) == BST_CHECKED;
  st->work.autostart =
      SendMessageW(st->autostart_chk, BM_GETCHECK, 0, 0) == BST_CHECKED;
  st->work.accent_dark = settings::Darken(st->work.accent, 0.55);

  settings::Save(st->work);
  if (st->out) *st->out = st->work;
  st->ok = true;
  DestroyWindow(hwnd);
}

// ---------- 窗口过程 ----------

void FocusNext(HWND hwnd, HWND current, bool backward) {
  const HWND from = (current && current != hwnd) ? current : nullptr;
  const HWND next = GetNextDlgTabItem(hwnd, from, backward ? TRUE : FALSE);
  if (next) SetFocus(next);
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_NCCREATE: {
      const CREATESTRUCTW* cs = reinterpret_cast<const CREATESTRUCTW*>(lp);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
      break;
    }
    case WM_CREATE: {
      DlgState* st = State(hwnd);
      if (!st) return -1;

      MakeLabel(hwnd, L"截图热键", 16, 21, 92, 18);
      st->hot_shot = MakeCtrl(hwnd, L"BUTTON", L"", WS_TABSTOP | BS_PUSHBUTTON, 112,
                              16, 210, 26, IDC_HOT_SHOT);
      MakeLabel(hwnd, L"贴图热键", 16, 55, 92, 18);
      st->hot_pin = MakeCtrl(hwnd, L"BUTTON", L"", WS_TABSTOP | BS_PUSHBUTTON, 112,
                             50, 210, 26, IDC_HOT_PIN);

      MakeLabel(hwnd, L"强调色", 16, 89, 92, 18);
      st->accent_btn =
          MakeCtrl(hwnd, L"BUTTON", L"", WS_TABSTOP | BS_OWNERDRAW, 112, 84, 150, 26,
                   IDC_ACCENT);

      MakeLabel(hwnd, L"遮罩变暗", 16, 123, 92, 18);
      st->dim_edit = MakeCtrl(hwnd, L"EDIT", L"",
                              WS_TABSTOP | ES_NUMBER | ES_AUTOHSCROLL, 112, 118, 60,
                              24, IDC_DIM, WS_EX_CLIENTEDGE);
      EnsureCommonControls();
      st->dim_spin = CreateWindowExW(
          0, UPDOWN_CLASSW, nullptr,
          WS_CHILD | WS_VISIBLE | UDS_ALIGNRIGHT | UDS_SETBUDDYINT | UDS_ARROWKEYS |
              UDS_NOTHOUSANDS,
          0, 0, 0, 0, hwnd,
          reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_DIM_SPIN)),
          reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(hwnd, GWLP_HINSTANCE)),
          nullptr);
      if (st->dim_spin) {
        SendMessageW(st->dim_spin, UDM_SETBUDDY,
                     reinterpret_cast<WPARAM>(st->dim_edit), 0);
        SendMessageW(st->dim_spin, UDM_SETRANGE32, 10, 90);
        SendMessageW(st->dim_spin, UDM_SETPOS32, 0,
                     static_cast<LPARAM>(st->work.dim_percent));
      } else {
        SetDlgItemInt(hwnd, IDC_DIM, static_cast<UINT>(st->work.dim_percent), FALSE);
      }

      st->border_chk = MakeCtrl(hwnd, L"BUTTON", L"贴图显示边框",
                                WS_TABSTOP | BS_AUTOCHECKBOX, 112, 154, 210, 22,
                                IDC_PIN_BORDER);
      st->autostart_chk = MakeCtrl(hwnd, L"BUTTON", L"开机自启",
                                   WS_TABSTOP | BS_AUTOCHECKBOX, 112, 180, 210, 22,
                                   IDC_AUTOSTART);
      MakeLabel(hwnd, L"提示：点击热键按钮后按下组合键；Esc 取消捕获", 16, 214, 388, 18);

      MakeCtrl(hwnd, L"BUTTON", L"保存", WS_TABSTOP | BS_DEFPUSHBUTTON, 218, 252, 88,
               28, IDOK);
      MakeCtrl(hwnd, L"BUTTON", L"取消", WS_TABSTOP | BS_PUSHBUTTON, 314, 252, 88, 28,
               IDCANCEL);

      SendMessageW(st->border_chk, BM_SETCHECK,
                   st->work.pin_border ? BST_CHECKED : BST_UNCHECKED, 0);
      SendMessageW(st->autostart_chk, BM_SETCHECK,
                   st->work.autostart ? BST_CHECKED : BST_UNCHECKED, 0);
      UpdateHotkeyLabels(st);
      EnumChildWindows(hwnd, SetChildFont,
                       reinterpret_cast<LPARAM>(GetStockObject(DEFAULT_GUI_FONT)));
      return 0;
    }
    case WM_DRAWITEM: {
      const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lp);
      const DlgState* st = State(hwnd);
      if (dis && st && dis->CtlID == IDC_ACCENT) {
        DrawAccentButton(st, dis);
        return TRUE;
      }
      break;
    }
    case WM_COMMAND: {
      DlgState* st = State(hwnd);
      if (!st) break;
      const int id = LOWORD(wp);
      if (id == IDOK) {
        SaveAndClose(hwnd, st);
        return 0;
      }
      if (id == IDCANCEL) {
        DestroyWindow(hwnd);
        return 0;
      }
      if (HIWORD(wp) == BN_CLICKED) {
        switch (id) {
          case IDC_HOT_SHOT:
            BeginCapture(st, 1, st->hot_shot);
            return 0;
          case IDC_HOT_PIN:
            BeginCapture(st, 2, st->hot_pin);
            return 0;
          case IDC_ACCENT:
            PickAccent(hwnd, st);
            return 0;
          default:
            break;
        }
      }
      break;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
      // 标签/复选框透明背景，和窗口底色一致
      SetBkMode(reinterpret_cast<HDC>(wp), TRANSPARENT);
      SetTextColor(reinterpret_cast<HDC>(wp), GetSysColor(COLOR_BTNTEXT));
      return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
    }
    case WM_CLOSE:
      DestroyWindow(hwnd);
      return 0;
    case WM_DESTROY: {
      UninstallCaptureHook();  // 任何退出路径都不能留下全局键盘钩子
      DlgState* st = State(hwnd);
      if (st) st->alive = false;
      return 0;
    }
    default:
      break;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

bool RegisterSettingsClass(HINSTANCE inst) {
  static bool registered = false;
  if (registered) return true;
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.style = CS_DBLCLKS;
  wc.lpfnWndProc = SettingsProc;
  wc.hInstance = inst;
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
  wc.hbrBackground =
      reinterpret_cast<HBRUSH>(static_cast<INT_PTR>(COLOR_BTNFACE + 1));
  wc.lpszClassName = kClassName;
  registered =
      RegisterClassExW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
  return registered;
}

void PlaceWindow(HWND hwnd, HWND owner) {
  RECT wnd{};
  if (!GetWindowRect(hwnd, &wnd)) return;
  const int w = wnd.right - wnd.left;
  const int h = wnd.bottom - wnd.top;

  RECT ref{};
  bool have_ref = false;
  if (owner && IsWindow(owner) && !IsIconic(owner) && GetWindowRect(owner, &ref) &&
      ref.right > ref.left && ref.bottom > ref.top) {
    have_ref = true;
  }

  MONITORINFO mi{};
  mi.cbSize = sizeof(mi);
  RECT work{};
  if (GetMonitorInfoW(MonitorFromWindow(owner ? owner : hwnd, MONITOR_DEFAULTTONEAREST),
                      &mi)) {
    work = mi.rcWork;
  } else {
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
  }
  if (!have_ref) ref = work;

  int x = ref.left + (ref.right - ref.left - w) / 2;
  int y = ref.top + (ref.bottom - ref.top - h) / 2;
  // 夹到工作区，避免高 DPI 或多显示器下跑到屏幕外
  if (x + w > work.right) x = work.right - w;
  if (y + h > work.bottom) y = work.bottom - h;
  if (x < work.left) x = work.left;
  if (y < work.top) y = work.top;
  SetWindowPos(hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

}  // namespace

bool ShowSettingsDialog(HWND owner, Settings* s) {
  if (!s) return false;
  if (g_open && IsWindow(g_open)) {  // 已有一个设置窗在前台，不重复打开
    SetForegroundWindow(g_open);
    return false;
  }

  const HINSTANCE inst = reinterpret_cast<HINSTANCE>(GetModuleHandleW(nullptr));
  if (!RegisterSettingsClass(inst)) {
    util::Debug(L"settings dialog: RegisterClassExW 失败 err=%u", GetLastError());
    return false;
  }

  DlgState st;
  st.work = *s;
  st.out = s;

  RECT rc{0, 0, kClientW, kClientH};
  const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
  const DWORD exstyle = WS_EX_DLGMODALFRAME;
  AdjustWindowRectEx(&rc, style, FALSE, exstyle);
  const int w = rc.right - rc.left;
  const int h = rc.bottom - rc.top;

  HWND hwnd = CreateWindowExW(exstyle, kClassName, kTitle, style, 0, 0, w, h, owner,
                              nullptr, inst, &st);
  if (!hwnd) {
    util::Debug(L"settings dialog: CreateWindowExW 失败 err=%u", GetLastError());
    return false;
  }
  g_open = hwnd;
  PlaceWindow(hwnd, owner);

  const bool disable_owner =
      owner != nullptr && IsWindow(owner) != FALSE && IsWindowEnabled(owner) != FALSE;
  if (disable_owner) EnableWindow(owner, FALSE);

  ShowWindow(hwnd, SW_SHOW);
  UpdateWindow(hwnd);
  SetForegroundWindow(hwnd);
  if (st.hot_shot) SetFocus(st.hot_shot);

  // 模态循环：GetMessage 直到窗口销毁；热键捕获期间键盘消息在这里优先消费
  bool repost_quit = false;
  MSG msg{};
  while (st.alive) {
    const BOOL got = GetMessageW(&msg, nullptr, 0, 0);
    if (got == 0) {
      repost_quit = true;  // 线程退出信号：清理后补发，别吞掉主循环的 WM_QUIT
      break;
    }
    if (got == -1) break;

    // 低级键盘钩子上报的捕获按键（Win+S 等被 shell 占用的组合也走这里）
    if (st.capture && msg.message == kCaptureMsg) {
      const UINT vk = static_cast<UINT>(msg.wParam);
      const bool up = (msg.lParam & 1) != 0;
      const UINT mods = static_cast<UINT>((msg.lParam >> 8) & 0xFF);
      ProcessCaptureKey(&st, up ? WM_KEYUP : WM_KEYDOWN, vk, mods);
      continue;
    }

    const bool own =
        msg.hwnd == hwnd || (msg.hwnd != nullptr && IsChild(hwnd, msg.hwnd) != FALSE);
    if (st.capture) {
      if (GetFocus() != st.cap_button) {
        CancelCapture(&st);  // 焦点移走：放弃捕获，保持原热键
      } else if (own && (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN ||
                         msg.message == WM_KEYUP || msg.message == WM_SYSKEYUP)) {
        // 钩子未安装成功时的后备路径
        ProcessCaptureKey(&st, msg.message, static_cast<UINT>(msg.wParam), CurrentMods());
        continue;
      }
    }

    if (own && !st.capture && msg.message == WM_KEYDOWN) {
      const UINT vk = static_cast<UINT>(msg.wParam);
      if (vk == VK_TAB) {
        FocusNext(hwnd, GetFocus(), (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        continue;
      }
      if (vk == VK_RETURN) {
        SendMessageW(hwnd, WM_COMMAND, IDOK, 0);
        continue;
      }
      if (vk == VK_ESCAPE) {
        SendMessageW(hwnd, WM_COMMAND, IDCANCEL, 0);
        continue;
      }
    }

    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  if (IsWindow(hwnd)) DestroyWindow(hwnd);
  g_open = nullptr;
  if (disable_owner && IsWindow(owner)) {
    EnableWindow(owner, TRUE);
    if (IsWindowVisible(owner) && !IsIconic(owner)) SetForegroundWindow(owner);
  }
  if (repost_quit) PostQuitMessage(static_cast<int>(msg.wParam));
  return st.ok;
}
