#pragma once
// 极简单测工具（不依赖任何第三方框架）
// 用法：
//   TEST_CASE(some_name) { EXPECT_EQ(a, b); EXPECT_TRUE(cond); }
// 失败计入 test::g_failures；test_main.cpp 的 main 据此返回非零退出码（ctest 判定失败）。
// 另提供 FileGuard：用例前备份真实文件、用例结束（含中途失败）自动还原，避免污染开发机。
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#define TEST_WIDEN_(x) L##x
#define TEST_WIDEN(x) TEST_WIDEN_(x)

namespace test {

inline int g_failures = 0;  // 断言失败次数（非零 → 进程退出码非零）
inline int g_checks = 0;    // 断言总数

// 把测试用值统一转成 wstring 便于打印（bool 打 true/false，算术类型打数值）
inline std::wstring W(const std::wstring& s) { return s; }
inline std::wstring W(const wchar_t* s) { return s ? std::wstring(s) : L"(null)"; }
inline std::wstring W(bool b) { return b ? L"true" : L"false"; }
template <typename T>
inline std::wstring W(T v) {
  return std::to_wstring(v);
}

// UTF-8 输出，避免默认 C locale 下宽字符写 stderr 失败而看不到失败原因
inline std::string Narrow(const std::wstring& s) {
  if (s.empty()) return std::string();
  const int len = static_cast<int>(s.size());
  const int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), len, nullptr, 0, nullptr,
                                    nullptr);
  if (n <= 0) return std::string();
  std::string out(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, s.c_str(), len, &out[0], n, nullptr, nullptr);
  return out;
}

inline void Report(const char* file, int line, const std::wstring& msg) {
  ++g_failures;
  fprintf(stderr, "[FAIL] %s:%d: %s\n", file, line, Narrow(msg).c_str());
}

inline void ExpectTrue(bool cond, const wchar_t* expr, const char* file, int line) {
  ++g_checks;
  if (!cond) Report(file, line, std::wstring(L"expected true: ") + expr);
}

inline void ExpectEqFail(const std::wstring& actual, const std::wstring& expected,
                         const wchar_t* lhs, const wchar_t* rhs, const char* file,
                         int line) {
  Report(file, line, std::wstring(L"expected ") + lhs + L" == " + rhs +
                         L" (actual: " + actual + L", expected: " + expected + L")");
}

template <typename A, typename B>
inline void ExpectEq(const A& a, const B& b, const wchar_t* ea, const wchar_t* eb,
                     const char* file, int line) {
  ++g_checks;
  if (!(a == b)) ExpectEqFail(W(a), W(b), ea, eb, file, line);
}

// ---- 用例注册 ----
struct Case {
  const char* name;
  void (*fn)();
};

inline std::vector<Case>& Registry() {
  static std::vector<Case> cases;
  return cases;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) { Registry().push_back({name, fn}); }
};

// ---- 文件备份 / 还原（真实 %APPDATA%\StickySnap 下的文件） ----
struct FileSnapshot {
  bool exists = false;
  std::string bytes;
};

inline FileSnapshot SnapshotFile(const std::wstring& path) {
  FileSnapshot snap;
  FILE* f = _wfopen(path.c_str(), L"rb");
  if (!f) return snap;
  snap.exists = true;
  char buf[4096];
  size_t n = 0;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) snap.bytes.append(buf, n);
  fclose(f);
  return snap;
}

inline void RestoreFile(const std::wstring& path, const FileSnapshot& snap) {
  if (!snap.exists) {
    _wremove(path.c_str());
    return;
  }
  FILE* f = _wfopen(path.c_str(), L"wb");
  if (!f) return;
  if (!snap.bytes.empty()) fwrite(snap.bytes.data(), 1, snap.bytes.size(), f);
  fclose(f);
}

// 用例作用域内备份并还原指定文件；析构时（含断言失败后）一律还原
class FileGuard {
 public:
  explicit FileGuard(std::wstring path)
      : path_(std::move(path)), snap_(SnapshotFile(path_)) {}
  ~FileGuard() { RestoreFile(path_, snap_); }
  FileGuard(const FileGuard&) = delete;
  FileGuard& operator=(const FileGuard&) = delete;
  const std::wstring& path() const { return path_; }

 private:
  std::wstring path_;
  FileSnapshot snap_;
};

// 以 UTF-8（带 BOM）写文本；与 src 里 _wfopen(..., "ccs=UTF-8") 的读法保持一致
inline bool WriteTextFile(const std::wstring& path, const std::wstring& text) {
  FILE* f = _wfopen(path.c_str(), L"w, ccs=UTF-8");
  if (!f) return false;
  fputws(text.c_str(), f);
  fclose(f);
  return true;
}

}  // namespace test

#define EXPECT_TRUE(cond) ::test::ExpectTrue((cond), TEST_WIDEN(#cond), __FILE__, __LINE__)
#define EXPECT_EQ(a, b) \
  ::test::ExpectEq((a), (b), TEST_WIDEN(#a), TEST_WIDEN(#b), __FILE__, __LINE__)
#define EXPECT_WSTR_EQ(a, b)                                              \
  ::test::ExpectEq(std::wstring(a), std::wstring(b), TEST_WIDEN(#a),      \
                   TEST_WIDEN(#b), __FILE__, __LINE__)

#define TEST_CASE(name)                                              \
  static void test_case_##name();                                    \
  static ::test::Registrar test_reg_##name(#name, &test_case_##name); \
  static void test_case_##name()
