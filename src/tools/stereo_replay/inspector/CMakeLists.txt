# Replays a workload and checks it: compares the replays with each other, takes screenshots and contact sheets, profiles, and makes the reports of
# the overlay report.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(StereoReplayInspector ${Sources})

target_link_libraries(StereoReplayInspector APIDumpNames Analysis Stereo StereoReplayEngineDLL)

set_architecture_name(StereoReplayInspector)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_options(StereoReplayInspector PRIVATE /LARGEADDRESSAWARE)
endif()

source_group ("StereoReplayInspector source files" FILES ${Sources})
