# Asks questions of a .passes file: its passes, draws, overlays and depth.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(APIDumpAnalysisQuery ${Sources})

target_link_libraries(APIDumpAnalysisQuery Analysis)

set_architecture_name(APIDumpAnalysisQuery)

source_group ("APIDumpAnalysisQuery source files" FILES ${Sources})
