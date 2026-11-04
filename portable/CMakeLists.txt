cmake_minimum_required(VERSION 3.21)

if(APPLE)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Minimum macOS version")
endif()

project(StickySnapPortable LANGUAGES CXX)

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
else()
  message(FATAL_ERROR
    "The portable StickySnap build currently implements the macOS platform "
    "layer. Add src/platform/windows or src/platform/linux before building "
    "on this host.")
endif()

qt_add_executable(StickySnap ${STICKYSNAP_APP_SOURCES})
target_link_libraries(StickySnap PRIVATE sticky_core Qt6::Widgets)
target_include_directories(StickySnap PRIVATE src)

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
    "-framework AppKit" "-framework CoreGraphics" "-framework Foundation"
    "-framework ScreenCaptureKit"
  )
  add_test(NAME ui_render_test COMMAND ui_render_test)
  set_tests_properties(ui_render_test PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
  )
endif()
