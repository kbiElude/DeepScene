# What a release is made of, and where each part goes. build.bat empties build_final\ and then installs both build trees into it:
#
#   cmake --install build_x86 --config <Config> --prefix build_final
#   cmake --install build_x64 --config <Config> --prefix build_final
#
# Only what is named here is installed. Every program but VRPlayer has the architecture it is built for in its name (_x64 or _x86, see
# Architecture.cmake), so a program that has both has them side by side, which is where each looks for its partners. The ones of the interceptor
# (the dumper, its DLL, the printer) are given the suffix as they are installed.
#
#   build_final\                        VRPlayer.exe
#   build_final\runtime\                what VRPlayer starts: StereoPresenter, StereoLauncher and StereoConverterDLL
#   build_final\apps\                   the catalog and the recipes (an application is downloaded into it the first time it is launched)
#   build_final\tools\api_dumper\       what captures a program: the dumper, its DLL and the printer (from the interceptor)
#   build_final\tools\analysis\         the extractors and the query tools of the analysis (of the passes and of the dependencies), the slicer
#                                       and the baker of the recipes
#   build_final\tools\stereo_replay\    the replay player, the replay engine and the consistency checker
include(${APIInterceptor_SOURCE_DIR}/cmake/InstallHelpers.cmake)

install_with_suffix(APIDumper                 "tools/api_dumper")
install_with_suffix(APIDumperDLL              "tools/api_dumper")

install_as_built(StereoConverterDLL             "runtime")
install_as_built(StereoLauncher                 "runtime")
install_as_built(StereoReplayConsistencyChecker "tools/stereo_replay")
install_as_built(StereoReplayEngineDLL          "tools/stereo_replay")
install_as_built(StereoReplayPlayer             "tools/stereo_replay")

# What is not made in 32 bits: the 64-bit tree installs it alone.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    install_with_suffix(APIDumpPrinter          "tools/api_dumper")

    install_as_built(APIDumpAnalysisExtractor   "tools/analysis")
    install_as_built(APIDumpAnalysisQuery       "tools/analysis")
    install_as_built(APIDumpDependencyExtractor "tools/analysis")
    install_as_built(APIDumpDependencyQuery     "tools/analysis")
    install_as_built(APIDumpSlicer              "tools/analysis")
    install_as_built(RecipeBake                 "tools/analysis")
    install_as_built(StereoPresenter            "runtime")
    install_as_built(VRPlayer                   ".")

    # The catalog and the recipes. What the catalog starts is not part of the release: it is downloaded when it is first launched, into the
    # directory of the catalog, which is why VRPlayer has to be where it can write.
    install(FILES       "${DemoVRPlayer_SOURCE_DIR}/apps/apps.json"
            DESTINATION "apps")
    install(DIRECTORY   "${DemoVRPlayer_SOURCE_DIR}/apps/recipes"
            DESTINATION "apps")

    # What each folder of the release is.
    install(FILES       "${DemoVRPlayer_SOURCE_DIR}/cmake/release_README.txt"
            DESTINATION "."
            RENAME      "README.txt")
endif()
