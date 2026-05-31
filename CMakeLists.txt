cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "x86_64")

project(ReplayBot VERSION 1.0.0)

# ── Geode SDK ─────────────────────────────────────────────────
# The GEODE_SDK environment variable must point to your Geode SDK checkout.
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR
        "GEODE_SDK environment variable is not set. "
        "Please set it to the path of your Geode SDK clone."
    )
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# ── Source files ──────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/ReplayBot.cpp
    src/UI.cpp
    src/Features.cpp
)

# ── Target ────────────────────────────────────────────────────
add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ── Geode setup (links SDK + cocos2d, sets mod metadata) ──────
setup_geode_mod(${PROJECT_NAME})

# ── Compiler options ──────────────────────────────────────────
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /W3
        /permissive-
        /utf-8
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -fno-rtti          # GD doesn't use RTTI
    )
endif()
