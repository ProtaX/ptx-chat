cmake_minimum_required(VERSION 3.2)

project(ptx-chat CXX)

include(cmake/StandardProjectSettings.cmake)

set(CMAKE_CXX_COMPILE_FEATURES -std=c++17)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(project_warnings INTERFACE)

include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)

include(cmake/Doxygen.cmake)
enable_doxygen()

include(cmake/Cache.cmake)

option(ENABLE_TESTING "Enable Test Builds" OFF)

option(ENABLE_PCH "Enable Precompiled Headers" OFF)
if(ENABLE_PCH)
  # This sets a global PCH parameter, each project will build its own PCH, which is a good idea if any #define's change
  #
  # consider breaking this out per project as necessary
  target_precompile_headers(
    project_options
    INTERFACE
    <vector>
    <string>
    <map>
    <utility>
    <memory>)
endif()

if(ENABLE_TESTING)
  enable_testing()
  message("Building Tests. Be sure to check out test/constexpr_tests for constexpr testing")
  add_subdirectory(test)
endif()

add_subdirectory(src)