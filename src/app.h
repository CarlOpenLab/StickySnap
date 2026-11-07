#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>

#include "pinmanager.h"
#include "settings.h"
#include "tray.h"

// 主应用：隐藏主窗口 + 全局热键 + 托盘 + 各模块编排
class App {
 public:
  int Run(HINSTANCE inst);

 private:
  static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
  LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void StartCapture();       // 截图热键：冻结屏幕 → 选区 → 贴图
  void PinClipboard();       // 贴图热键：剪贴板图片 → 贴图
  void OpenConfig();         // 打开 config.ini（用户可编辑，重启生效）
  void OpenSettings();       // 设置对话框（热键/强调色/边框/自启）
  void OpenLog();            // 打开运行日志
  void ShowAbout();          // 关于
  void ToggleAutostart();    // 开机自启开关（注册表 + config.ini）
  void RegisterHotkeys();    // （重新）注册全局热键，失败时托盘气泡提示
  void ApplySettings(const Settings& s);  // 保存后即时生效（热键/边框/自启）
  void RestoreTray();        // explorer 重启后重建托盘图标

  HWND hwnd_ = nullptr;
  HINSTANCE inst_ = nullptr;
  TrayIcon tray_;
  std::function<void(WORD)> tray_cb_;
  PinManager pins_;
  Settings settings_{};
  UINT taskbar_created_ = 0;
};
