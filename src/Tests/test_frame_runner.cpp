/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for FrameRunner: which calls of a frame are executed in which execution, and when the back buffer is captured.
 */
#include <gtest/gtest.h>
#include "frame_runner.h"
#include "replay_test_doubles.h"
#include <algorithm>
#include <string>

class FrameRunnerTest : public ::testing::Test
{
protected:
    /* Says to which class a call belongs, by its function, and keeps the notes that the runner asks for. */
    class Executor : public Tests::StubCallExecutor
    {
    public:
        ::Replay::ReplayEngineDLL::ExecutionClass get_execution_class(const Capture::APIDumpLoader::DumpedAPICall& in_call) const override
        {
            ::Replay::ReplayEngineDLL::ExecutionClass result = ::Replay::ReplayEngineDLL::ExecutionClass::FIRST_TIME_ONLY;

            if (in_call.func == AFTER_ALL_FUNCTION)
            {
                result = ::Replay::ReplayEngineDLL::ExecutionClass::AFTER_ALL;
            }
            else
            if (in_call.func == EVERY_TIME_FUNCTION)
            {
                result = ::Replay::ReplayEngineDLL::ExecutionClass::EVERY_TIME;
            }

            return result;
        }

        void restore_state() override
        {
            event_vec->push_back("restore");
        }

        void save_state() override
        {
            event_vec->push_back("save");
        }

        std::vector<std::string>* event_vec = nullptr;
    };

    /* The function of each class of call. */
    static const APIInterceptor::APIFunction AFTER_ALL_FUNCTION;
    static const APIInterceptor::APIFunction EVERY_TIME_FUNCTION;
    static const APIInterceptor::APIFunction FIRST_TIME_FUNCTION;

    void add_call(const APIInterceptor::APIFunction& in_function)
    {
        Capture::APIDumpLoader::DumpedAPICall call;

        call.func = in_function;

        m_buffer.add_call(call,
                          m_source);
    }

    /* Runs the frame; the notes are what the runner did, in order. */
    bool run(const bool& in_is_complete)
    {
        m_executor.event_vec = &m_event_vec;

        return Stereo::Replay::EngineDLL::FrameRunner::run(m_buffer,
                                                           in_is_complete,
                                                           m_skips_repeated_calls,
                                                           m_options,
                                                           {&m_executor},
                                                           [this](const uint32_t& in_n_execution)
                                                           {
                                                               (void) in_n_execution;

                                                               return m_begin_succeeds;
                                                           },
                                                           [this](const Capture::APIDumpLoader::DumpedAPICall& in_call)
                                                           {
                                                               (void) in_call;

                                                               return &m_executor;
                                                           },
                                                           [this](const Capture::APIDumpLoader::DumpedAPICall&     in_call,
                                                                  const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                                  const size_t&                                in_n_call,
                                                                  const uint32_t&                         in_n_execution)
                                                           {
                                                               (void) in_chunk_source;
                                                               (void) in_n_call;
                                                               (void) in_n_execution;

                                                               m_event_vec.push_back(get_name(in_call.func) );

                                                               return (m_event_vec.size() != m_failing_event_count);
                                                           },
                                                           [this](const uint32_t& in_n_execution)
                                                           {
                                                               m_event_vec.push_back("capture" + std::to_string(in_n_execution) );

                                                               return m_capture_succeeds;
                                                           });
    }

    /* A frame of a call of each class, in the order of a real one: the present comes last. */
    void add_typical_frame()
    {
        add_call(EVERY_TIME_FUNCTION);
        add_call(FIRST_TIME_FUNCTION);
        add_call(EVERY_TIME_FUNCTION);
        add_call(AFTER_ALL_FUNCTION);
    }

    static std::string get_name(const APIInterceptor::APIFunction& in_function)
    {
        std::string result = "?";

        if (in_function == AFTER_ALL_FUNCTION)
        {
            result = "after";
        }
        else
        if (in_function == EVERY_TIME_FUNCTION)
        {
            result = "every";
        }
        else
        if (in_function == FIRST_TIME_FUNCTION)
        {
            result = "first";
        }

        return result;
    }

