# VR: the applications which the VR player lists (AppCatalog), what puts one where it is played (AppInstaller: its archive from the network,
# checked and unpacked), and the window of a tool which is drawn with ImGui (UIHost: the control window of the replay player and the VR player).
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(VR STATIC ${Sources})

target_include_directories(VR PUBLIC "${DemoVRPlayer_SOURCE_DIR}/src/libs")
target_link_libraries     (VR PUBLIC Analysis ImGui miniz bcrypt winhttp)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
