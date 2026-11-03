cmake_minimum_required(VERSION 3.22)

# Keep in sync with include/lkmk3/version.hpp (a test checks it) and CHANGELOG.md.
project(lkmk3_lynx VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LKMK3_BUILD_TESTS "Build the hardware-free unit tests" ON)
option(LKMK3_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(LKMK3_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer (GCC / Clang)" OFF)

if(LKMK3_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
# lkmk3_core: JUCE-free and LYNX-free. Protocol layer + DAW mapping layer.
# (This list is also used by cmake/LaunchkeyMk3Lynx.cmake when building inside LYNX SHARE.)
include(cmake/Sources.cmake)

add_library(lkmk3_core STATIC ${LKMK3_CORE_SOURCES})
target_include_directories(lkmk3_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(lkmk3_core PRIVATE /W4)
else()
    target_compile_options(lkmk3_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
if(LKMK3_WARNINGS_AS_ERRORS)
    target_compile_options(lkmk3_core PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/WX,-Werror>)
endif()

if(LKMK3_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
