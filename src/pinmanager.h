#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <memory>
#include <vector>

#include "pin.h"

// 持有全部贴图，负责创建/关闭/会话恢复与延迟保存
class PinManager {
 public:
  void Init(HINSTANCE inst) { inst_ = inst; }
  void SetOwner(HWND owner) { owner_ = owner; }  // 定时器挂在主窗口上

  PinWindow* CreatePin(HBITMAP bmp, int x, int y);  // bmp 所有权移交
  void ClosePin(PinWindow* pin);
  void CloseAll();
  void Restore();       // 启动时恢复上次会话
  void MarkDirty();     // 有变更 → 延迟保存
  void FlushIfDirty();  // 定时器到期调用

  const std::vector<std::unique_ptr<PinWindow>>& pins() const { return pins_; }

 private:
  void SaveNow();

  HINSTANCE inst_ = nullptr;
  HWND owner_ = nullptr;
  std::vector<std::unique_ptr<PinWindow>> pins_;
  bool dirty_ = false;
};
