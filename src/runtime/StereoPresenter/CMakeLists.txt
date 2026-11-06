# The window which shows what the converter of an application passes on, and shows it in the headset. 64-bit whatever the application is.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(StereoPresenter WIN32 ${Sources})

target_link_libraries(StereoPresenter StereoD3D12 StereoSession StereoTransport StereoXR UIHost winmm)

source_group ("StereoPresenter source files" FILES ${Sources})
