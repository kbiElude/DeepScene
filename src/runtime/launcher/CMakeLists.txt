# The launcher which puts the converter in the application. Built for x86 and for x64, like the converter.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(StereoLauncher ${Sources})

target_link_libraries(StereoLauncher InjectionLauncher Stereo)

add_dependencies(StereoLauncher StereoConverterDLL)

source_group ("StereoLauncher source files" FILES ${Sources})
