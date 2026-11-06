# The stereo converter: a DLL which goes into the application and draws each frame for both eyes. The pixel converter of the replay engine
# is used as it is, to turn a back buffer of any format into BGRA. Built for x86 and for x64 (see build.bat), since the launcher has to be
# of the architecture of the application.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(StereoConverterDLL SHARED ${Sources}
                                      "${APIInterceptor_SOURCE_DIR}/src/Replay/ReplayEngineDLL/pixel_converter.cpp")

target_include_directories(StereoConverterDLL PRIVATE "${APIInterceptor_SOURCE_DIR}/src/Replay/ReplayEngineDLL")

target_link_libraries(StereoConverterDLL APIInterceptor D3D7Utils D3D8Utils Stereo winmm)

set_architecture_name(StereoConverterDLL)

source_group ("StereoConverterDLL source files" FILES ${Sources})
