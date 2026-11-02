@echo off
rem 本地快速编译脚本（MSVC，通过 vcvars64 激活工具链）
setlocal
cd /d %~dp0
call "C:\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
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
  src\capture.cpp src\clipboard.cpp src\tray.cpp ^
  /Fe:build\StickySnap.exe /Fo:build\ ^
  /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib dwmapi.lib ^
  msimg32.lib comdlg32.lib gdiplus.lib ole32.lib %RES%

if errorlevel 1 (
  echo [error] build failed
  exit /b 1
)
echo [ok] build\StickySnap.exe
