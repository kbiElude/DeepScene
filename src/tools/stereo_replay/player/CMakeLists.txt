# Plays a workload for both eyes in a window. The window is the one of UIHost, drawn with the Dear ImGui of the interceptor.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(StereoReplayPlayer ${Sources})

target_link_libraries(StereoReplayPlayer APIDumpLoader Analysis ImGui ReplayPlayerSupport StereoReplayEngineDLL VR)

set_architecture_name(StereoReplayPlayer)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    # A segment of the workload is 100 to 200 MB, and the replay reads several ahead: a 32-bit process has two gigabytes unless it says it can have four.
    target_link_options(StereoReplayPlayer PRIVATE /LARGEADDRESSAWARE)
endif()

source_group ("StereoReplayPlayer source files" FILES ${Sources})
