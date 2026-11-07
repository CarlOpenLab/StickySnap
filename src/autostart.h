#pragma once
// 开机自启（header-only）：HKCU\Software\Microsoft\Windows\CurrentVersion\Run
// 只依赖 windows.h + util.h，可被单测/CI 单独编译
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <string>

#include "util.h"

namespace autostart {

inline constexpr wchar_t kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr wchar_t kValueName[] = L"StickySnap";

// 注册表 Run 项的值：exe 绝对路径；路径含空格时 Run 项必须加引号
inline std::wstring Command() {
  wchar_t exe[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameW(nullptr, exe, ARRAYSIZE(exe));
  if (n == 0 || n >= ARRAYSIZE(exe)) {
    util::Debug(L"autostart: GetModuleFileNameW failed %u", GetLastError());
    return L"";
  }
  return L"\"" + std::wstring(exe, n) + L"\"";
}

inline bool IsEnabled() {
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key) !=
      ERROR_SUCCESS)
    return false;

  DWORD type = 0;
  DWORD size = 0;
  LONG rc = RegQueryValueExW(key, kValueName, nullptr, &type, nullptr, &size);
  bool enabled = false;
  if (rc == ERROR_SUCCESS && type == REG_SZ && size >= sizeof(wchar_t)) {
    std::wstring value(size / sizeof(wchar_t), L'\0');
    rc = RegQueryValueExW(key, kValueName, nullptr, &type,
                          reinterpret_cast<BYTE*>(value.data()), &size);
    if (rc == ERROR_SUCCESS && !value.empty() && value[0] != L'\0')
      enabled = true;
  }
  RegCloseKey(key);
  return enabled;
}

inline bool SetEnabled(bool on) {
  if (!on) {
    // 关闭：只改已存在的键，避免凭空创建空的 Run 子键
    HKEY key = nullptr;
    const LONG open =
        RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key);
    if (open == ERROR_FILE_NOT_FOUND) return true;  // 键都不存在 → 本来就没启用
    if (open != ERROR_SUCCESS) {
      util::Debug(L"autostart: RegOpenKeyEx failed %ld", open);
      return false;
    }
    LONG rc = RegDeleteValueW(key, kValueName);
    RegCloseKey(key);
    if (rc == ERROR_FILE_NOT_FOUND) rc = ERROR_SUCCESS;  // 本来就没启用
    if (rc != ERROR_SUCCESS) {
      util::Debug(L"autostart: RegDeleteValue failed %ld", rc);
      return false;
    }
    return true;
  }

  const std::wstring cmd = Command();
  if (cmd.empty()) return false;

  HKEY key = nullptr;
  const LONG open = RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                                    KEY_SET_VALUE, nullptr, &key, nullptr);
  if (open != ERROR_SUCCESS) {
    util::Debug(L"autostart: RegCreateKeyEx failed %ld", open);
    return false;
  }
  const LONG rc = RegSetValueExW(
      key, kValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()),
      static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
  if (rc != ERROR_SUCCESS) {
    util::Debug(L"autostart: RegSetValueEx failed %ld", rc);
    return false;
  }
  return true;
}

}  // namespace autostart
