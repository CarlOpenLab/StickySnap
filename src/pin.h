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
  // 全局边框开关（默认 true），同时作用于之后创建和已存在的贴图窗口
  static void SetBorderEnabled(bool on);
  // 供 PinManager 统一复位/恢复穿透，不暴露内部 SetPass
  void SetPassThrough(bool on) { SetPass(on); }

  HWND hwnd() const { return hwnd_; }

 private:
  PinWindow(PinManager* mgr, HBITMAP bmp, std::wstring img_path);
  PinWindow(const PinWindow&) = delete;
  PinWindow& operator=(const PinWindow&) = delete;

  static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
  LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  // 边框开关变化时遍历已有贴图窗口：复位悬停状态并重绘
  static BOOL CALLBACK ResetHoverProc(HWND hwnd, LPARAM lp);

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
  bool hover_ = false;     // 鼠标是否在窗口内（边框内圈换强调色）
  bool tracking_ = false;  // 是否已注册 TrackMouseEvent 的离开通知
  POINT drag_off_{};
  std::wstring img_path_;
};
