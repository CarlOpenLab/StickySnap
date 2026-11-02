#include "app.h"  // 先经由 util.h 引入带 LEAN_AND_MEAN/NOMINMAX 的 windows.h

#include <objbase.h>
#include <gdiplus.h>

#include "config.h"
#include "util.h"

int APIENTRY wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
  HANDLE mutex = CreateMutexW(nullptr, TRUE, config::kMutex);
  if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
    CloseHandle(mutex);
    return 0;
  }

  // Per-Monitor-V2 DPI aware；动态加载，兼容旧头文件
  {
    using SetCtxFn = BOOL(WINAPI*)(void*);
    auto set_ctx = reinterpret_cast<SetCtxFn>(reinterpret_cast<void*>(GetProcAddress(
        GetModuleHandleW(L"user32.dll"), "SetProcessDpiAwarenessContext")));
    void* kPmv2 = reinterpret_cast<void*>(static_cast<intptr_t>(-4));  // PER_MONITOR_AWARE_V2
    if (!set_ctx || !set_ctx(kPmv2)) SetProcessDPIAware();
  }

  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED |
                                           COINIT_DISABLE_OLE1DDE)))
    return 0;

  Gdiplus::GdiplusStartupInput gp_input;
  ULONG_PTR gp_token = 0;
  Gdiplus::GdiplusStartup(&gp_token, &gp_input, nullptr);

  App app;
  const int ret = app.Run(inst);

  Gdiplus::GdiplusShutdown(gp_token);
  CoUninitialize();
  if (mutex) {
    ReleaseMutex(mutex);
    CloseHandle(mutex);
  }
  return ret;
}
