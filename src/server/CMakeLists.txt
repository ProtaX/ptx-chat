add_executable(ptx-server main.cc server.cc)
target_link_libraries(ptx-server PRIVATE project_warnings pthread)