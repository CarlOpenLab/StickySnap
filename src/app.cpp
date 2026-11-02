#include "app.h"

#include "clipboard.h"
#include "config.h"
#include "overlay.h"
#include "util.h"

int App::Run(HINSTANCE inst) {
  inst_ = inst;
  settings_ = settings::Load();  // 读取用户配置（首次运行生成默认文件）

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

  if (!RegisterHotKey(hwnd_, config::kHotkeyShot, 0, settings_.vk_shot))
    util::Debug(L"RegisterHotKey shot failed %u", GetLastError());
  if (!RegisterHotKey(hwnd_, config::kHotkeyPin, 0, settings_.vk_pin))
    util::Debug(L"RegisterHotKey pin failed %u", GetLastError());

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
        case config::kCmdQuit:
          DestroyWindow(hwnd_);
          break;
        default:
          break;
      }
      return 0;

    case config::kTrayMsg:
      // 旧版 NOTIFYICONDATA：lParam 为鼠标消息
      if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP || lp == WM_CONTEXTMENU)
        tray_.ShowMenu();
      return 0;

    case WM_TIMER:
      if (wp == config::kTimerSave) pins_.FlushIfDirty();
      return 0;

    case WM_DESTROY:
      KillTimer(hwnd_, config::kTimerSave);
      UnregisterHotKey(hwnd_, config::kHotkeyShot);
      UnregisterHotKey(hwnd_, config::kHotkeyPin);
      pins_.CloseAll();
      pins_.FlushIfDirty();
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

void App::StartCapture() {
  if (OverlayWindow::Active()) return;  // 已在截图流程中
  auto* screen = new CapturedScreen();
  if (!screen->Capture()) {
    delete screen;
    util::Debug(L"capture failed");
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
  settings::WriteDefault();  // 不存在则先生成
  ShellExecuteW(hwnd_, L"open", settings::Path().c_str(), nullptr, nullptr,
                SW_SHOWNORMAL);
}

void App::RestoreTray() {
  tray_.Remove();
  tray_.Create(inst_, hwnd_, config::kTrayMsg, tray_cb_);
}
