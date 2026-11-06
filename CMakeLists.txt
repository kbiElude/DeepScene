cmake_minimum_required (VERSION 3.10)
project (DemoVRPlayer)

option(DEMOVRPLAYER_BUILD_TESTS "Build automated tests" OFF)

if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif (MSVC)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${DemoVRPlayer_BINARY_DIR}/bin")
set(LIBRARY_OUTPUT_PATH            "${DemoVRPlayer_BINARY_DIR}/bin")

# The interception and capture code which this project builds on, as a submodule. Nothing of it is built unless a program of
# this project needs it, and its tests are built when the tests of this project are (they share its test support and googletest).
set(APIINTERCEPTOR_BUILD_INTERCEPTOR ON                          CACHE BOOL "" FORCE)
set(APIINTERCEPTOR_BUILD_LIBS        ON                          CACHE BOOL "" FORCE)
set(APIINTERCEPTOR_BUILD_TESTS       ${DEMOVRPLAYER_BUILD_TESTS} CACHE BOOL "" FORCE)
set(APIINTERCEPTOR_BUILD_TOOLS       ON                          CACHE BOOL "" FORCE)

add_subdirectory(deps/APIInterceptor EXCLUDE_FROM_ALL)

# A release has the tools which capture a program; nothing else of this project needs them.
add_custom_target(CaptureTools ALL DEPENDS APIDumper APIDumperDLL APIDumpPrinter)

include_directories("${DemoVRPlayer_SOURCE_DIR}/include"
                    "${DemoVRPlayer_SOURCE_DIR}/src")

# The libraries.
include(cmake/Analysis.cmake)
include(cmake/AppInstaller.cmake)
include(cmake/Replay.cmake)
include(cmake/Stereo.cmake)
include(cmake/VR.cmake)
include(cmake/Headset.cmake)

# The programs, each in the folder which it is installed as: src/runtime for what the VR player starts, src/tools for the tools, and the VR
# player in src/VRPlayer. The headset is of the 64-bit programs.
add_subdirectory(src/runtime/StereoConverterDLL)
add_subdirectory(src/runtime/StereoLauncher)
add_subdirectory(src/tools/Analysis/APIDumpAnalysisExtractor)
add_subdirectory(src/tools/Analysis/APIDumpAnalysisQuery)
add_subdirectory(src/tools/Analysis/APIDumpDependencyExtractor)
add_subdirectory(src/tools/Analysis/APIDumpDependencyQuery)
add_subdirectory(src/tools/Analysis/APIDumpSlicer)
add_subdirectory(src/tools/Analysis/RecipeBake)
add_subdirectory(src/tools/Replay/StereoReplayEngineDLL)
add_subdirectory(src/tools/Replay/StereoReplayPlayer)
add_subdirectory(src/tools/Replay/StereoReplayConsistencyChecker)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_subdirectory(src/runtime/StereoPresenter)
    add_subdirectory(src/VRPlayer)
endif()

include(cmake/Tests.cmake)
include(cmake/Install.cmake)
include(cmake/Folders.cmake)
