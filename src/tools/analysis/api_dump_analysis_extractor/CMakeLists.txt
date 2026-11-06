# Reads a workload and writes what each frame of it is made of (a .passes file).
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(APIDumpAnalysisExtractor ${Sources})

target_link_libraries(APIDumpAnalysisExtractor Analysis)

set_architecture_name(APIDumpAnalysisExtractor)

source_group ("APIDumpAnalysisExtractor source files" FILES ${Sources})
