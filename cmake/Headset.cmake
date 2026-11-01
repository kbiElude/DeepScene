# The headset, the window which shows what the converter of an application passes on, and the VR player. They are 64-bit whatever the
# application is (the playback is done by the converter, which has both builds).
if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    return()
endif()

# The OpenXR loader, which finds the runtime of the headset: a library of its own, and nothing else of the SDK.
set(BUILD_API_LAYERS         OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS  OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS              OFF CACHE BOOL "" FORCE)
set(DYNAMIC_LOADER           OFF CACHE BOOL "" FORCE)

add_subdirectory(deps/OpenXR-SDK EXCLUDE_FROM_ALL)

file(GLOB StereoXRIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/StereoXR/*.h")
file(GLOB StereoXRSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/StereoXR/*.cpp")

# The headset: a screen in the room, in the OpenXR runtime of whichever headset there is, which shows the images of the eyes.
add_library(StereoXR STATIC ${StereoXRIncludes} ${StereoXRSources})

target_compile_definitions(StereoXR PUBLIC XR_USE_GRAPHICS_API_D3D12 XR_USE_PLATFORM_WIN32)
target_include_directories(StereoXR PUBLIC "${DemoVRPlayer_SOURCE_DIR}/deps/OpenXR-SDK/include")
target_link_libraries     (StereoXR PUBLIC openxr_loader StereoD3D12 VRDevices)

source_group ("StereoXR include files" FILES ${StereoXRIncludes})
source_group ("StereoXR source files"  FILES ${StereoXRSources})

file(GLOB StereoPresenterSources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/StereoPresenter/*.cpp"
                                                   "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/StereoPresenter/*.h")

# The window which shows what the converter of an application passes on.
add_executable(StereoPresenter WIN32 ${StereoPresenterSources})

target_include_directories(StereoPresenter PRIVATE "${DemoVRPlayer_SOURCE_DIR}/src")
target_link_libraries     (StereoPresenter StereoD3D12 StereoSession StereoTransport StereoXR UIHost winmm)

source_group ("StereoPresenter source files" FILES ${StereoPresenterSources})

file(GLOB VRPlayerSources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/VR/VRPlayer/*.cpp"
                                            "${DemoVRPlayer_SOURCE_DIR}/src/VR/VRPlayer/*.h")

# The front-end of the VR playback: a window.
add_executable(VRPlayer WIN32 ${VRPlayerSources})

target_include_directories(VRPlayer PRIVATE "${DemoVRPlayer_SOURCE_DIR}/src")
target_link_libraries     (VRPlayer AppCatalog InjectionLauncher StereoCore StereoTransport StereoXR UIHost VRDevices)

source_group ("VRPlayer source files" FILES ${VRPlayerSources})
