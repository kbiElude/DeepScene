# Writes into a recipe what the analysis knows of the application, and checks a recipe against it (--check).
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(RecipeBake ${Sources})

target_link_libraries(RecipeBake Analysis)

set_architecture_name(RecipeBake)

source_group ("RecipeBake source files" FILES ${Sources})
