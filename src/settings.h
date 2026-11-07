#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cwctype>
#include <string>

#include "config.h"
#include "util.h"

namespace settings {

// 主色 → 深版（圆点描边/按压态）；accent_dark 不落盘，始终由 accent 派生，
// 因此默认值也取同一派生结果，保证 Save→Load 后结构体完全一致
inline COLORREF Darken(COLORREF c, double keep) {
  return RGB(static_cast<int>(GetRValue(c) * keep),
             static_cast<int>(GetGValue(c) * keep),
             static_cast<int>(GetBValue(c) * keep));
}

}  // namespace settings

// 用户配置（%APPDATA%\StickySnap\config.ini，key=value 文本，# 开头为注释）
// 首次运行自动生成带注释的默认文件；改完保存重启程序生效
struct Settings {
  COLORREF accent = config::kAccent;        // 工具栏/标注激活色（品牌蓝）
  COLORREF accent_dark = settings::Darken(config::kAccent, 0.55);
  UINT vk_shot = config::kVkShot;           // 截图热键：主键虚拟键码
  UINT mod_shot = 0;                        // 截图热键：修饰键（MOD_CONTROL 等按位或）
  UINT vk_pin = config::kVkPin;             // 贴剪贴板热键：主键虚拟键码
  UINT mod_pin = 0;                         // 贴剪贴板热键：修饰键
  int dim_percent = 55;                     // 遮罩变暗百分比 (10-90)
  bool pin_border = true;                   // 贴图是否显示描边
  bool autostart = false;                   // 开机自启（与注册表 Run 项同步）
};

