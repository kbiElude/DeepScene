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

# What the programs include of each other (src/tools/stereo_replay/engine_dll/types.h) is named from here; what they include of a library is
# named from src/libs, which each library says (analysis/api_dump_analysis/model.h).
include_directories("${DemoVRPlayer_SOURCE_DIR}/src")

# Third-party code: miniz zips and unzips the images and the applications; the OpenXR loader finds the runtime of the headset (a library of its
# own, and nothing else of the SDK), and is of the 64-bit programs.
add_subdirectory(deps/miniz EXCLUDE_FROM_ALL)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(BUILD_API_LAYERS         OFF CACHE BOOL "" FORCE)
    set(BUILD_CONFORMANCE_TESTS  OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS              OFF CACHE BOOL "" FORCE)
    set(DYNAMIC_LOADER           OFF CACHE BOOL "" FORCE)

    add_subdirectory(deps/OpenXR-SDK EXCLUDE_FROM_ALL)
endif()

include(cmake/Architecture.cmake)

# The libraries, in src/libs: four, by what they are for (each folder has what it is in its CMakeLists.txt).
add_subdirectory(src/libs/analysis)
add_subdirectory(src/libs/stereo)
add_subdirectory(src/libs/vr)
add_subdirectory(src/libs/headset)

# The programs, each in the folder which it is installed as: src/runtime for what the VR player starts, src/tools for the tools, and the VR
# player in src/vr_player. The headset is of the 64-bit programs.
add_subdirectory(src/runtime/converter_dll)
add_subdirectory(src/runtime/launcher)
add_subdirectory(src/tools/analysis/api_dump_analysis_extractor)
add_subdirectory(src/tools/analysis/api_dump_analysis_query)
add_subdirectory(src/tools/analysis/api_dump_dependency_extractor)
add_subdirectory(src/tools/analysis/api_dump_dependency_query)
add_subdirectory(src/tools/analysis/api_dump_slicer)
add_subdirectory(src/tools/analysis/recipe_bake)
add_subdirectory(src/tools/stereo_replay/engine_dll)
add_subdirectory(src/tools/stereo_replay/player)
add_subdirectory(src/tools/stereo_replay/consistency_checker)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_subdirectory(src/runtime/presenter)
    add_subdirectory(src/vr_player)
endif()

include(cmake/Tests.cmake)
include(cmake/Install.cmake)
include(cmake/Folders.cmake)
