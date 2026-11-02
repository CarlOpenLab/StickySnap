#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <functional>
#include <string>
#include <vector>

#include "capture.h"
#include "settings.h"
#include "toolbar.h"

struct OverlayResult {
  HBITMAP cropped = nullptr;  // 选区合成结果（32bpp DIB），所有权移交调用方
  POINT origin{};             // 选区左上角（屏幕物理坐标）
};

// 标注图形（客户区坐标，即虚拟屏幕坐标）
enum class ShapeType {
  kRect = 0,   // 与 ToolId 顺序对齐
  kEllipse,
  kArrow,
  kPen,
  kMarker,
  kMosaic,
};

struct Shape {
  ShapeType type = ShapeType::kRect;
  COLORREF color = 0;
  int width = 2;               // 线宽 px（marker 实际 = width*4，mosaic 半径 = 8+width*3）
  std::vector<POINT> pts;      // rect/ellipse/arrow: [起点,终点]；其余: 折线
  void Offset(int dx, int dy);
};

// 全屏冻结遮罩选区窗 + Snipaste 式图标工具栏。
// 流程：F1 冻结 → 选区（拖框/单击窗口）→ 工具栏标注 → 贴图/复制/保存。
// 自管生命周期：WM_NCDESTROY 时 delete this。
class OverlayWindow {
 public:
  using FinishFn = std::function<void(const OverlayResult&)>;
  using ClosedFn = std::function<void()>;

  // screen 所有权转移给 OverlayWindow（结束时自动 delete）
  static OverlayWindow* Open(HINSTANCE inst, CapturedScreen* screen,
                             const Settings& settings,
                             FinishFn on_finish, ClosedFn on_closed);
  static bool Active() { return s_active_ != nullptr; }
  void Destroy();

 private:
  OverlayWindow() = default;
  ~OverlayWindow();
  OverlayWindow(const OverlayWindow&) = delete;
  OverlayWindow& operator=(const OverlayWindow&) = delete;

  static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
  LRESULT Handle(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  static bool RegisterClass(HINSTANCE inst);

  void BuildDimLayer();
  void OnMouseMove(const POINT& pt);
  void UpdateHover();
  void DrawMagnifier(HDC dc);
  void DrawLabel(HDC dc, const std::wstring& text, int x, int y) const;
  SIZE MeasureLabel(HDC dc, const std::wstring& text) const;
  void DrawSizeLabel(HDC dc);
  void DrawHintLabel(HDC dc);
  bool HasSel() const;
  HBITMAP Crop() const;
  HBITMAP FlattenSelection() const;  // 裁剪 + 标注合成（贴图/复制/保存共用）
  void Commit();
  void CopySel();
  void DoSave();
  void Cancel();

  // ---- 标注引擎 ----
  void OnToolChanged(int tool);
  void OnAction(int id);
  void ResetEdit();                 // 选区变化时清空标注状态
  bool EnsureCanvases() const;      // 惰性重建 ARGB 画布（脏标记触发）
  bool EnsureMosaic() const;        // 惰性构建选区像素化图
  void ShowToolbar();               // 按选区位置计算并显示工具栏
  Shape MakePreview() const;        // 按当前工具/颜色/粗细构造进行中的形状

  HWND hwnd_ = nullptr;
  HINSTANCE inst_ = nullptr;
  CapturedScreen* screen_ = nullptr;
  Settings st_{};  // 用户配置快照（主题色/遮罩暗度）
  FinishFn on_finish_;
  ClosedFn on_closed_;
  HDC dim_dc_ = nullptr;
  HBITMAP dim_bmp_ = nullptr;
  bool selecting_ = false;
  POINT sel_start_{};
  RECT sel_{};  // 客户区坐标（客户区原点 = 虚拟屏幕原点）
  bool hover_valid_ = false;
  RECT hover_rect_{};
  POINT cursor_{};
  bool closed_called_ = false;

  // ---- 标注状态 ----
  std::vector<Shape> shapes_;
  mutable bool shapes_dirty_ = false;
  mutable HBITMAP canvas_ = nullptr;      // 不透明形状层（ARGB）
  mutable HDC canvas_dc_ = nullptr;
  mutable HBITMAP canvas_m_ = nullptr;    // 荧光笔层（ARGB，半透明）
  mutable HDC canvas_m_dc_ = nullptr;
  mutable SIZE canvas_size_{};
  mutable HBITMAP mosaic_ = nullptr;      // 选区像素化图
  mutable HDC mosaic_dc_ = nullptr;
  ToolbarWindow* toolbar_ = nullptr;
  ToolbarState tstate_{};
  bool drawing_ = false;                  // 正在绘制笔画
  Shape preview_{};                       // 进行中的形状

  // ---- 后台缓冲（防闪烁：整帧合成后一次上屏）----
  HBITMAP bb_ = nullptr;
  HDC bb_dc_ = nullptr;
  bool EnsureBackBuffer(HDC ref, int w, int h);

  static OverlayWindow* s_active_;
};
