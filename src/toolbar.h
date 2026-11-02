#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>

// ---------- 工具栏与标注共享常量 ----------

// 标注工具（id 0..5，与 ShapeType 顺序一致）
enum ToolId {
  TOOL_RECT = 0,
  TOOL_ELLIPSE,
  TOOL_ARROW,
  TOOL_PEN,
  TOOL_MARKER,
  TOOL_MOSAIC,
  TOOL_COUNT
};

// 动作按钮 id
enum ActionId {
  ACT_UNDO = 100,
  ACT_PIN,
  ACT_COPY,
  ACT_SAVE,
  ACT_CANCEL,
};

constexpr int kColorCount = 9;
// 标注调色板（落地页 .cdot 同款：Radix 系 9 色）
constexpr COLORREF kPalette[kColorCount] = {
    RGB(0xE5, 0x48, 0x4D), RGB(0xFF, 0x8D, 0x3A), RGB(0xF7, 0xC9, 0x48),
    RGB(0x46, 0xA7, 0x58), RGB(0x2F, 0x6F, 0xED), RGB(0x7A, 0x5A, 0xF8),
    RGB(0xE3, 0x6B, 0xAA), RGB(0x0E, 0xA5, 0xA5), RGB(0xF2, 0xF2, 0xF2)};

constexpr int kWidthCount = 3;
constexpr int kWidths[kWidthCount] = {2, 4, 7};  // 标注线宽 px
constexpr int kDotDiam[kWidthCount] = {5, 8, 12};  // 粗细档圆点直径（.wdot）

// ---------- 工具栏窗口 ----------
// 选区完成后浮在选区旁的图标工具栏（无激活、置顶、自绘）
// 状态（当前工具/颜色/粗细/主题色）存在 ToolbarState 里，由 OverlayWindow 持有
struct ToolbarState {
  int active_tool = -1;  // -1 = 未选工具
  int color = 4;         // 默认选中蓝（落地页 .cdot.on）
  int width = 1;         // 默认 8px 档（落地页 .wdot.on）
  COLORREF accent = RGB(255, 75, 62);        // 激活底色（落地页 --accent）
  COLORREF accent_dark = RGB(140, 41, 34);
};

class ToolbarWindow {
 public:
  using Fn = std::function<void(int)>;
  // 回调：on_tool(工具id/-1) on_action(ActionId) on_color(索引) on_width(索引)
  static ToolbarWindow* Create(HINSTANCE inst, ToolbarState* state, Fn on_tool,
                               Fn on_action, Fn on_color, Fn on_width);
  void ShowAt(int x, int y);  // 屏幕物理坐标（工具栏左上角）
  void Hide();
  void Destroy();  // 销毁窗口（WM_NCDESTROY 时 delete this）
  void Refresh();  // 状态变化后重绘
  bool visible() const { return visible_; }
  SIZE size() const { return size_; }

 private:
  ToolbarWindow() = default;
  ~ToolbarWindow();
  ToolbarWindow(const ToolbarWindow&) = delete;
  ToolbarWindow& operator=(const ToolbarWindow&) = delete;

  static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
  LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static bool RegisterClass(HINSTANCE inst);

  void Layout();
  int HitTest(POINT p) const;
  void Draw(HDC dc);
  void DrawGlyph(HDC dc, int id, RECT r) const;

  HWND hwnd_ = nullptr;
  HINSTANCE inst_ = nullptr;
  ToolbarState* state_ = nullptr;
  Fn on_tool_, on_action_, on_color_, on_width_;
  int hover_ = -1;
  bool tracking_ = false;
  bool visible_ = false;
  SIZE size_{};
  std::vector<int> ids_;    // 与 rects_ 一一对应
  std::vector<RECT> rects_; // 客户区坐标
};
