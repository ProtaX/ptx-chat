cmake_minimum_required(VERSION 3.2)

project(ptx-chat CXX)

include(cmake/StandardProjectSettings.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_COMPILE_FEATURES -std=c++17)
set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

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