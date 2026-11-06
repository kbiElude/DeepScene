# Reads a workload and writes which frames and textures each frame needs (a .deps file), which is what makes a seek fast.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(APIDumpDependencyExtractor ${Sources})

target_link_libraries(APIDumpDependencyExtractor Analysis)

source_group ("APIDumpDependencyExtractor source files" FILES ${Sources})
