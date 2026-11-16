cmake_minimum_required(VERSION 3.21)

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version")
endif()

project(StickySnapPortable LANGUAGES CXX)

# 版本号：默认 0.0.2，release CI 用 -DSTICKYSNAP_VERSION=<tag> 覆盖，注入 Info.plist
set(STICKYSNAP_VERSION "0.0.2" CACHE STRING "StickySnap app version")
set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${STICKYSNAP_VERSION}")
set(MACOSX_BUNDLE_BUNDLE_VERSION "${STICKYSNAP_VERSION}")

if(APPLE)
  enable_language(OBJCXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Test)

qt_standard_project_setup()

add_library(sticky_core STATIC
  src/core/annotation_model.cpp
  src/core/session_store.cpp
  src/core/settings.cpp
  src/core/selection_model.cpp
)
target_include_directories(sticky_core PUBLIC src)
target_link_libraries(sticky_core PUBLIC Qt6::Core Qt6::Gui)
if(MSVC)
  target_compile_options(sticky_core PRIVATE /utf-8)
endif()

set(STICKYSNAP_APP_SOURCES
  src/main.cpp
  src/ui/app_controller.cpp
  src/ui/overlay_session.cpp
  src/ui/overlay_window.cpp
  src/ui/pin_manager.cpp
  src/ui/pin_window.cpp
  src/ui/settings_dialog.cpp
  src/ui/toolbar_widget.cpp
  src/ui/tray_icon.cpp
)

if(APPLE)
  list(APPEND STICKYSNAP_APP_SOURCES
    src/platform/macos/hotkeys_macos.mm
    src/platform/macos/platform_macos.mm
    src/platform/macos/screen_capture_macos.mm
  )
  set_source_files_properties(
    src/platform/macos/hotkeys_macos.mm
    src/platform/macos/platform_macos.mm
    src/platform/macos/screen_capture_macos.mm
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc"
  )
elseif(WIN32)
  list(APPEND STICKYSNAP_APP_SOURCES
    src/platform/windows/hotkeys_windows.cpp
    src/platform/windows/platform_windows.cpp
    src/platform/windows/screen_capture_windows.cpp
    src/platform/windows/ocr_windows.cpp
  )

  # OCR 探测：SDK 自带 C++/WinRT 投影头时启用 Windows.Media.Ocr 实现，
  # 否则 ocr_windows.cpp 编译为 stub（菜单入口报「此构建未包含 OCR」）。
  set(_cppwinrt_candidates "")
  foreach(_root
      "C:/Program Files (x86)/Windows Kits/10/Include"
      "C:/Program Files/Windows Kits/10/Include")
    file(GLOB _dirs "${_root}/*/cppwinrt")
    foreach(_dir "${_dirs}")
      if(EXISTS "${_dir}/winrt/Windows.Media.Ocr.h")
        list(APPEND _cppwinrt_candidates "${_dir}")
      endif()
    endforeach()
  endforeach()
  set(_stickysnap_ocr_ok FALSE)
  set(_stickysnap_cppwinrt_dir "")
  if(_cppwinrt_candidates)
    list(SORT _cppwinrt_candidates)
    list(REVERSE _cppwinrt_candidates)
    list(GET _cppwinrt_candidates 0 _stickysnap_cppwinrt_dir)
    set(_stickysnap_ocr_ok TRUE)
  endif()
  if(_stickysnap_ocr_ok)
    message(STATUS "StickySnap: OCR enabled via ${_stickysnap_cppwinrt_dir}")
  else()
    message(STATUS "StickySnap: OCR disabled (C++/WinRT projections not found)")
  endif()
  set(STICKYSNAP_HAVE_OCR ${_stickysnap_ocr_ok} CACHE INTERNAL "")
  set(STICKYSNAP_CPPWINRT_DIR "${_stickysnap_cppwinrt_dir}" CACHE INTERNAL "")
elseif(UNIX AND NOT APPLE)
  list(APPEND STICKYSNAP_APP_SOURCES
    src/platform/linux/platform_linux.cpp
    src/platform/linux/hotkeys_linux.cpp
    src/platform/linux/screen_capture_linux.cpp
  )
else()
  message(FATAL_ERROR
    "The portable StickySnap build supports macOS, Windows and X11 Linux. "
    "Platform layer not implemented for this host (Wayland unsupported).")
endif()

qt_add_executable(StickySnap ${STICKYSNAP_APP_SOURCES})
target_link_libraries(StickySnap PRIVATE sticky_core Qt6::Widgets)
target_include_directories(StickySnap PRIVATE src)

