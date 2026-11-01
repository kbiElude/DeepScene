# The tests.
if (DEMOVRPLAYER_BUILD_TESTS)
    file(GLOB DemoVRPlayerTestsSources CONFIGURE_DEPENDS "${DemoVRPlayer_SOURCE_DIR}/src/Tests/*.cpp")

    # The headset is of the 64-bit programs.
    if (CMAKE_SIZEOF_VOID_P EQUAL 4)
        list(FILTER DemoVRPlayerTestsSources EXCLUDE REGEX ".*/test_(headset_session|openxr_vr_device_provider|screen_geometry)\.cpp$")
    endif()

    # Sources under test which are not part of a library, and the helpers of the tests of the interceptor which these tests use.
    add_executable(DemoVRPlayerTests ${DemoVRPlayerTestsSources}
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/RecipeBake/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Analysis/RecipeBake/recipe_baker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/call_context.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/call_walker.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/executor_registry.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/eye_matrix_binder.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/frame_buffer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/frame_pacer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/frame_runner.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/frame_rewriter.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/gl_index_replayer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/gl_replay_context.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/i_call_executor.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/live_object_list.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/null_call_executor.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/object_remapper.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/overlay_chunk_source.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL/pixel_converter.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayPlayer/command_line.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayPlayer/eye_image_buffer.cpp"
                                     "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayPlayer/quit_handler.cpp"
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
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayEngineDLL"
                                                         "${DemoVRPlayer_SOURCE_DIR}/src/Replay/StereoReplayPlayer"
                                                         "${APIInterceptor_SOURCE_DIR}/src/Tests")
    target_compile_definitions(DemoVRPlayerTests PRIVATE DEMOVRPLAYER_APPS_DIR="${DemoVRPlayer_SOURCE_DIR}/apps"
                                                         DEMOVRPLAYER_TESTS_DATA_DIR="${DemoVRPlayer_SOURCE_DIR}/src/Tests/data")
    target_link_libraries(DemoVRPlayerTests gtest_main APIInterceptorTestSupport APIDumpAnalysis APIDumpAnalysisFrontends APIDumpLoader D3D8Utils InjectionLauncher ws2_32 RecipeApplier RecipeLoader StereoCore StereoReplayEngineDLL StereoSession StereoTransport)

    enable_testing()
    add_test(NAME DemoVRPlayerTests COMMAND DemoVRPlayerTests)
endif()
