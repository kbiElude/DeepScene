/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the replay profile: the time of a call is what passes between the engine asking whether to execute it and its saying that it was replayed,
 * and it is summed by function and by frame.
 */
#include <gtest/gtest.h>
#include "tools/stereo_replay/consistency_checker/replay_profiler.h"

namespace
{
    /* A clock which gives the times it is told to, one at a time. */
    class ScriptedClock
    {
    public:
        explicit ScriptedClock(const std::vector<uint64_t>& in_value_vec)
            :m_n_value(0),
             m_value_vec(in_value_vec)
        {
            /* Stub */
        }

        uint64_t read()
        {
            return m_value_vec.at(m_n_value++);
        }

    private:
        size_t                m_n_value;
        std::vector<uint64_t> m_value_vec;
    };

    /* The engine's side of one call. */
    void replay_call(Stereo::Replay::ConsistencyChecker::ReplayProfiler* inout_profiler_ptr,
                     const APIInterceptor::APIFunction&                  in_function,
                     const uint32_t&                                     in_n_frame,
                     const bool&                                         in_is_executed)
    {
        Capture::APIDumpLoader::DumpedAPICall call;

        call.func = in_function;

        inout_profiler_ptr->should_execute_call(call,
                                                in_n_frame);
        inout_profiler_ptr->on_call_replayed   (call,
                                                in_n_frame,
                                                in_is_executed);
    }
}

TEST(ReplayProfiler, TheTimeOfACallIsSummedByFunctionAndByFrame)
{
    ScriptedClock                                       clock({0, 100, 100, 400, 400, 450});
    Stereo::Replay::ConsistencyChecker::ReplayProfiler profiler([&clock]()
                                                                {
                                                                    return clock.read();
                                                                });

    replay_call(&profiler,
                APIInterceptor::APIFUNCTION_GL_GLACCUM,
                0,
                true);
    replay_call(&profiler,
                APIInterceptor::APIFUNCTION_GL_GLBEGIN,
                0,
                true);
    replay_call(&profiler,
                APIInterceptor::APIFUNCTION_GL_GLACCUM,
                1,
                true);

    EXPECT_EQ(profiler.get_n_calls(),
              3u);
    EXPECT_EQ(profiler.get_total_n_nanoseconds(),
              450u);

    const auto function_vec = profiler.get_slowest_functions(10);

    ASSERT_EQ(function_vec.size(),
              2u);
    EXPECT_EQ(function_vec.at(0).function,
              APIInterceptor::APIFUNCTION_GL_GLBEGIN);
    EXPECT_EQ(function_vec.at(0).total_n_nanoseconds,
              300u);
    EXPECT_EQ(function_vec.at(1).function,
              APIInterceptor::APIFUNCTION_GL_GLACCUM);
    EXPECT_EQ(function_vec.at(1).n_calls,
              2u);
    EXPECT_EQ(function_vec.at(1).total_n_nanoseconds,
              150u);
    EXPECT_EQ(function_vec.at(1).max_n_nanoseconds,
              100u);

    const auto frame_vec = profiler.get_slowest_frames(10);

    ASSERT_EQ(frame_vec.size(),
              2u);
    EXPECT_EQ(frame_vec.at(0).n_frame,
              0u);
    EXPECT_EQ(frame_vec.at(0).n_calls,
              2u);
    EXPECT_EQ(frame_vec.at(0).total_n_nanoseconds,
              400u);
    EXPECT_EQ(frame_vec.at(0).slowest_function,
              APIInterceptor::APIFUNCTION_GL_GLBEGIN);
    EXPECT_EQ(frame_vec.at(0).slowest_n_nanoseconds,
              300u);
    EXPECT_EQ(frame_vec.at(1).n_frame,
              1u);
}

TEST(ReplayProfiler, OnlyTheAskedForNumberOfEntriesIsGivenAndASkippedCallHasNoTime)
{
    ScriptedClock                                       clock({0, 100, 100, 400, 400, 450});
    Stereo::Replay::ConsistencyChecker::ReplayProfiler profiler([&clock]()
                                                                {
                                                                    return clock.read();
                                                                });

    replay_call(&profiler,
                APIInterceptor::APIFUNCTION_GL_GLACCUM,
                0,
                true);
    replay_call(&profiler,
                APIInterceptor::APIFUNCTION_GL_GLBEGIN,
                1,
                false); /* skipped: it has no entry */
    replay_call(&profiler,
                APIInterceptor::APIFUNCTION_GL_GLBEGIN,
                2,
                true);

    EXPECT_EQ(profiler.get_n_calls(),
              2u);
    EXPECT_EQ(profiler.get_slowest_functions(1).size(),
              1u);
    EXPECT_EQ(profiler.get_slowest_frames(5).size(),
              2u);
}
