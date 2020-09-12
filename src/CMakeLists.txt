option(BUILD_SERVER "Enable compilation of ptx-server" ON)
option(BUILD_CLIENT "Enable compilation of ptx-client" OFF)

if (BUILD_SERVER)
  message("Building ptx-server")
  add_subdirectory(server)
endif()

if (BUILD_CLIENT)
  message("Building ptx-client")
  add_subdirectory(client)
endif()