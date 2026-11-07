#include "app.h"

#include "autostart.h"
#include "clipboard.h"
#include "config.h"
#include "overlay.h"
#include "pin.h"
#include "settings_dialog.h"
#include "util.h"

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

int App::Run(HINSTANCE inst) {
  inst_ = inst;
  settings_ = settings::Load();  // 读取用户配置（首次运行生成默认文件）

  PinWindow::SetBorderEnabled(settings_.pin_border);  // 贴图常显边框
  autostart::SetEnabled(settings_.autostart);         // 配置文件是自启的权威来源

  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = WndProcStatic;
  wc.hInstance = inst;
  wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
  wc.lpszClassName = config::kClassName;
  if (!RegisterClassExW(&wc)) return 1;

  hwnd_ = CreateWindowExW(0, config::kClassName, config::kAppName,
                          WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          400, 300, nullptr, nullptr, inst, this);
  if (!hwnd_) return 1;

  pins_.Init(inst);
  pins_.SetOwner(hwnd_);

  tray_cb_ = [this](WORD cmd) { PostMessageW(hwnd_, WM_COMMAND, MAKEWPARAM(cmd, 0), 0); };
  tray_.Create(inst_, hwnd_, config::kTrayMsg, tray_cb_);

  RegisterHotkeys();

  taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");
  pins_.Restore();  // 恢复上次会话的贴图

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK App::WndProcStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_NCCREATE) {
    auto* app = static_cast<App*>(
        reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
  }
  auto* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!app) return DefWindowProcW(hwnd, msg, wp, lp);
  return app->Handle(hwnd, msg, wp, lp);
}

LRESULT App::Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_HOTKEY:
      if (wp == config::kHotkeyShot)
        StartCapture();
      else if (wp == config::kHotkeyPin)
        PinClipboard();
      return 0;

    case WM_COMMAND:
      switch (LOWORD(wp)) {
        case config::kCmdShot:
          StartCapture();
          break;
        case config::kCmdPaste:
          PinClipboard();
          break;
        case config::kCmdConfig:
          OpenConfig();
          break;
        case config::kCmdResetPass:
          pins_.SetAllPass(false);
          break;
        case config::kCmdSettings:
          OpenSettings();
          break;
        case config::kCmdAutostart:
          ToggleAutostart();
          break;
        case config::kCmdLog:
          OpenLog();
          break;
        case config::kCmdAbout:
          ShowAbout();
          break;
        case config::kCmdQuit:
          DestroyWindow(hwnd_);
          break;
        default:
          break;
      }
      return 0;

    case config::kTrayMsg:
      // 旧版 NOTIFYICONDATA：lParam 为鼠标消息。
      // 左键直接截图（对齐 Snipaste/PixPin 的习惯），右键弹菜单。
      if (lp == WM_LBUTTONUP)
        StartCapture();
      else if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU)
        tray_.ShowMenu(settings_);
      return 0;

    case WM_TIMER:
      if (wp == config::kTimerSave) pins_.FlushIfDirty();
      return 0;

    case WM_DISPLAYCHANGE:
      // 拔插显示器/改分辨率：把贴图拉回可见区域
      pins_.ClampToVirtualScreen();
      return 0;

    case WM_DESTROY:
      KillTimer(hwnd_, config::kTimerSave);
      UnregisterHotKey(hwnd_, config::kHotkeyShot);
      UnregisterHotKey(hwnd_, config::kHotkeyPin);
      // 顺序关键：先落盘（此时窗口还在，能取到位置），再关窗且不标记 dirty，
      // 否则 CloseAll 之后的 FlushIfDirty 会把空列表写回会话。
      pins_.SaveNow();
      pins_.CloseAll(false);
      tray_.Remove();
      PostQuitMessage(0);
      return 0;

    default:
      if (msg == taskbar_created_) {
        RestoreTray();
        return 0;
      }
      return DefWindowProcW(hwnd, msg, wp, lp);
  }
}

void App::RegisterHotkeys() {
  UnregisterHotKey(hwnd_, config::kHotkeyShot);
  UnregisterHotKey(hwnd_, config::kHotkeyPin);

  const UINT flags = MOD_NOREPEAT;
  const bool shot_ok =
      RegisterHotKey(hwnd_, config::kHotkeyShot, settings_.mod_shot | flags,
                     settings_.vk_shot) != FALSE;
  const bool pin_ok =
      RegisterHotKey(hwnd_, config::kHotkeyPin, settings_.mod_pin | flags,
                     settings_.vk_pin) != FALSE;
  if (!shot_ok || !pin_ok) {
    const UINT bad_mod = shot_ok ? settings_.mod_pin : settings_.mod_shot;
    const UINT bad_vk = shot_ok ? settings_.vk_pin : settings_.vk_shot;
    wchar_t text[256];
    swprintf_s(text, L"%s 注册失败，可能被其它程序占用。\n可在托盘「设置」中更换组合键。",
               settings::FormatHotkey(bad_mod, bad_vk).c_str());
    util::Debug(L"RegisterHotKey failed (shot=%d pin=%d)", shot_ok, pin_ok);
    tray_.Notify(L"StickySnap 热键不可用", text);
  }
}

