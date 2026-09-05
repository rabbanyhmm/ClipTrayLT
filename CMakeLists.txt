cmake_minimum_required(VERSION 3.20)
project(simpleclipboard_native LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

add_compile_options(-O2 -Wall -Wextra)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network DBus)
find_package(PkgConfig REQUIRED)

pkg_check_modules(LIBEVDEV REQUIRED IMPORTED_TARGET libevdev)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)
pkg_check_modules(ATSPI REQUIRED IMPORTED_TARGET atspi-2 gobject-2.0 glib-2.0)

include_directories(
    src
    src/core
    src/ui
)

set(SOURCES
    src/core/storage.cpp
    src/core/paste_injector.cpp
    src/core/evdev_listener.cpp
    src/core/clipboard_daemon.cpp
    src/core/caret_detector.cpp
    src/core/global_hotkey.cpp
    src/ui/item_card.cpp
    src/ui/smooth_scroll_area.cpp
    src/ui/flyout_window.cpp
    src/ui/simple_test_window.cpp
    src/main.cpp
)

add_executable(simpleclipboard-native ${SOURCES})

target_link_libraries(simpleclipboard-native
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::DBus
    PkgConfig::LIBEVDEV
    PkgConfig::SQLITE3
    PkgConfig::ATSPI
    X11
    xcb
)

# Unit Tests
add_executable(test_storage tests/test_storage.cpp src/core/storage.cpp)
target_link_libraries(test_storage PRIVATE PkgConfig::SQLITE3)

# Instant IPC Trigger CLI
add_executable(simpleclipboard-trigger src/trigger.c)
