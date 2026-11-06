# Stereo: what a converter decides, whatever the API is (Core), what is done to the vertices of a Direct3D 7 draw for an eye (D3D7), how the
# launcher hands a playback over to the converter (Session), the images which go from the application to the program which shows them
# (Transport), and the images of a replay, written as PNG, compared and put together (Replay/FrameImages).
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(Stereo STATIC ${Sources})

target_include_directories(Stereo PUBLIC "${DemoVRPlayer_SOURCE_DIR}/src/libs")
target_link_libraries     (Stereo PUBLIC Analysis miniz)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
