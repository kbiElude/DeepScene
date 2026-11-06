# The replay engine: the engine of the interceptor (ReplayEngineCore) with the frames played for the eyes of a recipe. It needs the analysis of
# a capture. The 32-bit one has its own name, so that the 64-bit player can find it next to itself (see PlayerLauncher).
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(StereoReplayEngineDLL SHARED ${Sources})

target_compile_definitions(StereoReplayEngineDLL PRIVATE STEREOREPLAYENGINEDLL_BUILDING_DLL)
target_link_libraries     (StereoReplayEngineDLL APIDumpAnalysisFrontends APIDumpDependencies APIDumpLoader RecipeApplier ReplayEngineCore StereoCore StereoD3D7)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(StereoReplayEngineDLL PROPERTIES OUTPUT_NAME "StereoReplayEngineDLL_x86")
endif()

source_group ("StereoReplayEngineDLL source files" FILES ${Sources})
