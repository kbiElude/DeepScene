# Asks questions of a .textures file.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(APIDumpTextureQuery ${Sources})

target_link_libraries(APIDumpTextureQuery Analysis)

set_architecture_name(APIDumpTextureQuery)

source_group ("APIDumpTextureQuery source files" FILES ${Sources})
