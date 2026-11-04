#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cwchar>
#include <string>

#include "config.h"
#include "util.h"

// 用户配置（%APPDATA%\StickySnap\config.ini，key=value 文本，# 开头为注释）
// 首次运行自动生成带注释的默认文件；改完保存重启程序生效
struct Settings {
  COLORREF accent = config::kAccent;        // 工具栏/标注激活色（品牌蓝）
  COLORREF accent_dark = config::kAccentDark;
  UINT vk_shot = config::kVkShot;           // 截图热键
  UINT vk_pin = config::kVkPin;             // 贴剪贴板热键
  int dim_percent = 55;                     // 遮罩变暗百分比 (10-90)
};

namespace settings {

inline std::wstring Path() { return util::AppDir() + L"\\config.ini"; }

inline std::wstring Trim(const std::wstring& s) {
  size_t a = s.find_first_not_of(L" \t\r\n");
  if (a == std::wstring::npos) return L"";
  size_t b = s.find_last_not_of(L" \t\r\n");
  return s.substr(a, b - a + 1);
}

inline COLORREF Darken(COLORREF c, double keep) {
  return RGB(static_cast<int>(GetRValue(c) * keep),
             static_cast<int>(GetGValue(c) * keep),
             static_cast<int>(GetBValue(c) * keep));
}

inline COLORREF ParseColor(const std::wstring& s, COLORREF fallback) {
  if (s.size() < 7 || s[0] != L'#') return fallback;
  unsigned v = 0;
  for (size_t i = 1; i < 7; ++i) {
    wchar_t c = s[i];
    unsigned d;
    if (c >= L'0' && c <= L'9') d = c - L'0';
    else if (c >= L'a' && c <= L'f') d = c - L'a' + 10;
    else if (c >= L'A' && c <= L'F') d = c - L'A' + 10;
    else return fallback;
    v = v * 16 + d;
  }
  return RGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

inline UINT ParseVk(const std::wstring& s, UINT fallback) {
  if (s.empty()) return fallback;
  wchar_t c = towupper(s[0]);
  if (c == L'F' && s.size() >= 2) {
    int n = _wtoi(s.c_str() + 1);
    if (n >= 1 && n <= 12) return static_cast<UINT>(VK_F1 + n - 1);
  }
  if (c >= L'0' && c <= L'9' && s.size() == 1) return static_cast<UINT>(c);
  if (c >= L'A' && c <= L'Z' && s.size() == 1) return static_cast<UINT>(c);
  return fallback;
}

inline void WriteDefault() {
  FILE* f = _wfopen(Path().c_str(), L"w, ccs=UTF-8");
  if (!f) return;
  fwprintf(f, L"# StickySnap 配置文件（编辑保存后重启程序生效）\n\n");
  fwprintf(f, L"# 界面强调色：选区边框/调节点/工具栏激活色（#RRGGBB）\n");
  fwprintf(f, L"accent=#%02X%02X%02X\n\n", GetRValue(config::kAccent),
           GetGValue(config::kAccent), GetBValue(config::kAccent));
  fwprintf(f, L"# 截图热键（F1-F12 / 单个字母 / 单个数字）\n");
  fwprintf(f, L"hotkey_shot=F1\nhotkey_pin=F3\n\n");
  fwprintf(f, L"# 遮罩变暗百分比（10-90）\n");
  fwprintf(f, L"dim_opacity=55\n");
  fclose(f);
}

inline Settings Load() {
  Settings s;
  if (GetFileAttributesW(Path().c_str()) == INVALID_FILE_ATTRIBUTES) WriteDefault();

  FILE* f = _wfopen(Path().c_str(), L"r, ccs=UTF-8");
  if (!f) return s;
  wchar_t line[512];
  while (fgetws(line, 512, f)) {
    std::wstring row = Trim(line);
    if (row.empty() || row[0] == L'#' || row[0] == L'[') continue;
    const size_t eq = row.find(L'=');
    if (eq == std::wstring::npos) continue;
    const std::wstring key = Trim(row.substr(0, eq));
    const std::wstring val = Trim(row.substr(eq + 1));
    if (key == L"accent") {
      s.accent = ParseColor(val, s.accent);
      s.accent_dark = Darken(s.accent, 0.55);
    } else if (key == L"hotkey_shot") {
      s.vk_shot = ParseVk(val, s.vk_shot);
    } else if (key == L"hotkey_pin") {
      s.vk_pin = ParseVk(val, s.vk_pin);
    } else if (key == L"dim_opacity") {
      int v = _wtoi(val.c_str());
      if (v >= 10 && v <= 90) s.dim_percent = v;
    }
  }
  fclose(f);
  s.accent_dark = Darken(s.accent, 0.55);
  return s;
}

}  // namespace settings
