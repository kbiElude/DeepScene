# The libraries of the project. There are four, by what they are for; each is the folder of its name under src/libs, and the programs (see
# src/runtime, src/tools and src/VRPlayer) link the ones they need.
#
#   Analysis   what a capture is made of, the dependencies between its frames, the cutting of it, and the recipes
#   Stereo     what a converter decides, the images that go from the application to the program that shows them, and the images of a replay
#   Headset    the images of the eyes on the GPU, the OpenXR session which shows them, and the devices which are listed
#   VR         the applications of the VR player, their installing, and the window of a tool which is drawn with ImGui

# Analysis. Everything here reads a workload through the loader of the interceptor (APIDumpLoader).
file(GLOB_RECURSE AnalysisIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Analysis/*.h")
file(GLOB_RECURSE AnalysisSources  CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/*.cpp")

# The compressor of the slicing is the one of the dumper (a source of its DLL, which a program cannot link).
add_library(Analysis STATIC ${AnalysisIncludes} ${AnalysisSources} "${APIInterceptor_SOURCE_DIR}/src/Capture/APIDumperDLL/zstd_file_writer.cpp")

# hasher.cpp uses the xxHash that ships with Zstandard, and zstd_file_writer.cpp includes its header by its name.
target_include_directories(Analysis PRIVATE "${APIInterceptor_SOURCE_DIR}/deps/zstd/lib/common"
                                            "${APIInterceptor_SOURCE_DIR}/include/Capture/APIDumperDLL")
target_link_libraries(Analysis PUBLIC APIDumpLoader D3D7Utils D3D8Utils)

source_group ("Analysis include files" FILES ${AnalysisIncludes})
source_group ("Analysis source files"  FILES ${AnalysisSources})

# Stereo. The images are written as PNG and zipped with miniz.
add_subdirectory(deps/miniz EXCLUDE_FROM_ALL)

file(GLOB_RECURSE StereoIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/Core/*.h"
                                                   "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/D3D7/*.h"
                                                   "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/Replay/FrameImages/*.h"
                                                   "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/Session/*.h"
                                                   "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/Transport/*.h")
file(GLOB_RECURSE StereoSources  CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Core/*.cpp"
                                                   "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/D3D7/*.cpp"
                                                   "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/FrameImages/*.cpp"
                                                   "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Session/*.cpp"
                                                   "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Transport/*.cpp")

add_library(Stereo STATIC ${StereoIncludes} ${StereoSources})

target_link_libraries(Stereo PUBLIC Analysis miniz)

source_group ("Stereo include files" FILES ${StereoIncludes})
source_group ("Stereo source files"  FILES ${StereoSources})

# VR. miniz unpacks the zip files of the applications; the window is drawn with the Dear ImGui of the interceptor.
file(GLOB_RECURSE VRIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/VR/AppCatalog/*.h"
                                               "${DemoVRPlayer_SOURCE_DIR}/include/VR/AppInstaller/*.h"
                                               "${DemoVRPlayer_SOURCE_DIR}/include/VR/UIHost/*.h")
file(GLOB_RECURSE VRSources  CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/VR/AppCatalog/*.cpp"
                                               "${DemoVRPlayer_SOURCE_DIR}/src/VR/AppInstaller/*.cpp"
                                               "${DemoVRPlayer_SOURCE_DIR}/src/VR/UIHost/*.cpp")

add_library(VR STATIC ${VRIncludes} ${VRSources})

target_link_libraries(VR PUBLIC Analysis ImGui miniz bcrypt winhttp)

source_group ("VR include files" FILES ${VRIncludes})
source_group ("VR source files"  FILES ${VRSources})

# Headset. The OpenXR session is of the 64-bit programs: a headset is shown the images by a program of the architecture of the machine, whatever
# the application is.
file(GLOB_RECURSE HeadsetIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/D3D12/*.h"
                                                    "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/XR/*.h"
                                                    "${DemoVRPlayer_SOURCE_DIR}/include/VR/VRDevices/*.h")
file(GLOB_RECURSE HeadsetSources  CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/D3D12/*.cpp"
                                                    "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/XR/*.cpp"
                                                    "${DemoVRPlayer_SOURCE_DIR}/src/VR/VRDevices/*.cpp")

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    list(FILTER HeadsetIncludes EXCLUDE REGEX "/Stereo/XR/")
    list(FILTER HeadsetSources  EXCLUDE REGEX "/Stereo/XR/")
endif()

add_library(Headset STATIC ${HeadsetIncludes} ${HeadsetSources})

target_link_libraries(Headset PUBLIC Stereo d3d12 dxgi)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    # The OpenXR loader, which finds the runtime of the headset: a library of its own, and nothing else of the SDK.
    set(BUILD_API_LAYERS         OFF CACHE BOOL "" FORCE)
    set(BUILD_CONFORMANCE_TESTS  OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS              OFF CACHE BOOL "" FORCE)
    set(DYNAMIC_LOADER           OFF CACHE BOOL "" FORCE)

    add_subdirectory(deps/OpenXR-SDK EXCLUDE_FROM_ALL)

    target_compile_definitions(Headset PUBLIC XR_USE_GRAPHICS_API_D3D12 XR_USE_PLATFORM_WIN32)
    target_include_directories(Headset PUBLIC "${DemoVRPlayer_SOURCE_DIR}/deps/OpenXR-SDK/include")
    target_link_libraries     (Headset PUBLIC openxr_loader)
endif()

source_group ("Headset include files" FILES ${HeadsetIncludes})
source_group ("Headset source files"  FILES ${HeadsetSources})