    Stereo::Replay::EngineDLL::FrameBuffer           m_buffer;
    bool                                             m_begin_succeeds       = true;
    bool                                             m_capture_succeeds     = true;
    std::vector<std::string>                         m_event_vec;
    Executor                                         m_executor;
    size_t                                           m_failing_event_count  = 0;
    bool                                             m_skips_repeated_calls = false;
    Stereo::Replay::EngineDLL::FrameExecutionOptions m_options;
    Tests::MapChunkSource                            m_source;
};

const APIInterceptor::APIFunction FrameRunnerTest::AFTER_ALL_FUNCTION  = APIInterceptor::APIFUNCTION_GL_GLALPHAFUNC;
const APIInterceptor::APIFunction FrameRunnerTest::EVERY_TIME_FUNCTION = APIInterceptor::APIFUNCTION_GL_GLACCUM;
const APIInterceptor::APIFunction FrameRunnerTest::FIRST_TIME_FUNCTION = APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE;

TEST_F(FrameRunnerTest, AFrameExecutedOnceRunsEveryCallOnceAndThePresentLast)
{
    m_options.mode = Stereo::Replay::EngineDLL::FrameExecution::BUFFERED_ONCE;

    add_typical_frame();

    ASSERT_TRUE(run(true) );

    EXPECT_EQ(m_event_vec,
              (std::vector<std::string>{"every", "first", "every", "capture0", "after"}) );
}

TEST_F(FrameRunnerTest, AFrameExecutedTwiceRepeatsWhatIsToBeRepeated)
{
    m_options.mode = Stereo::Replay::EngineDLL::FrameExecution::TWICE;

    add_typical_frame();

    ASSERT_TRUE(run(true) );

    EXPECT_EQ(m_event_vec,
              (std::vector<std::string>{"every", "first", "every", "capture0",
                                        "every", "every", "capture1",
                                        "after"}) );
}

TEST_F(FrameRunnerTest, TheOptionToRestoreTheStateSavesItOnceAndRestoresItBeforeTheSecondExecution)
{
    m_options.mode           = Stereo::Replay::EngineDLL::FrameExecution::TWICE;
    m_options.restores_state = true;

    add_typical_frame();

    ASSERT_TRUE(run(true) );

    EXPECT_EQ(m_event_vec,
              (std::vector<std::string>{"save",
                                        "every", "first", "every", "capture0",
                                        "restore",
                                        "every", "every", "capture1",
                                        "after"}) );
}

TEST_F(FrameRunnerTest, TheStateIsLeftAloneIfTheFrameIsExecutedOnce)
{
    m_options.mode           = Stereo::Replay::EngineDLL::FrameExecution::BUFFERED_ONCE;
    m_options.restores_state = true;

    add_typical_frame();

    ASSERT_TRUE(run(true) );

    EXPECT_EQ(std::count(m_event_vec.begin(), m_event_vec.end(), "save"),
              0);
    EXPECT_EQ(std::count(m_event_vec.begin(), m_event_vec.end(), "restore"),
              0);
}

TEST_F(FrameRunnerTest, AFrameCutShortIsExecutedOnceInItsOrderAndNotCaptured)
{
    m_options.mode = Stereo::Replay::EngineDLL::FrameExecution::TWICE;

    add_typical_frame();

    ASSERT_TRUE(run(false) );

    EXPECT_EQ(m_event_vec,
              (std::vector<std::string>{"every", "first", "every", "after"}) );
}

TEST_F(FrameRunnerTest, ACallThatFailsEndsTheRun)
{
    m_options.mode        = Stereo::Replay::EngineDLL::FrameExecution::TWICE;
    m_failing_event_count = 3;

    add_typical_frame();

    EXPECT_FALSE(run(true) );
    EXPECT_EQ   (m_event_vec,
                 (std::vector<std::string>{"every", "first", "every"}) );
}

TEST_F(FrameRunnerTest, ACallThatFailsInTheSecondExecutionEndsTheRunBeforeThePresent)
{
    m_options.mode        = Stereo::Replay::EngineDLL::FrameExecution::TWICE;
    m_failing_event_count = 6; /* the first call of the second execution */

    add_typical_frame();

    EXPECT_FALSE(run(true) );

    for (const auto& event : m_event_vec)
    {
        EXPECT_NE(event,
                  "after");
    }
}

