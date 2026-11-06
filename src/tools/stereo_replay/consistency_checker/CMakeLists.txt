# Replays a workload and checks it: compares the replays with each other, takes screenshots and contact sheets, profiles, and makes the reports of
# the overlay probe.
file(GLOB Sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                                    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

add_executable(StereoReplayConsistencyChecker ${Sources})

target_link_libraries(StereoReplayConsistencyChecker APIDumpNames Analysis Stereo StereoReplayEngineDLL)

set_architecture_name(StereoReplayConsistencyChecker)

if (CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_options(StereoReplayConsistencyChecker PRIVATE /LARGEADDRESSAWARE)
endif()

source_group ("StereoReplayConsistencyChecker source files" FILES ${Sources})
