# The stereo converter: a DLL which goes into the application and draws each frame for both eyes. The pixel converter of the replay engine
# is used as it is, to turn a back buffer of any format into BGRA. Built for x86 and for x64 (see build.bat), since the launcher has to be
# of the architecture of the application. What is of Direct3D 7 is in D3D7/, what is of Direct3D 8 in D3D8/ and what is of OpenGL in OpenGL/;
# what routes the calls to them and what they share is here.
file(GLOB_RECURSE Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                            "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_library(StereoConverterDLL SHARED ${Sources}
                                      "${APIInterceptor_SOURCE_DIR}/src/replay/replay_engine_dll/pixel_converter.cpp")

# Shared files are included by their own name ("environment.h"); files of an API folder with the folder in front ("D3D7/d3d7_eye_renderer.h").
target_include_directories(StereoConverterDLL PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
                                                      "${APIInterceptor_SOURCE_DIR}/src/replay/replay_engine_dll")

target_link_libraries(StereoConverterDLL APIInterceptor D3D7Utils D3D8Utils Stereo opengl32 winmm)

set_architecture_name(StereoConverterDLL)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${Sources})
