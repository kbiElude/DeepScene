# The replay engine: the engine of the interceptor (ReplayEngineCore) with the frames played for the eyes of a recipe. It needs the analysis of
# a capture. Each architecture has its own name, so that the 64-bit player finds the 32-bit one next to itself (see PlayerLauncher).
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(StereoReplayEngineDLL SHARED ${Sources})

target_compile_definitions(StereoReplayEngineDLL PRIVATE STEREOREPLAYENGINEDLL_BUILDING_DLL)
target_link_libraries     (StereoReplayEngineDLL APIDumpLoader Analysis ReplayEngineCore Stereo)

set_architecture_name(StereoReplayEngineDLL)

source_group ("StereoReplayEngineDLL source files" FILES ${Sources})
