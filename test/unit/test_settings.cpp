// settings::Load 热键解析 / settings::FormatHotkey 输出
// 注：配置路径固定为 %APPDATA%\StickySnap\config.ini，用例前后由 FileGuard 备份/还原。
#include <string>

#include "test_util.h"

#include "settings.h"

namespace {

// 写入指定的 ini 文本并加载；FileGuard 由调用方持有
Settings LoadFromIni(const std::wstring& ini_text) {
  ::test::WriteTextFile(::settings::Path(), ini_text);
  return ::settings::Load();
}

}  // namespace

TEST_CASE(settings_load_combined_hotkeys) {
  ::test::FileGuard guard(::settings::Path());

  const Settings s = LoadFromIni(L"hotkey_shot=Ctrl+Alt+A\nhotkey_pin=Shift+F1\n");
  EXPECT_EQ(s.vk_shot, static_cast<UINT>('A'));
  EXPECT_EQ(s.mod_shot, static_cast<UINT>(MOD_CONTROL | MOD_ALT));
  EXPECT_EQ(s.vk_pin, static_cast<UINT>(VK_F1));
  EXPECT_EQ(s.mod_pin, static_cast<UINT>(MOD_SHIFT));
}

TEST_CASE(settings_load_lowercase_function_key) {
  ::test::FileGuard guard(::settings::Path());

  // 解析大小写不敏感；无修饰键
  const Settings s = LoadFromIni(L"hotkey_shot=f1\n");
  EXPECT_EQ(s.vk_shot, static_cast<UINT>(VK_F1));
  EXPECT_EQ(s.mod_shot, static_cast<UINT>(0));
}

TEST_CASE(settings_load_invalid_hotkey_falls_back) {
  ::test::FileGuard guard(::settings::Path());

  const Settings defaults;
  // 非法键名 "Foo" → 回退默认，且不得留下半解析的修饰键
  const Settings s = LoadFromIni(L"hotkey_shot=Ctrl+Foo\n");
  EXPECT_EQ(s.vk_shot, defaults.vk_shot);
  EXPECT_EQ(s.mod_shot, defaults.mod_shot);
}

TEST_CASE(settings_load_other_keys) {
  ::test::FileGuard guard(::settings::Path());

  const Settings s = LoadFromIni(L"dim_opacity=70\naccent=#102030\n");
  EXPECT_EQ(s.dim_percent, 70);
  EXPECT_EQ(GetRValue(s.accent), static_cast<BYTE>(0x10));
  EXPECT_EQ(GetGValue(s.accent), static_cast<BYTE>(0x20));
  EXPECT_EQ(GetBValue(s.accent), static_cast<BYTE>(0x30));
}

TEST_CASE(settings_format_hotkey) {
  EXPECT_WSTR_EQ(::settings::FormatHotkey(MOD_CONTROL | MOD_ALT, 'A'), L"Ctrl+Alt+A");
  EXPECT_WSTR_EQ(::settings::FormatHotkey(MOD_SHIFT, VK_F1), L"Shift+F1");
  EXPECT_WSTR_EQ(::settings::FormatHotkey(0, VK_F1), L"F1");
  EXPECT_WSTR_EQ(::settings::FormatHotkey(0, VK_F12), L"F12");
  EXPECT_WSTR_EQ(::settings::FormatHotkey(0, 'Q'), L"Q");
}
