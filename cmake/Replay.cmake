# The images of replayed frames: written as PNG, compared, and put together. The replay engine, the player and the checker are in
# src/tools/Replay.

file(GLOB StereoReplayFrameImagesIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/Replay/FrameImages/*.h")
file(GLOB StereoReplayFrameImagesSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/FrameImages/*.cpp")

add_library(StereoReplayFrameImages STATIC ${StereoReplayFrameImagesIncludes} ${StereoReplayFrameImagesSources})

target_link_libraries(StereoReplayFrameImages PUBLIC miniz)

source_group ("Stereo Replay Frame Images include files" FILES ${StereoReplayFrameImagesIncludes})
source_group ("Stereo Replay Frame Images source files"  FILES ${StereoReplayFrameImagesSources})
