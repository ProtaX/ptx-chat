# Qt5
set(CMAKE_INCLUDE_CURRENT_DIR ON)
find_package(Qt5 COMPONENTS Widgets Core REQUIRED)
qt5_wrap_cpp(ptx-server-src ${INCLUDE_DIR}/Gui.h)

add_executable(ptx-server main.cc server.cc ${ptx-server-src})
target_link_libraries(ptx-server PRIVATE project_warnings pthread Qt5::Widgets Qt5::Core)