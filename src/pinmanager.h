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
  // mark_dirty=false 用于退出：此时贴图即将全部销毁，不能把空列表写回会话
  void CloseAll(bool mark_dirty = true);
  void Restore();          // 启动时恢复上次会话
  void SaveNow();          // 立即落盘（退出前调用，必须在 CloseAll 之前）
  void ClampToVirtualScreen();  // 显示器拔插/改分辨率后把贴图拉回可见区域
  void SetAllPass(bool on);     // 批量开关鼠标穿透（托盘“取消所有穿透”）
  void MarkDirty();        // 有变更 → 延迟保存
  void FlushIfDirty();     // 定时器到期调用

  const std::vector<std::unique_ptr<PinWindow>>& pins() const { return pins_; }

 private:

  HINSTANCE inst_ = nullptr;
  HWND owner_ = nullptr;
  std::vector<std::unique_ptr<PinWindow>> pins_;
  bool dirty_ = false;
};
