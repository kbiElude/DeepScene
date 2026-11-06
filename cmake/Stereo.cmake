# The libraries of the stereo conversion: what a converter which plays an application as it runs decides, and what carries its images to the
# program which shows them. The converter, the launcher and the presenter are in src/runtime.

file(GLOB StereoSessionIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/Session/*.h")
file(GLOB StereoSessionSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Session/*.cpp")

# How the launcher hands a playback over to the converter in the application: through the environment of the application.
add_library(StereoSession STATIC ${StereoSessionIncludes} ${StereoSessionSources})

source_group ("StereoSession include files" FILES ${StereoSessionIncludes})
source_group ("StereoSession source files"  FILES ${StereoSessionSources})

file(GLOB StereoCoreIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/Core/*.h")
file(GLOB StereoCoreSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Core/*.cpp")

# What a converter which plays an application as it runs decides, whatever the API is.
add_library(StereoCore STATIC ${StereoCoreIncludes} ${StereoCoreSources})

target_link_libraries(StereoCore PUBLIC RecipeApplier)

source_group ("Stereo Core include files" FILES ${StereoCoreIncludes})
source_group ("Stereo Core source files"  FILES ${StereoCoreSources})

file(GLOB StereoD3D7Includes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/D3D7/*.h")
file(GLOB StereoD3D7Sources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/D3D7/*.cpp")

# What is done to the vertices of a Direct3D 7 draw for an eye, whoever holds them: a dump that is replayed, or an application that runs.
add_library(StereoD3D7 STATIC ${StereoD3D7Includes} ${StereoD3D7Sources})

target_link_libraries(StereoD3D7 PUBLIC RecipeLoader)

source_group ("StereoD3D7 include files" FILES ${StereoD3D7Includes})
source_group ("StereoD3D7 source files"  FILES ${StereoD3D7Sources})

file(GLOB StereoTransportIncludes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/Transport/*.h")
file(GLOB StereoTransportSources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Transport/*.cpp")

# The images which go from the application to the program which shows them.
add_library(StereoTransport STATIC ${StereoTransportIncludes} ${StereoTransportSources})

source_group ("StereoTransport include files" FILES ${StereoTransportIncludes})
source_group ("StereoTransport source files"  FILES ${StereoTransportSources})

file(GLOB StereoD3D12Includes CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/include/Stereo/D3D12/*.h")
file(GLOB StereoD3D12Sources CONFIGURE_DEPENDS  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/D3D12/*.cpp")

# The images of the eyes on the GPU: what the memory of the channel is opened as, to be copied to the textures of a device.
add_library(StereoD3D12 STATIC ${StereoD3D12Includes} ${StereoD3D12Sources})

target_link_libraries(StereoD3D12 PUBLIC StereoTransport d3d12 dxgi)

source_group ("StereoD3D12 include files" FILES ${StereoD3D12Includes})
source_group ("StereoD3D12 source files"  FILES ${StereoD3D12Sources})
