set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 COMPONENTS Widgets Core REQUIRED)
qt5_wrap_cpp(server-gui ${INCLUDE_DIR}/PtxServerGui.h)
qt5_wrap_cpp(frontend-gui ${INCLUDE_DIR}/PtxGuiFrontend.h)

add_executable(ptx-server main.cc ${server-gui} ${frontend-gui})

add_library(server server.cc)

target_link_libraries(server ptx-gui-backend pthread)
target_link_libraries(ptx-server PRIVATE project_warnings project_options Qt5::Widgets Qt5::Core server)