# VRPlayer: what the VR player (src/vr_player, the window) is made of besides the headset: the applications which it lists (AppCatalog), and what
# puts one where it is played (AppInstaller: its archive from the network, checked and unpacked, with what Utils has for that). The target is
# VRPlayerBackend, since VRPlayer is the program.
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(VRPlayerBackend STATIC ${Sources})

target_include_directories(VRPlayerBackend PUBLIC "${DemoVRPlayer_SOURCE_DIR}/src/libs")
target_link_libraries     (VRPlayerBackend PUBLIC Analysis Utils)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
