# The projects of the solution in folders, the same as the source tree is.
include(${APIInterceptor_SOURCE_DIR}/cmake/FolderHelpers.cmake)

assign_folder("libs"                Analysis Headset Stereo Utils VRPlayerBackend)
assign_folder("runtime"             StereoConverterDLL StereoLauncher StereoPresenter)
assign_folder("tools/analysis"      APIDumpAnalysisExtractor APIDumpAnalysisQuery APIDumpDependencyExtractor APIDumpDependencyQuery APIDumpSlicer RecipeBake)
assign_folder("tools/stereo_replay" StereoReplayConsistencyChecker StereoReplayEngineDLL StereoReplayPlayer)
assign_folder("vr_player"           VRPlayer)
assign_folder("tests"               DemoVRPlayerTests)
assign_folder("Third party"         miniz)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    assign_folder_to_directory("Third party/OpenXR" "${DemoVRPlayer_SOURCE_DIR}/deps/OpenXR-SDK")
endif()
