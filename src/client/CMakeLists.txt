add_executable(ptx-client main.cc client.cc)
target_link_libraries(ptx-client PRIVATE project_warnings)