# VR: the applications which the VR player lists (AppCatalog), and what puts one where it is played (AppInstaller: its archive from the network,
# checked and unpacked, with what Utils has for that).
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(VR STATIC ${Sources})

target_include_directories(VR PUBLIC "${DemoVRPlayer_SOURCE_DIR}/src/libs")
target_link_libraries     (VR PUBLIC Analysis Utils)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
