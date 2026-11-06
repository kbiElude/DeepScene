# The replay engine: the engine of the interceptor (ReplayEngineCore) with the frames played for the eyes of a recipe. It needs the analysis of
# a capture. Each architecture has its own name, so that the 64-bit player finds the 32-bit one next to itself (see PlayerLauncher). What is of
# Direct3D 7 only is in D3D7/.
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(StereoReplayEngineDLL SHARED ${Sources})

# A file of D3D7/ names what is of the engine as it is ("frame_buffer.h"), and the other files name D3D7/ ("D3D7/d3d7_draw_rewriter.h").
target_include_directories(StereoReplayEngineDLL PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(StereoReplayEngineDLL PRIVATE STEREOREPLAYENGINEDLL_BUILDING_DLL)
target_link_libraries     (StereoReplayEngineDLL APIDumpLoader Analysis ReplayEngineCore Stereo)

set_architecture_name(StereoReplayEngineDLL)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
