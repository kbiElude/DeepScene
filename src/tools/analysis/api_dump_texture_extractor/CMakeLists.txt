# Reads a workload and writes what happens to its textures (a .textures file, and the file of blobs beside it): the uploads, the copies that rendering makes, the
# levels that the driver makes, and the points where what rendering made is read.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(APIDumpTextureExtractor ${Sources})

target_link_libraries(APIDumpTextureExtractor Analysis)

set_architecture_name(APIDumpTextureExtractor)

source_group ("APIDumpTextureExtractor source files" FILES ${Sources})
