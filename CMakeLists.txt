cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "x86_64")

project(ObjectRenderExport VERSION 1.0.0)

# Fetch Geode SDK - adjust path if you have it locally
if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "GEODE_SDK environment variable not set. Install Geode CLI and run 'geode sdk install'.")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

# Collect source files, excluding android_media.cpp on non-Android platforms
file(GLOB_RECURSE SOURCES src/*.cpp)
list(FILTER SOURCES EXCLUDE REGEX ".*android_media\\.cpp$")

if(ANDROID OR GEODE_IS_ANDROID OR GEODE_IS_ANDROID64 OR GEODE_IS_ANDROID32)
    list(APPEND SOURCES src/android_media.cpp)
endif()

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)

setup_geode_mod(${PROJECT_NAME})
