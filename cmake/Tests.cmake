# The tests.
if (DEMOVRPLAYER_BUILD_TESTS)
    file(GLOB DemoVRPlayerTestsSources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Tests/*.cpp")

    # The headset is of the 64-bit programs.
    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        list(FILTER DemoVRPlayerTestsSources EXCLUDE REGEX ".*/test_(headset_session|openxr_vr_device_provider|screen_geometry)\.cpp$")
    endif()

    # Sources under test which are not part of a library, and the helpers of the tests of the interceptor which these tests use.
    add_executable(DemoVRPlayerTests ${DemoVRPlayerTestsSources}
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/APIDumpSlicer/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/RecipeBake/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/RecipeBake/recipe_baker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/RecipeBake/recipe_checker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/ConverterDLL/d3d7_eye_renderer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/ConverterDLL/d3d7_surface_twins.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/ConverterDLL/d3d7_windowed_mode.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/ConverterDLL/environment.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/ConverterDLL/eye_image_limits.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/ConverterDLL/invisible_windows.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/ConverterDLL/own_call_scope.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Launcher/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Launcher/log_summary.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Presenter/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/ConsistencyChecker/frame_sampler.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/ConsistencyChecker/replay_profiler.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL/d3d7_draw_rewriter.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL/eye_matrix_binder.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL/frame_buffer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL/frame_pacer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL/frame_rewriter.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL/frame_runner.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL/overlay_chunk_source.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/Player/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/Player/eye_image_buffer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/Player/quit_handler.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/VR/VRPlayer/app_session.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/VR/VRPlayer/command_line.cpp"
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
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/ConverterDLL"
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/EngineDLL"
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/Stereo/Replay/Player"
                                                         "${APIInterceptor_SOURCE_DIR}/src/Tests")
    target_compile_definitions(DemoVRPlayerTests PRIVATE DEMOVRPLAYER_APPS_DIR="${DemoVRPlayer_SOURCE_DIR}/apps"
                                                         DEMOVRPLAYER_TESTS_DATA_DIR="${DemoVRPlayer_SOURCE_DIR}/src/Tests/data")
    target_link_libraries(DemoVRPlayerTests gtest_main APIInterceptorTestSupport ReplayEngineCore APIDumpAnalysis APIDumpAnalysisFrontends APIDumpDependencies APIDumpLoader APIDumpSlicing AppCatalog AppInstaller D3D7Utils D3D8Utils InjectionLauncher ws2_32 RecipeApplier RecipeLoader StereoCore StereoD3D12 StereoD3D7 StereoReplayEngineDLL StereoReplayFrameImages StereoSession StereoTransport VRDevices)

    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        target_link_libraries(DemoVRPlayerTests StereoXR)
    endif()

    enable_testing()
    add_test(NAME DemoVRPlayerTests COMMAND DemoVRPlayerTests)
endif()
