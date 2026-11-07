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

#include "settings.h"

// 托盘图标：菜单命令通过回调上抛
class TrayIcon {
 public:
  using Callback = std::function<void(WORD)>;
  bool Create(HINSTANCE inst, HWND owner, UINT callback_msg, Callback cb);
  void Remove();
  // 右键菜单：截图/贴剪贴板的加速键文案跟随用户配置动态生成
  void ShowMenu(const Settings& s);
  // 气泡提示（NIF_INFO + NIIF_WARNING，5 秒）；图标未创建或调用失败返回 false
  bool Notify(const wchar_t* title, const wchar_t* text);

 private:
  NOTIFYICONDATAW nid_{};
  HWND owner_ = nullptr;
  Callback cb_;
  bool added_ = false;
};
