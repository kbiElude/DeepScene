# Cuts the first frames of a workload out into a smaller one.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(APIDumpSlicer ${Sources})

target_link_libraries(APIDumpSlicer APIDumpAnalysis APIDumpSlicing)

source_group ("APIDumpSlicer source files" FILES ${Sources})
