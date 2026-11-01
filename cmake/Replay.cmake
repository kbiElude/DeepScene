# The replay of a workload for both eyes, and the programs which play it in a window and check it. The engine needs the D3D8 description of the
# interceptor (D3D8Utils) and the analysis of a capture; the window is the one of UIHost (see VR.cmake), drawn with the Dear ImGui of the
# interceptor. The 32-bit pair has its own names, so that the 64-bit player can find it next to itself (see PlayerLauncher).

file(GLOB StereoReplayEngineDLLIncludes CONFIGURE_DEPENDS        "${DemoVRPlayer_SOURCE_DIR}/include/Replay/StereoReplayEngineDLL/*.h")
file(GLOB StereoReplayEngineDLLPrivateIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/*.h")
file(GLOB StereoReplayEngineDLLSources CONFIGURE_DEPENDS         "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/*.cpp")

add_library(StereoReplayEngineDLL SHARED ${StereoReplayEngineDLLIncludes} ${StereoReplayEngineDLLPrivateIncludes} ${StereoReplayEngineDLLSources})

target_compile_definitions(StereoReplayEngineDLL PRIVATE STEREOREPLAYENGINEDLL_BUILDING_DLL)
target_include_directories(StereoReplayEngineDLL PRIVATE "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL")
target_link_libraries     (StereoReplayEngineDLL APIDumpAnalysisFrontends APIDumpLoader RecipeApplier D3D8Utils gdi32 opengl32 user32 winmm)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(StereoReplayEngineDLL PROPERTIES OUTPUT_NAME "StereoReplayEngineDLL_x86")
endif()

source_group ("Stereo Replay Engine include files" FILES ${StereoReplayEngineDLLIncludes} ${StereoReplayEngineDLLPrivateIncludes})
source_group ("Stereo Replay Engine source files"  FILES ${StereoReplayEngineDLLSources})