if(WIN32)
  set_target_properties(StickySnap PROPERTIES WIN32_EXECUTABLE TRUE)
  target_compile_definitions(StickySnap PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
  # 嵌入品牌图标与版本信息（explorer / 快捷方式显示的图标来源）
  enable_language(RC)
  string(REGEX REPLACE "\\." "," STICKYSNAP_RC_VERSION "${STICKYSNAP_VERSION}")
  set(STICKYSNAP_ICO_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/app-icon.ico")
  configure_file(resources/app-icon.rc.in "${CMAKE_CURRENT_BINARY_DIR}/app-icon.rc" @ONLY)
  target_sources(StickySnap PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app-icon.rc")
  target_link_libraries(StickySnap PRIVATE
    user32 gdi32 shell32 dwmapi advapi32 uiautomationcore)
  if(STICKYSNAP_HAVE_OCR)
    target_compile_definitions(StickySnap PRIVATE STICKYSNAP_HAVE_OCR)
    target_include_directories(StickySnap PRIVATE "${STICKYSNAP_CPPWINRT_DIR}")
    target_link_libraries(StickySnap PRIVATE windowsapp.lib)
  endif()
elseif(UNIX AND NOT APPLE)
  find_package(X11 REQUIRED)
  target_link_libraries(StickySnap PRIVATE X11::X11 ${X11_Xext_LIB})
endif()

qt_add_resources(StickySnap app_icons
  PREFIX "/icons"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/app-icon.png"
)

if(APPLE)
  set_target_properties(StickySnap PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "StickySnap"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.carlopenlab.stickysnap"
    MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in"
  )
  target_sources(StickySnap PRIVATE macos/AppIcon.icns)
  set_source_files_properties(macos/AppIcon.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION "Resources")
  target_link_libraries(StickySnap PRIVATE
    "-framework AppKit"
    "-framework Carbon"
    "-framework Cocoa"
    "-framework CoreGraphics"
    "-framework Foundation"
    "-framework ScreenCaptureKit"
    "-framework ServiceManagement"
    "-framework Vision"
    "-framework ApplicationServices"
  )
endif()

if(MSVC)
  target_compile_options(StickySnap PRIVATE /W3 /utf-8)
else()
  target_compile_options(StickySnap PRIVATE -Wall -Wextra)
endif()

enable_testing()

function(sticky_add_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE sticky_core Qt6::Test)
  if(MSVC)
    target_compile_options(${name} PRIVATE /utf-8)
  endif()
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )
endfunction()

sticky_add_test(session_store_test tests/session_store_test.cpp)
sticky_add_test(settings_test tests/settings_test.cpp)
sticky_add_test(selection_model_test tests/selection_model_test.cpp)
sticky_add_test(annotation_model_test tests/annotation_model_test.cpp)

add_executable(settings_dialog_test
  tests/settings_dialog_test.cpp
  src/ui/settings_dialog.cpp
)
target_include_directories(settings_dialog_test PRIVATE src)
target_link_libraries(settings_dialog_test PRIVATE
  sticky_core Qt6::Widgets Qt6::Test)
if(MSVC)
  target_compile_options(settings_dialog_test PRIVATE /utf-8)
endif()
add_test(NAME settings_dialog_test COMMAND settings_dialog_test)
set_tests_properties(settings_dialog_test PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)

if(APPLE)
  add_executable(ui_render_test
    tests/ui_render_test.cpp
    src/ui/overlay_window.cpp
    src/ui/pin_window.cpp
    src/ui/toolbar_widget.cpp
    src/ui/tray_icon.cpp
    src/platform/macos/platform_macos.mm
    src/platform/macos/screen_capture_macos.mm
  )
  set_source_files_properties(
    src/platform/macos/platform_macos.mm
    src/platform/macos/screen_capture_macos.mm
    PROPERTIES COMPILE_OPTIONS "-fobjc-arc"
  )
  target_include_directories(ui_render_test PRIVATE src)
  target_link_libraries(ui_render_test PRIVATE
    sticky_core Qt6::Widgets Qt6::Test
    "-framework AppKit" "-framework ApplicationServices"
    "-framework CoreGraphics" "-framework Foundation"
    "-framework ScreenCaptureKit" "-framework ServiceManagement"
    "-framework Vision"
  )
elseif(WIN32)
  add_executable(ui_render_test
    tests/ui_render_test.cpp
    src/ui/overlay_window.cpp
    src/ui/pin_window.cpp
    src/ui/toolbar_widget.cpp
    src/ui/tray_icon.cpp
    src/platform/windows/platform_windows.cpp
    src/platform/windows/screen_capture_windows.cpp
    src/platform/windows/ocr_windows.cpp
  )
  qt_add_resources(ui_render_test app_icons
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/app-icon.png"
  )
  target_include_directories(ui_render_test PRIVATE src)
  target_compile_definitions(ui_render_test PRIVATE
    NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
  if(MSVC)
    target_compile_options(ui_render_test PRIVATE /utf-8)
  endif()
  target_link_libraries(ui_render_test PRIVATE
    sticky_core Qt6::Widgets Qt6::Test
    user32 gdi32 shell32 dwmapi advapi32 uiautomationcore
  )
elseif(UNIX AND NOT APPLE)
  add_executable(ui_render_test
    tests/ui_render_test.cpp
    src/ui/overlay_window.cpp
    src/ui/pin_window.cpp
    src/ui/toolbar_widget.cpp
    src/ui/tray_icon.cpp
    src/platform/linux/platform_linux.cpp
    src/platform/linux/screen_capture_linux.cpp
  )
  qt_add_resources(ui_render_test app_icons
    PREFIX "/icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/app-icon.png"
  )
  target_include_directories(ui_render_test PRIVATE src)
  target_link_libraries(ui_render_test PRIVATE
    sticky_core Qt6::Widgets Qt6::Test X11::X11 ${X11_Xext_LIB}
  )
else()
  return()
endif()

add_test(NAME ui_render_test COMMAND ui_render_test)
set_tests_properties(ui_render_test PROPERTIES
  ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
)
