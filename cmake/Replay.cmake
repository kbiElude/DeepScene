# The replay of a workload for both eyes, and the programs which play it in a window and check it. The engine needs the D3D8 description of the
# interceptor (D3D8Utils) and the analysis of a capture; the window is the one of UIHost (see VR.cmake), drawn with the Dear ImGui of the
# interceptor. The 32-bit pair has its own names, so that the 64-bit player can find it next to itself (see PlayerLauncher).

file(GLOB StereoReplayEngineDLLIncludes CONFIGURE_DEPENDS        "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/Replay/EngineDLL/*.h")
file(GLOB StereoReplayEngineDLLPrivateIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL/*.h")
file(GLOB StereoReplayEngineDLLSources CONFIGURE_DEPENDS         "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL/*.cpp")

add_library(StereoReplayEngineDLL SHARED ${StereoReplayEngineDLLIncludes} ${StereoReplayEngineDLLPrivateIncludes} ${StereoReplayEngineDLLSources})

target_compile_definitions(StereoReplayEngineDLL PRIVATE STEREOREPLAYENGINEDLL_BUILDING_DLL)
target_include_directories(StereoReplayEngineDLL PRIVATE "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL")
target_link_libraries     (StereoReplayEngineDLL APIDumpAnalysisFrontends APIDumpLoader RecipeApplier D3D7Utils D3D8Utils gdi32 opengl32 user32 winmm)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(StereoReplayEngineDLL PROPERTIES OUTPUT_NAME "StereoReplayEngineDLL_x86")
endif()

source_group ("Stereo Replay Engine include files" FILES ${StereoReplayEngineDLLIncludes} ${StereoReplayEngineDLLPrivateIncludes})
source_group ("Stereo Replay Engine source files"  FILES ${StereoReplayEngineDLLSources})

file(GLOB StereoReplayPlayerSources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/Player/*.cpp"
                                                      "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/Player/*.h")

add_executable(StereoReplayPlayer ${StereoReplayPlayerSources})

target_link_libraries(StereoReplayPlayer APIDumpAnalysis APIDumpLoader RecipeLoader StereoReplayEngineDLL ImGui UIHost comdlg32)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(StereoReplayPlayer PROPERTIES OUTPUT_NAME "StereoReplayPlayer_x86")
    # A segment of the workload is 100 to 200 MB, and the replay reads several ahead: a 32-bit process has two gigabytes unless it says it can have four.
    target_link_options(StereoReplayPlayer PRIVATE /LARGEADDRESSAWARE)
endif()

source_group ("StereoReplayPlayer source files" FILES ${StereoReplayPlayerSources})

file(GLOB StereoReplayConsistencyCheckerSources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/ConsistencyChecker/*.cpp"
                                                                  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/ConsistencyChecker/*.h")

add_executable(StereoReplayConsistencyChecker ${StereoReplayConsistencyCheckerSources})

target_link_libraries(StereoReplayConsistencyChecker APIDumpAnalysis RecipeLoader StereoReplayEngineDLL)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(StereoReplayConsistencyChecker PROPERTIES OUTPUT_NAME "StereoReplayConsistencyChecker_x86")
    target_link_options(StereoReplayConsistencyChecker PRIVATE /LARGEADDRESSAWARE)
endif()

source_group ("StereoReplayConsistencyChecker source files" FILES ${StereoReplayConsistencyCheckerSources})
