#include "tray.h"

#include <string>

#include "autostart.h"
#include "config.h"
#include "settings.h"
#include "util.h"

namespace {

// 菜单项文案：名称 + 动态热键加速键（如 L"截图(&S)\tCtrl+Alt+A"）
std::wstring MenuText(const wchar_t* title, UINT mod, UINT vk) {
  std::wstring text(title);
  text += L'\t';
  text += settings::FormatHotkey(mod, vk);
  return text;
}

}  // namespace

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

void TrayIcon::ShowMenu(const Settings& s) {
  HMENU menu = CreatePopupMenu();
  if (!menu) return;

  const std::wstring shot = MenuText(L"截图(&S)", s.mod_shot, s.vk_shot);
  const std::wstring pin = MenuText(L"贴剪贴板(&P)", s.mod_pin, s.vk_pin);

  AppendMenuW(menu, MF_STRING, config::kCmdShot, shot.c_str());
  AppendMenuW(menu, MF_STRING, config::kCmdPaste, pin.c_str());
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, config::kCmdResetPass, L"取消所有穿透");
  AppendMenuW(menu, MF_STRING, config::kCmdSettings, L"设置...");
  // 勾选状态实时读取注册表，反映真实开机自启状态
  AppendMenuW(menu,
              MF_STRING | (autostart::IsEnabled() ? MF_CHECKED : MF_UNCHECKED),
              config::kCmdAutostart, L"开机自启");
  AppendMenuW(menu, MF_STRING, config::kCmdConfig, L"打开配置文件(&C)");
  AppendMenuW(menu, MF_STRING, config::kCmdLog, L"打开日志");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, config::kCmdAbout, L"关于");
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

bool TrayIcon::Notify(const wchar_t* title, const wchar_t* text) {
  if (!added_) return false;

  // NIM_MODIFY 时只带 NIF_INFO，避免动到图标/提示文本
  NOTIFYICONDATAW nid = nid_;
  nid.uFlags = NIF_INFO;
  nid.dwInfoFlags = NIIF_WARNING;
  nid.uTimeout = 5000;  // 毫秒（与 uVersion 共用联合体，NIF_INFO 下表示超时）
  lstrcpynW(nid.szInfoTitle, title ? title : L"", ARRAYSIZE(nid.szInfoTitle));
  lstrcpynW(nid.szInfo, text ? text : L"", ARRAYSIZE(nid.szInfo));

  if (!Shell_NotifyIconW(NIM_MODIFY, &nid)) {
    util::Debug(L"Shell_NotifyIcon notify failed %u", GetLastError());
    return false;
  }
  return true;
}
