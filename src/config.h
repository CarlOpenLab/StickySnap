#pragma once
// StickySnap —— Snipaste 式截图+贴图工具（代号，后续可改）
// 全局常量集中在这里；M1 引入配置文件后再拆
#include <windows.h>

namespace config {

inline constexpr wchar_t kClassName[]    = L"StickySnap.Main";
inline constexpr wchar_t kOverlayClass[] = L"StickySnap.Overlay";
inline constexpr wchar_t kPinClass[]     = L"StickySnap.Pin";
inline constexpr wchar_t kToolbarClass[] = L"StickySnap.Toolbar";
inline constexpr wchar_t kMutex[]        = L"Local\\StickySnap.SingleInstance";
inline constexpr wchar_t kAppName[]      = L"StickySnap";

inline constexpr UINT kTrayMsg = WM_APP + 1;  // 托盘回调消息

// 全局热键（M0 固定默认值，M1 做自定义）
inline constexpr UINT kHotkeyShot = 1;
inline constexpr UINT kHotkeyPin  = 2;
inline constexpr UINT kVkShot = VK_F1;
inline constexpr UINT kVkPin  = VK_F3;

inline constexpr UINT kTimerSave = 3;  // 会话延迟保存定时器

// 贴图缩放/透明度
inline constexpr double kZoomStep  = 1.1;
inline constexpr double kZoomMin   = 0.1;
inline constexpr double kZoomMax   = 8.0;
inline constexpr int    kAlphaMin  = 32;
inline constexpr int    kAlphaStep = 16;

// 托盘菜单命令 ID（与 tray.cpp 保持一致）
inline constexpr WORD kCmdShot   = 100;
inline constexpr WORD kCmdPaste  = 101;
inline constexpr WORD kCmdConfig = 102;
inline constexpr WORD kCmdQuit   = 103;

// 贴图右键菜单命令 ID
inline constexpr WORD kPinMenuPass = 200;
inline constexpr WORD kPinMenuCopy = 201;
inline constexpr WORD kPinMenuSave = 202;
inline constexpr WORD kPinMenuClose = 203;

// ---------- UI 主题色（选区边框 / 工具栏共用，改这里全局生效） ----------
inline constexpr COLORREF kAccent     = RGB(0xFF, 0x4B, 0x3E);  // 主强调色（落地页 --accent）
inline constexpr COLORREF kAccentDark = RGB(0x8C, 0x29, 0x22);  // 强调色深版（圆点描边）

}  // namespace config
