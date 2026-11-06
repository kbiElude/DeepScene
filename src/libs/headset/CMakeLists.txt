# Headset: the images of the eyes on the GPU (D3D12: the uploader which copies the memory of the channel to the textures of a device), the
# OpenXR session which shows them as a screen in the room (XR), and the devices which the VR player can play on (VRDevices). The session is of
# the 64-bit programs: a headset is shown the images by a program of the architecture of the machine, whatever the application is.
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    list(FILTER Sources EXCLUDE REGEX "/xr/")
endif()

add_library(Headset STATIC ${Sources})

target_include_directories(Headset PUBLIC "${DemoVRPlayer_SOURCE_DIR}/src/libs")
target_link_libraries     (Headset PUBLIC Stereo Utils d3d12 dxgi)

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    target_compile_definitions(Headset PUBLIC XR_USE_GRAPHICS_API_D3D12 XR_USE_PLATFORM_WIN32)
    target_include_directories(Headset PUBLIC "${DemoVRPlayer_SOURCE_DIR}/deps/OpenXR-SDK/include")
    target_link_libraries     (Headset PUBLIC openxr_loader)
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
