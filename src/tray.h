#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <functional>

// 托盘图标：菜单命令通过回调上抛
class TrayIcon {
 public:
  using Callback = std::function<void(WORD)>;
  bool Create(HINSTANCE inst, HWND owner, UINT callback_msg, Callback cb);
  void Remove();
  void ShowMenu();

 private:

  NOTIFYICONDATAW nid_{};
  HWND owner_ = nullptr;
  Callback cb_;
  bool added_ = false;
};
