// 单测入口：顺序运行全部 TEST_CASE，任一断言失败则返回非零
// 无网络、无 GUI、无交互：ctest 直接跑本程序即可
#include "test_util.h"

int main() {
  const std::vector<::test::Case>& cases = ::test::Registry();
  if (cases.empty()) {
    fwprintf(stderr, L"[FAIL] no test cases registered\n");
    return 1;
  }

  int failed_cases = 0;
  for (const ::test::Case& c : cases) {
    const int before = ::test::g_failures;
    c.fn();
    const bool ok = ::test::g_failures == before;
    if (!ok) ++failed_cases;
    printf("[%s] %s\n", ok ? "ok" : "fail", c.name);
  }

  printf("%d/%d cases passed, %d checks, %d failures\n",
         static_cast<int>(cases.size()) - failed_cases,
         static_cast<int>(cases.size()), ::test::g_checks, ::test::g_failures);
  fflush(stdout);
  return ::test::g_failures == 0 ? 0 : 1;
}
