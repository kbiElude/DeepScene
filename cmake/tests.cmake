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
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/analysis/recipe_bake/parity_auditor.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/analysis/recipe_bake/pass_camera_reader.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/analysis/recipe_bake/recipe_baker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/analysis/recipe_bake/recipe_checker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/D3D7/d3d7_eye_renderer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/D3D7/d3d7_surface_twins.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/D3D7/d3d7_windowed_mode.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/OpenGL/gl_color_target_formats.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/OpenGL/gl_run_call_classifier.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/OpenGL/gl_texture_twin_table.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/environment_variable_reader.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/eye_image_limits.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/invisible_window_manager.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/live_call_analyzer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/own_call_scope.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/pixel_row_writer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/render_scale_planner.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll/resolution_hazard_gate.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/launcher/button_clicker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/launcher/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/launcher/log_summarizer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/runtime/presenter/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/inspector/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/inspector/contact_sheet_frame_picker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/inspector/crash_breadcrumb.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/inspector/frame_repeatability.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/inspector/overlay_report.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/inspector/overlay_report_page.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/inspector/redundant_state_counter.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/inspector/replay_profiler.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/D3D7/d3d7_draw_rewriter.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/content_slice_call_handler.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/eye_matrix_binder.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/frame_buffer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/frame_pacer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/frame_rewriter.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/frame_runner.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/gl_attribute_coverage.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/gl_display_list_tracker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/gl_slice_call_handler.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/gl_texture_parameter_cache.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/hurried_frame_selector.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/overlay_chunk_source.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/present_skipper.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll/slice_skipper.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/player/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/player/eye_image_buffer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/player/quit_handler.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/vr_player/app_session.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/vr_player/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/vr_player/thumbnail_textures.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/common/api_call_gate.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/common/callback_registry.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/common/callbacks.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/common/chunk_pool.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/common/data_chunk_store.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/common/memory_chunk.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/interception/hooks/OpenGL/buffer_binding_tracker.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/interception/hooks/OpenGL/client_vertex_array_tracker.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/interception/hooks/OpenGL/mapped_buffer_write_tracker.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/interception/hooks/OpenGL/param_counts.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/interception/hooks/OpenGL/pixel_store_modes.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/interception/hooks/OpenGL/string_array_chunk.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/interception/hooks/OpenGL/string_chunk.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/interception/hooks/OpenGL/texture_binding_tracker.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/interception/hooks/OpenGL/utils.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/interception/hooks/WGL/utils.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/tests/argument_info_checker.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/tests/d3d8_struct_bytes.cpp"
                                     "${APIInterceptor_SOURCE_DIR}/src/tests/workload_file_builder.cpp")

    target_include_directories(DemoVRPlayerTests PRIVATE "${DemoVRPlayer_SOURCE_DIR}/src"
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/runtime/converter_dll"
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/engine_dll"
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/tools/stereo_replay/player"
                                                         "${APIInterceptor_SOURCE_DIR}/src/tests")
    target_compile_definitions(DemoVRPlayerTests PRIVATE DEMOVRPLAYER_APPS_DIR="${DemoVRPlayer_SOURCE_DIR}/apps"
                                                         DEMOVRPLAYER_REPOSITORY_ROOT="${DemoVRPlayer_SOURCE_DIR}"
                                                         DEMOVRPLAYER_TESTS_DATA_DIR="${DemoVRPlayer_SOURCE_DIR}/src/tests/data")
    target_link_libraries(DemoVRPlayerTests gtest_main APIInterceptorTestSupport APIDumpLoader APIDumpNames Analysis D3D7Utils D3D8Utils Headset InjectionLauncher ReplayEngineCore Stereo StereoReplayEngineDLL Utils VRPlayerBackend d3d11 windowscodecs ws2_32)

    enable_testing()
    add_test(NAME DemoVRPlayerTests COMMAND DemoVRPlayerTests)
endif()
