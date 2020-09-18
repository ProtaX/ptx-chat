cmake_minimum_required(VERSION 3.12.1)

project(ptx-chat CXX)

include(cmake/StandardProjectSettings.cmake)

add_library(project_options INTERFACE)
target_compile_features(project_options INTERFACE cxx_std_17)

set(CMAKE_AUTOMOC ON)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_BUILD_TYPE Debug)

include_directories(${INCLUDE_DIR})

# Enable all warnings
add_library(project_warnings INTERFACE)
include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)

# Enable ccache
include(cmake/Cache.cmake)

# Pre-compiled headers
option(ENABLE_PCH "Enable Precompiled Headers" OFF)
if(ENABLE_PCH)
  target_precompile_headers(
    project_options
    INTERFACE
    <vector>
    <string>
    <map>
    <utility>
    <memory>)
endif()

# Tests
option(ENABLE_TESTING "Enable Test Builds" OFF)
if(ENABLE_TESTING)
  enable_testing()
  message("Building Tests. Be sure to check out test/constexpr_tests for constexpr testing")
  add_subdirectory(test)
endif()

add_subdirectory(src)
add_subdirectory(lib)