#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>

#include "session.h"

class PinManager;

// 贴图窗口：置顶无边框悬浮图。
// 生命周期由 PinManager 持有（vector<unique_ptr>），窗口销毁由 manager 驱动。
class PinWindow {
 public:
  static void RegisterClass(HINSTANCE inst);
  // bmp 所有权移交 PinWindow；init 为会话恢复参数（可为 nullptr）
  static PinWindow* Create(HINSTANCE inst, PinManager* mgr, HBITMAP bmp,
                           const std::wstring& img_path, int x, int y,
                           const PinSession* init = nullptr);
  ~PinWindow();

  void Close();  // 销毁窗口（对象由 manager 释放）
  PinSession Session() const;
  // 截图期间贴图被临时设为鼠标穿透，结束后按自身 pass_ 状态恢复
  void RestoreExStyle();

  HWND hwnd() const { return hwnd_; }

 private:
  PinWindow(PinManager* mgr, HBITMAP bmp, std::wstring img_path);
  PinWindow(const PinWindow&) = delete;
  PinWindow& operator=(const PinWindow&) = delete;

  static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
  LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

  void ApplyZoom();
  void SetPass(bool on);
  void ShowMenu(int x, int y);
  void SaveAs();

  HWND hwnd_ = nullptr;
  PinManager* mgr_ = nullptr;
  HBITMAP bmp_ = nullptr;
  int bw_ = 0, bh_ = 0;
  double zoom_ = 1.0;
  int alpha_ = 255;
  bool pass_ = false;
  bool dragging_ = false;
  POINT drag_off_{};
  std::wstring img_path_;
};