TEST_F(FrameRunnerTest, DestroyingCallsInTheMiddleOfAFrameWaitForTheLastExecution)
{
    m_options.mode = Stereo::Replay::EngineDLL::FrameExecution::TWICE;

    add_call(EVERY_TIME_FUNCTION);
    add_call(AFTER_ALL_FUNCTION); /* the frame destroys an object, say */
    add_call(EVERY_TIME_FUNCTION);
    add_call(AFTER_ALL_FUNCTION); /* the present */

    ASSERT_TRUE(run(true) );

    EXPECT_EQ(m_event_vec,
              (std::vector<std::string>{"every", "every", "capture0",
                                        "every", "every", "capture1",
                                        "after", "after"}) );
}

TEST_F(FrameRunnerTest, AnEmptyFrameIsNothing)
{
    m_options.mode = Stereo::Replay::EngineDLL::FrameExecution::TWICE;

    ASSERT_TRUE(run(true) );

    EXPECT_EQ(m_event_vec,
              (std::vector<std::string>{"capture0", "capture1"}) );
}

TEST_F(FrameRunnerTest, AFrameThatSkipsTheRepeatedCallsIsExecutedOnceWithoutThem)
{
    m_options.mode         = Stereo::Replay::EngineDLL::FrameExecution::TWICE;
    m_skips_repeated_calls = true;

    add_typical_frame();

    ASSERT_TRUE(run(true) );

    EXPECT_EQ(m_event_vec,
              (std::vector<std::string>{"first", "capture0", "after"}) );
}

TEST_F(FrameRunnerTest, TheCallsAreToldTheirPositionAndTheExecution)
{
    std::vector<std::pair<size_t, uint32_t>> position_vec;

    m_options.mode = Stereo::Replay::EngineDLL::FrameExecution::TWICE;

    add_typical_frame();

    m_executor.event_vec = &m_event_vec;

    ASSERT_TRUE(Stereo::Replay::EngineDLL::FrameRunner::run(m_buffer,
                                                            true,
                                                            false,
                                                            m_options,
                                                            {&m_executor},
                                                            [](const uint32_t& in_n_execution)
                                                            {
                                                                (void) in_n_execution;

                                                                return true;
                                                            },
                                                            [this](const Capture::APIDumpLoader::DumpedAPICall& in_call)
                                                            {
                                                                (void) in_call;

                                                                return &m_executor;
                                                            },
                                                            [&](const Capture::APIDumpLoader::DumpedAPICall&  in_call,
                                                                const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                                const size_t&                                in_n_call,
                                                                const uint32_t&                      in_n_execution)
                                                            {
                                                                (void) in_call;
                                                                (void) in_chunk_source;

                                                                position_vec.push_back({in_n_call,
                                                                                        in_n_execution});

                                                                return true;
                                                            },
                                                            [](const uint32_t& in_n_execution)
                                                            {
                                                                (void) in_n_execution;

                                                                return true;
                                                            }) );

    /* every, first, every | every, every | after (the last execution) */
    const std::vector<std::pair<size_t, uint32_t>> expected_vec =
    {
        {0, 0}, {1, 0}, {2, 0},
        {0, 1}, {2, 1},
        {3, 1}
    };

    EXPECT_EQ(position_vec,
              expected_vec);
}

TEST_F(FrameRunnerTest, AFrameStopsWhenAnExecutionCannotBegin)
{
    add_typical_frame();

    m_begin_succeeds = false;

    EXPECT_FALSE(run(true) );
    EXPECT_TRUE(m_event_vec.empty() );
}

TEST_F(FrameRunnerTest, AFrameStopsWhenACaptureFails)
{
    m_options.mode = Stereo::Replay::EngineDLL::FrameExecution::TWICE;

    add_typical_frame();

    m_capture_succeeds = false;

    EXPECT_FALSE(run(true) );
    EXPECT_EQ(m_event_vec,
              (std::vector<std::string>{"every", "first", "every", "capture0"}) );
}
