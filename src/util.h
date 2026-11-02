#pragma once
// 通用小工具（header-only）
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace util {

inline void Debug(const wchar_t* fmt, ...) {
  wchar_t buf[512];
  va_list args;
  va_start(args, fmt);
  vswprintf(buf, 511, fmt, args);
  va_end(args);
  OutputDebugStringW(buf);
  OutputDebugStringW(L"\n");
  // 同时追加到 %TEMP%\stickysnap_debug.log（自动化测试取证用）
  static FILE* f = [] {
    const wchar_t* tmp = _wgetenv(L"TEMP");
    wchar_t p[MAX_PATH];
    swprintf_s(p, L"%s\\stickysnap_debug.log", tmp ? tmp : L".");
    return _wfopen(p, L"w, ccs=UTF-8");
  }();
  if (f) {
    fwprintf(f, L"%s\n", buf);
    fflush(f);
  }
}

// 把窗口排除出屏幕捕获（WDA_EXCLUDEFROMCAPTURE，Win10 2004+）
// 动态加载，规避不同 SDK 头文件差异
// 环境变量 STICKYSNAP_VISIBLE_CAPTURE=1 时跳过排除（自动化测试用：截屏能看到遮罩）
inline bool ExcludeFromCapture(HWND hwnd) {
  static const bool visible_in_capture = [] {
    char buf[8] = {};
    const DWORD n = GetEnvironmentVariableA("STICKYSNAP_VISIBLE_CAPTURE", buf, 8);
    return n > 0 && n < 8 && buf[0] == '1';
  }();
  if (visible_in_capture) return false;
  using Fn = BOOL(WINAPI*)(HWND, DWORD);
  static Fn fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(
      GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowDisplayAffinity")));
  if (!fn) return false;
  return fn(hwnd, 0x00000011 /* WDA_EXCLUDEFROMCAPTURE */) != 0;
}

// %APPDATA%\StickySnap（首次调用时创建目录）
inline const std::wstring& AppDir() {
  static std::wstring dir = [] {
    std::wstring d;
    PWSTR raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &raw))) {
      d = std::wstring(raw) + L"\\StickySnap";
      CoTaskMemFree(raw);
    } else {
      d = L".\\StickySnap";
    }
    CreateDirectoryW(d.c_str(), nullptr);
    CreateDirectoryW((d + L"\\pins").c_str(), nullptr);
    return d;
  }();
  return dir;
}

inline std::wstring PinsDir() { return AppDir() + L"\\pins"; }

// 新贴图图片路径（会话恢复用）
inline std::wstring NewPinImagePath() {
  static unsigned seq = 0;
  wchar_t name[64];
  swprintf_s(name, L"pin_%u_%u.png", static_cast<unsigned>(GetTickCount()), ++seq);
  return PinsDir() + L"\\" + name;
}

// v 夹在 [lo, hi] 之间；允许 LONG/int 混用，以 v 的类型为准
template <typename T, typename U, typename V>
inline T Clamp(T v, U lo, V hi) {
  const T lo_t = static_cast<T>(lo);
  const T hi_t = static_cast<T>(hi);
  return v < lo_t ? lo_t : (v > hi_t ? hi_t : v);
}

}  // namespace util
