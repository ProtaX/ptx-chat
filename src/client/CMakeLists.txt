set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5 COMPONENTS Widgets Core REQUIRED)
qt5_wrap_cpp(gui-src-c ${INCLUDE_DIR}/PtxGuiFrontend.h)

add_executable(ptx-client main.cc client.cc ${gui-src-c})

add_library(ptx-client-gui STATIC PtxClientGui.cc)

target_link_libraries(ptx-client-gui ptx-gui-backend Qt5::Widgets Qt5::Core)
target_link_libraries(ptx-client ptx-client-gui)
target_link_libraries(ptx-client pthread)