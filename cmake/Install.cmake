# What a release is made of, and where each part goes. build.bat empties build_final\ and then installs both build trees into it:
#
#   cmake --install build_x86 --config <Config> --prefix build_final
#   cmake --install build_x64 --config <Config> --prefix build_final
#
# Only what is named here is installed. A program that has both architectures has them side by side, the 64-bit one with the
# suffix _x64 and the 32-bit one with _x86, since each looks for its partners next to itself (the programs of the replay have _x86 in
# the name of the 32-bit target, see Replay.cmake).
#
#   build_final\                     VRPlayer.exe
#   build_final\runtime\             what VRPlayer starts: StereoPresenter, StereoLauncher and StereoConverterDLL
#   build_final\apps\                the catalog and the recipes (an application is downloaded into it the first time it is launched)
#   build_final\tools\APIDumper\     what captures a program: the dumper, its DLL and the printer (from the interceptor)
#   build_final\tools\Analysis\      the extractors and the query tools of the analysis (of the passes and of the dependencies), the slicer
#                                    and the baker of the recipes
#   build_final\tools\Replay\        the replay player, the replay engine and the consistency checker
include(${APIInterceptor_SOURCE_DIR}/cmake/InstallHelpers.cmake)

install_with_suffix(APIDumper                 "tools/APIDumper")
install_with_suffix(APIDumperDLL              "tools/APIDumper")
install_with_suffix(StereoConverterDLL        "runtime")
install_with_suffix(StereoLauncher            "runtime")

install_as_built(StereoReplayConsistencyChecker "tools/Replay")
install_as_built(StereoReplayEngineDLL          "tools/Replay")
install_as_built(StereoReplayPlayer             "tools/Replay")

# What is not made in 32 bits: the 64-bit tree installs it alone.
if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    install_as_built(APIDumpAnalysisExtractor   "tools/Analysis")
    install_as_built(APIDumpAnalysisQuery       "tools/Analysis")
    install_as_built(APIDumpDependencyExtractor "tools/Analysis")
    install_as_built(APIDumpDependencyQuery     "tools/Analysis")
    install_as_built(APIDumpPrinter             "tools/APIDumper")
    install_as_built(APIDumpSlicer            "tools/Analysis")
    install_as_built(RecipeBake               "tools/Analysis")
    install_as_built(StereoPresenter          "runtime")
    install_as_built(VRPlayer                 ".")

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