namespace settings {

inline std::wstring Path() { return util::AppDir() + L"\\config.ini"; }

inline std::wstring Trim(const std::wstring& s) {
  size_t a = s.find_first_not_of(L" \t\r\n");
  if (a == std::wstring::npos) return L"";
  size_t b = s.find_last_not_of(L" \t\r\n");
  return s.substr(a, b - a + 1);
}

inline std::wstring Lower(const std::wstring& s) {
  std::wstring out = s;
  for (auto& c : out) c = static_cast<wchar_t>(towlower(c));
  return out;
}

inline bool AllDigits(const std::wstring& s) {
  if (s.empty()) return false;
  for (wchar_t c : s)
    if (c < L'0' || c > L'9') return false;
  return true;
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

// 1/0/true/false（大小写不敏感），其余回退 fallback
inline bool ParseBool(const std::wstring& s, bool fallback) {
  const std::wstring t = Lower(Trim(s));
  if (t == L"1" || t == L"true") return true;
  if (t == L"0" || t == L"false") return false;
  return fallback;
}

// 单个键名 → 虚拟键码（纯函数；无法识别返回 0）
// 支持 F1-F24、A-Z、0-9、Print/Insert/Delete/Home/End/PageUp/PageDown/
// Space/Tab/Backspace/Escape，以及 FormatHotkey 对未命名键输出的 0xNN
inline UINT ParseKeyName(const std::wstring& name) {
  const std::wstring t = Lower(Trim(name));
  if (t.empty()) return 0;
  if (t.size() == 1) {
    const wchar_t c = t[0];
    if (c >= L'a' && c <= L'z') return static_cast<UINT>(towupper(c));
    if (c >= L'0' && c <= L'9') return static_cast<UINT>(c);
    return 0;
  }
  if (t[0] == L'f' && AllDigits(t.substr(1))) {
    const int n = _wtoi(t.c_str() + 1);
    if (n >= 1 && n <= 24) return static_cast<UINT>(VK_F1 + n - 1);
    return 0;
  }
  if (t == L"print") return VK_SNAPSHOT;
  if (t == L"insert") return VK_INSERT;
  if (t == L"delete") return VK_DELETE;
  if (t == L"home") return VK_HOME;
  if (t == L"end") return VK_END;
  if (t == L"pageup") return VK_PRIOR;
  if (t == L"pagedown") return VK_NEXT;
  if (t == L"space") return VK_SPACE;
  if (t == L"tab") return VK_TAB;
  if (t == L"backspace") return VK_BACK;
  if (t == L"escape") return VK_ESCAPE;
  if (t.size() == 4 && t[0] == L'0' && t[1] == L'x') {
    unsigned v = 0;
    for (size_t i = 2; i < 4; ++i) {
      const wchar_t c = t[i];
      unsigned d;
      if (c >= L'0' && c <= L'9') d = c - L'0';
      else if (c >= L'a' && c <= L'f') d = c - L'a' + 10;
      else return 0;
      v = v * 16 + d;
    }
    return v;
  }
  return 0;
}

// 单个修饰词 → MOD_*（纯函数；非修饰词返回 0）
inline UINT ParseModifierName(const std::wstring& name) {
  const std::wstring t = Lower(Trim(name));
  if (t == L"ctrl" || t == L"control") return MOD_CONTROL;
  if (t == L"alt") return MOD_ALT;
  if (t == L"shift") return MOD_SHIFT;
  if (t == L"win" || t == L"windows") return MOD_WIN;
  return 0;
}

// 解析热键文本："Ctrl+Alt+A" / "Shift+F1" / "Win+Shift+S" / "F1"
// 大小写不敏感，修饰词顺序任意；非法文本回退到 fallback_vk + mod 0。
// 纯函数、无全局状态，便于单测。成功返回 true。
inline bool ParseHotkey(const std::wstring& text, UINT fallback_vk, UINT& mod,
                        UINT& vk) {
  mod = 0;
  vk = fallback_vk;
  const std::wstring t = Trim(text);
  if (t.empty()) return false;
  UINT parsed_mod = 0;
  size_t pos = 0;
  while (true) {
    const size_t plus = t.find(L'+', pos);
    const std::wstring tok = Trim(
        t.substr(pos, plus == std::wstring::npos ? std::wstring::npos : plus - pos));
    if (tok.empty()) return false;  // 空段：如 "Ctrl++A" / "A+"
    if (plus == std::wstring::npos) {
      const UINT key = ParseKeyName(tok);
      if (!key) return false;  // 末段必须是可识别的键
      mod = parsed_mod;
      vk = key;
      return true;
    }
    const UINT m = ParseModifierName(tok);
    if (!m) return false;  // 修饰位出现非修饰词（含多个主键）
    parsed_mod |= m;
    pos = plus + 1;
  }
}

// 虚拟键码 → 键名（Ctrl+Alt+A 里的 A / F1 / PageUp / 0xNN）
inline std::wstring KeyName(UINT vk) {
  if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
  if (vk >= L'a' && vk <= L'z') vk = static_cast<UINT>(towupper(static_cast<wchar_t>(vk)));
  if ((vk >= L'A' && vk <= L'Z') || (vk >= L'0' && vk <= L'9'))
    return std::wstring(1, static_cast<wchar_t>(vk));
  switch (vk) {
    case VK_SNAPSHOT: return L"Print";
    case VK_INSERT: return L"Insert";
    case VK_DELETE: return L"Delete";
    case VK_HOME: return L"Home";
    case VK_END: return L"End";
    case VK_PRIOR: return L"PageUp";
    case VK_NEXT: return L"PageDown";
    case VK_SPACE: return L"Space";
    case VK_TAB: return L"Tab";
    case VK_BACK: return L"Backspace";
    case VK_ESCAPE: return L"Escape";
    default: break;
  }
  // 未命名键：输出 0xNN，ParseKeyName 可还原（保证 Save→Load 一致）
  static const wchar_t kHex[] = L"0123456789ABCDEF";
  std::wstring out = L"0x";
  out += kHex[(vk >> 4) & 0xF];
  out += kHex[vk & 0xF];
  return out;
}

// 托盘菜单文案：Ctrl+Alt+A（修饰顺序 Ctrl、Alt、Shift、Win；无修饰只输出键名）
inline std::wstring FormatHotkey(UINT mod, UINT vk) {
  std::wstring out;
  if (mod & MOD_CONTROL) out += L"Ctrl+";
  if (mod & MOD_ALT) out += L"Alt+";
  if (mod & MOD_SHIFT) out += L"Shift+";
  if (mod & MOD_WIN) out += L"Win+";
  out += KeyName(vk);
  return out;
}

// 写回全部配置项（覆盖写，保留带注释的模板风格；失败静默并记调试日志）
inline void Save(const Settings& s) {
  FILE* f = _wfopen(Path().c_str(), L"w, ccs=UTF-8");
  if (!f) {
    util::Debug(L"settings: 写 %s 失败 err=%u", Path().c_str(), GetLastError());
    return;
  }
  fwprintf(f, L"# StickySnap 配置文件（编辑保存后重启程序生效）\n\n");
  fwprintf(f, L"# 界面强调色：选区边框/调节点/工具栏激活色（#RRGGBB）\n");
  fwprintf(f, L"accent=#%02X%02X%02X\n\n", GetRValue(s.accent),
           GetGValue(s.accent), GetBValue(s.accent));
  fwprintf(f, L"# 截图热键（如 Ctrl+Alt+A / Shift+F1 / F1；修饰词 Ctrl/Alt/Shift/Win，顺序任意）\n");
  fwprintf(f, L"hotkey_shot=%s\n\n", FormatHotkey(s.mod_shot, s.vk_shot).c_str());
  fwprintf(f, L"# 贴剪贴板热键（格式同上）\n");
  fwprintf(f, L"hotkey_pin=%s\n\n", FormatHotkey(s.mod_pin, s.vk_pin).c_str());
  fwprintf(f, L"# 遮罩变暗百分比（10-90）\n");
  fwprintf(f, L"dim_opacity=%d\n\n", s.dim_percent);
  fwprintf(f, L"# 贴图是否显示描边（1=显示，0=隐藏）\n");
  fwprintf(f, L"pin_border=%d\n\n", s.pin_border ? 1 : 0);
  fwprintf(f, L"# 开机自启（1=启用，0=关闭；由托盘菜单开关写入）\n");
  fwprintf(f, L"autostart=%d\n", s.autostart ? 1 : 0);
  fclose(f);
}

// 生成带注释的默认配置文件（全部六个键）
inline void WriteDefault() { Save(Settings{}); }

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
      ParseHotkey(val, config::kVkShot, s.mod_shot, s.vk_shot);
    } else if (key == L"hotkey_pin") {
      ParseHotkey(val, config::kVkPin, s.mod_pin, s.vk_pin);
    } else if (key == L"dim_opacity") {
      int v = _wtoi(val.c_str());
      if (v >= 10 && v <= 90) s.dim_percent = v;
    } else if (key == L"pin_border") {
      s.pin_border = ParseBool(val, s.pin_border);
    } else if (key == L"autostart") {
      s.autostart = ParseBool(val, s.autostart);
    }
  }
  fclose(f);
  s.accent_dark = Darken(s.accent, 0.55);
  return s;
}

}  // namespace settings
