cmake_minimum_required(VERSION 3.21)
project(wordbooster
    VERSION 0.1.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_TESTS "Build unit tests" ON)

# Find optional fcitx5 (only needed for the actual addon, not for module tests)
find_package(Fcitx5Core QUIET)
if(Fcitx5Core_FOUND)
    include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")
endif()

# SQLite (required by HistoryStore)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SQLITE3 REQUIRED IMPORTED_TARGET sqlite3)

# ICU (required by text_utils for UTF-8 lowercasing)
find_package(ICU REQUIRED COMPONENTS uc)

add_subdirectory(src)

if(ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
