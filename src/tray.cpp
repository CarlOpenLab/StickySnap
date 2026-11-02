#include "tray.h"

#include "config.h"
#include "util.h"

bool TrayIcon::Create(HINSTANCE inst, HWND owner, UINT callback_msg, Callback cb) {
  owner_ = owner;
  cb_ = std::move(cb);
  HICON icon = LoadIconW(inst, MAKEINTRESOURCEW(1));
  if (!icon) icon = LoadIconW(nullptr, IDI_APPLICATION);

  ZeroMemory(&nid_, sizeof(nid_));
  nid_.cbSize = sizeof(nid_);
  nid_.hWnd = owner;
  nid_.uID = 1;
  nid_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  nid_.uCallbackMessage = callback_msg;
  nid_.hIcon = icon;
  lstrcpynW(nid_.szTip, config::kAppName, ARRAYSIZE(nid_.szTip));
  added_ = Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
  if (!added_) util::Debug(L"Shell_NotifyIcon add failed %u", GetLastError());
  return added_;
}

void TrayIcon::Remove() {
  if (added_) Shell_NotifyIconW(NIM_DELETE, &nid_);
  added_ = false;
}

void TrayIcon::ShowMenu() {
  HMENU menu = CreatePopupMenu();
  if (!menu) return;
  AppendMenuW(menu, MF_STRING, config::kCmdShot, L"截图(&S)\tF1");
  AppendMenuW(menu, MF_STRING, config::kCmdPaste, L"贴剪贴板(&P)\tF3");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, config::kCmdConfig, L"打开配置文件(&C)");
  AppendMenuW(menu, MF_STRING, config::kCmdQuit, L"退出(&Q)");

  POINT pt{};
  GetCursorPos(&pt);
  SetForegroundWindow(owner_);  // 托盘菜单经典前置，否则点击菜单外不消失
  WORD cmd = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
                            pt.x, pt.y, 0, owner_, nullptr);
  DestroyMenu(menu);
  PostMessageW(owner_, WM_NULL, 0, 0);
  if (cmd && cb_) cb_(cmd);
}
