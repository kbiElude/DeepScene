# What converts an application to stereo as it runs, and what carries its images to the program which shows them. Each of the programs
# is built for x86 and for x64 (see build.bat), since the launcher has to be of the architecture of the application.

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

file(GLOB StereoConverterDLLSources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/ConverterDLL/*.cpp"
                                                      "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/ConverterDLL/*.h")

# The stereo converter: a DLL which goes into the application. The pixel converter of the replay engine is used as it is, to turn a back
# buffer of any format into BGRA.
add_library(StereoConverterDLL SHARED ${StereoConverterDLLSources}
                                      "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL/pixel_converter.cpp")

target_link_libraries(StereoConverterDLL APIDumpAnalysisFrontends APIInterceptor D3D8Utils StereoCore StereoSession StereoTransport)

source_group ("StereoConverterDLL source files" FILES ${StereoConverterDLLSources})

file(GLOB StereoLauncherSources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Launcher/*.cpp"
                                                  "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Launcher/*.h")

# The launcher which puts the converter in the application.
add_executable(StereoLauncher ${StereoLauncherSources})

target_include_directories(StereoLauncher PRIVATE "${DemoVRPlayer_SOURCE_DIR}/src")
target_link_libraries     (StereoLauncher RecipeLoader InjectionLauncher StereoSession)

add_dependencies(StereoLauncher StereoConverterDLL)

source_group ("StereoLauncher source files" FILES ${StereoLauncherSources})
