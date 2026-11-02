cmake_minimum_required(VERSION 3.16)
project(StickySnap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PINSHOT_SOURCES
  src/main.cpp
  src/app.cpp
  src/overlay.cpp
  src/toolbar.cpp
  src/pin.cpp
  src/pinmanager.cpp
  src/capture.cpp
  src/clipboard.cpp
  src/tray.cpp)

add_executable(StickySnap WIN32 ${PINSHOT_SOURCES})

if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/res/app.rc)
  target_sources(StickySnap PRIVATE res/app.rc)
endif()

target_include_directories(StickySnap PRIVATE src)

target_compile_definitions(StickySnap PRIVATE
  UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0A00 WINVER=0x0A00)

target_link_libraries(StickySnap PRIVATE
  user32 gdi32 shell32 dwmapi msimg32 comdlg32 gdiplus ole32)

if (MSVC)
  target_compile_options(StickySnap PRIVATE /W3 /utf-8)
else()
  target_compile_options(StickySnap PRIVATE -Wall -Wextra)
  if (MINGW)
    target_link_options(StickySnap PRIVATE -municode)
  endif()
endif()
