# Asks questions of a .deps file.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(APIDumpDependencyQuery ${Sources})

target_link_libraries(APIDumpDependencyQuery Analysis)

source_group ("APIDumpDependencyQuery source files" FILES ${Sources})
