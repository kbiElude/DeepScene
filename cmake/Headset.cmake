# The headset: the OpenXR loader and the library which shows the images of the eyes in it. The presenter and the VR player which use it are in
# src/runtime and src/VRPlayer; they are 64-bit whatever the application is (the playback is done by the converter, which has both builds).
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    return()
endif()

# The OpenXR loader, which finds the runtime of the headset: a library of its own, and nothing else of the SDK.
set(BUILD_API_LAYERS         OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS  OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS              OFF CACHE BOOL "" FORCE)
set(DYNAMIC_LOADER           OFF CACHE BOOL "" FORCE)

add_subdirectory(deps/OpenXR-SDK EXCLUDE_FROM_ALL)

file(GLOB StereoXRIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/XR/*.h")
file(GLOB StereoXRSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/XR/*.cpp")

# The headset: a screen in the room, in the OpenXR runtime of whichever headset there is, which shows the images of the eyes.
add_library(StereoXR STATIC ${StereoXRIncludes} ${StereoXRSources})

target_compile_definitions(StereoXR PUBLIC XR_USE_GRAPHICS_API_D3D12 XR_USE_PLATFORM_WIN32)
target_include_directories(StereoXR PUBLIC "${DemoVRPlayer_SOURCE_DIR}/deps/OpenXR-SDK/include")
target_link_libraries     (StereoXR PUBLIC openxr_loader StereoD3D12 VRDevices)

source_group ("StereoXR include files" FILES ${StereoXRIncludes})
source_group ("StereoXR source files"  FILES ${StereoXRSources})
