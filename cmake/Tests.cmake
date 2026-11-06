# The tests.
if (DEMOVRPLAYER_BUILD_TESTS)
    file(GLOB DemoVRPlayerTestsSources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/tests/*.cpp")

    # The headset is of the 64-bit programs.
    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        list(FILTER DemoVRPlayerTestsSources EXCLUDE REGEX ".*/test_(headset_session|openxr_vr_device_provider|screen_geometry)\.cpp$")
    endif()

    # Sources under test which are not part of a library, and the helpers of the tests of the interceptor which these tests use.
    add_executable(DemoVRPlayerTests ${DemoVRPlayerTestsSources}
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/analysis/api_dump_slicer/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/analysis/recipe_bake/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/analysis/recipe_bake/overlay_candidate_finder.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/analysis/recipe_bake/recipe_baker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/analysis/recipe_bake/recipe_checker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/d3d7_eye_renderer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/d3d7_surface_twins.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/d3d7_windowed_mode.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/environment.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/eye_image_limits.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/invisible_windows.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/own_call_scope.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/launcher/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/launcher/log_summary.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/presenter/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/consistency_checker/frame_sampler.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/consistency_checker/overlay_probe.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/consistency_checker/replay_profiler.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/d3d7_draw_rewriter.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/eye_matrix_binder.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/frame_buffer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/frame_hurry.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/frame_pacer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/frame_rewriter.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/frame_runner.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/overlay_chunk_source.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/present_skipper.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/slice_skipper.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/player/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/player/eye_image_buffer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/player/quit_handler.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/vr_player/app_session.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/vr_player/command_line.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Common/api_call_gate.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Common/callback_registry.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Common/callbacks.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Common/data_chunk_store.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Common/memory_chunk.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Interception/Hooks/OpenGL/buffer_manager.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Interception/Hooks/OpenGL/buffer_mapping_manager.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Interception/Hooks/OpenGL/client_array_manager.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Interception/Hooks/OpenGL/param_counts.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Interception/Hooks/OpenGL/pixel_store_manager.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Interception/Hooks/OpenGL/string_array_chunk.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Interception/Hooks/OpenGL/string_chunk.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Interception/Hooks/OpenGL/texture_manager.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Interception/Hooks/OpenGL/utils.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Interception/Hooks/WGL/utils.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Tests/argument_info_checker.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Tests/d3d8_struct_bytes.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/Tests/workload_file_builder.cpp")

    target_include_directories(DemoVRPlayerTests PRIVATE "${DemoVRPlayer_SOURCE_DIR}/src"
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll"
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll"
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/player"
                                                         "${APIInterceptor_SOURCE_DIR}/src/Tests")
    target_compile_definitions(DemoVRPlayerTests PRIVATE DEMOVRPLAYER_APPS_DIR="${DemoVRPlayer_SOURCE_DIR}/apps"
                                                         DEMOVRPLAYER_REPOSITORY_ROOT="${DemoVRPlayer_SOURCE_DIR}"
                                                         DEMOVRPLAYER_TESTS_DATA_DIR="${DemoVRPlayer_SOURCE_DIR}/src/tests/data")
    target_link_libraries(DemoVRPlayerTests gtest_main APIInterceptorTestSupport APIDumpLoader Analysis D3D7Utils D3D8Utils Headset InjectionLauncher ReplayEngineCore Stereo StereoReplayEngineDLL Utils VRPlayerBackend ws2_32)

    enable_testing()
    add_test(NAME DemoVRPlayerTests COMMAND DemoVRPlayerTests)
endif()
