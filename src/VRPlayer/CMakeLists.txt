# The front-end of the VR playback: a window which lists the applications and starts one in the headset. 64-bit whatever the application is
# (the playback is done by the converter, which has both builds).
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(VRPlayer WIN32 ${Sources})

target_link_libraries(VRPlayer AppCatalog InjectionLauncher StereoCore StereoTransport StereoXR UIHost VRDevices)

source_group ("VRPlayer source files" FILES ${Sources})