void App::ApplySettings(const Settings& s) {
  settings_ = s;
  PinWindow::SetBorderEnabled(s.pin_border);
  autostart::SetEnabled(s.autostart);
  RegisterHotkeys();
}

void App::OpenSettings() {
  Settings draft = settings_;
  if (!ShowSettingsDialog(hwnd_, &draft)) return;  // 用户取消
  settings::Save(draft);
  ApplySettings(draft);
}

void App::ToggleAutostart() {
  const bool on = !autostart::IsEnabled();
  if (!autostart::SetEnabled(on)) {
    tray_.Notify(L"StickySnap", L"开机自启设置失败（注册表不可写）。");
    return;
  }
  settings_.autostart = on;
  settings::Save(settings_);
}

void App::StartCapture() {
  if (OverlayWindow::Active()) return;  // 已在截图流程中
  auto* screen = new CapturedScreen();
  if (!screen->Capture()) {
    delete screen;
    util::Debug(L"capture failed");
    tray_.Notify(L"StickySnap", L"屏幕捕获失败。");
    return;
  }
  OverlayWindow::Open(
      inst_, screen, settings_,
      [this](const OverlayResult& r) {
        if (r.cropped) {
          pins_.CreatePin(r.cropped, r.origin.x, r.origin.y);
          pins_.MarkDirty();
        }
      },
      // 关闭时恢复贴图的鼠标状态（截图期间临时全部穿透）
      [this]() {
        for (auto& p : pins_.pins()) {
          if (p->hwnd()) p->RestoreExStyle();
        }
      });
  // 贴图升到遮罩之上：可见（贴图不在截图里），但不拦截选区操作
  for (auto& p : pins_.pins()) {
    if (p->hwnd()) {
      SetWindowPos(p->hwnd(), HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
      LONG_PTR ex = GetWindowLongPtrW(p->hwnd(), GWL_EXSTYLE);
      SetWindowLongPtrW(p->hwnd(), GWL_EXSTYLE, ex | WS_EX_TRANSPARENT);
    }
  }
}

void App::PinClipboard() {
  if (OverlayWindow::Active()) return;
  HBITMAP bmp = clipboard::GetImage();
  if (!bmp) {
    util::Debug(L"clipboard has no image");
    tray_.Notify(L"StickySnap", L"剪贴板里没有图片。");
    return;
  }
  POINT c{};
  GetCursorPos(&c);
  // 让贴图完整落在虚拟屏幕内
  const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  const int x = util::Clamp(c.x + 12, vx, vx + vw - 40);
  const int y = util::Clamp(c.y + 12, vy, vy + vh - 40);
  pins_.CreatePin(bmp, x, y);
  pins_.MarkDirty();
}

void App::OpenConfig() {
  // 只在文件不存在时生成默认值；否则每次点“打开配置文件”都会把用户改动冲掉
  if (GetFileAttributesW(settings::Path().c_str()) == INVALID_FILE_ATTRIBUTES)
    settings::WriteDefault();
  ShellExecuteW(hwnd_, L"open", settings::Path().c_str(), nullptr, nullptr,
                SW_SHOWNORMAL);
}

void App::OpenLog() {
  util::Debug(L"log opened from tray");  // 确保文件存在
  if (reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd_, L"open",
                                              util::LogPath().c_str(), nullptr,
                                              nullptr, SW_SHOWNORMAL)) <= 32) {
    ShellExecuteW(hwnd_, L"open", util::LogDir().c_str(), nullptr, nullptr,
                  SW_SHOWNORMAL);
  }
}

void App::ShowAbout() {
  MessageBoxW(hwnd_,
              L"StickySnap 0.0.1 (M0.6)\n\n"
              L"截图即贴图：F1 截图、Enter 贴桌面、F3 贴剪贴板。\n"
              L"纯本地、不联网、无广告。\n\n"
              L"贴图可拖动、滚轮缩放、Ctrl+滚轮调透明度、右键菜单。",
              L"关于 StickySnap", MB_OK | MB_ICONINFORMATION);
}

void App::RestoreTray() {
  tray_.Remove();
  tray_.Create(inst_, hwnd_, config::kTrayMsg, tray_cb_);
}
