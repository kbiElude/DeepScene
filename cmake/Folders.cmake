# The projects of the solution in folders, by what they are for.
include(${APIInterceptor_SOURCE_DIR}/cmake/FolderHelpers.cmake)

assign_folder("Analysis/Libraries" APIDumpAnalysis APIDumpAnalysisFrontends RecipeApplier RecipeLoader)
assign_folder("Analysis/Tools"     APIDumpAnalysisExtractor APIDumpAnalysisQuery RecipeBake)
assign_folder("Stereo/Libraries"   StereoCore StereoD3D12 StereoD3D7 StereoSession StereoTransport StereoXR)
assign_folder("Stereo/Programs"    StereoConverterDLL StereoLauncher StereoPresenter)
assign_folder("Stereo/Replay"      StereoReplayConsistencyChecker StereoReplayEngineDLL StereoReplayPlayer)
assign_folder("Stereo/Replay/Libraries" StereoReplayFrameImages)
assign_folder("VR Player"          AppCatalog AppInstaller UIHost VRDevices VRPlayer)
assign_folder("Tests"              DemoVRPlayerTests)
assign_folder("Third party"        miniz)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    assign_folder_to_directory("Third party/OpenXR" "${DemoVRPlayer_SOURCE_DIR}/deps/OpenXR-SDK")
endif()
