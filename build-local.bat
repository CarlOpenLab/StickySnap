@echo off
rem Local quick build (MSVC, vcvars64)
rem Locate VS via vswhere, fall back to C:\BuildTools
setlocal
cd /d %~dp0

set "VCVARS="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    if not defined VCVARS set "VCVARS=%%i\VC\Auxiliary\Build\vcvars64.bat"
  )
)
if defined VCVARS if not exist "%VCVARS%" set "VCVARS="
if not defined VCVARS (
  if exist "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" set "VCVARS=C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS (
  echo [error] vcvars64.bat not found: install VS 2022 with the "Desktop development with C++" workload
  exit /b 1
)

echo [info] vcvars64.bat: %VCVARS%
call "%VCVARS%" >nul
if errorlevel 1 (
  echo [error] vcvars64.bat failed
  exit /b 1
)
if not exist build mkdir build

rc /nologo /fo build\app.res res\app.rc
if errorlevel 1 (
  echo [warn] rc.exe failed, building without icon resource
  set "RES="
) else (
  set "RES=build\app.res"
)

cl /nologo /std:c++17 /utf-8 /W3 /EHsc /O2 ^
  /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^
  /D_CRT_SECURE_NO_WARNINGS /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 ^
  /Isrc ^
  src\main.cpp src\app.cpp src\overlay.cpp src\toolbar.cpp src\pin.cpp src\pinmanager.cpp ^
  src\capture.cpp src\clipboard.cpp src\tray.cpp src\settings_dialog.cpp ^
  /Fe:build\StickySnap.exe /Fo:build\ ^
  /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib dwmapi.lib ^
  msimg32.lib comdlg32.lib gdiplus.lib ole32.lib advapi32.lib comctl32.lib %RES%

if errorlevel 1 (
  echo [error] build failed
  exit /b 1
)
echo [ok] build\StickySnap.exe

rem ---------- unit tests (no framework, console) ----------
echo [info] building unit tests
pushd build
cl /nologo /std:c++17 /utf-8 /W3 /EHsc /O2 ^
  /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^
  /D_CRT_SECURE_NO_WARNINGS /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 ^
  /I..\src ^
  ..\test\unit\*.cpp ^
  /Fe:stickysnap_tests.exe ^
  /link user32.lib gdi32.lib shell32.lib ole32.lib gdiplus.lib advapi32.lib comctl32.lib
set "TEST_RC=%errorlevel%"
popd

if not "%TEST_RC%"=="0" (
  echo [error] unit tests build failed
  exit /b 1
)

build\stickysnap_tests.exe
if errorlevel 1 (
  echo [error] unit tests failed
  exit /b 1
)
echo [ok] unit tests passed
