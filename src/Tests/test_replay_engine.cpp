/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the playback control of the replay engine: frame-boundary pausing, resetting and interception.
 */
#include <gtest/gtest.h>
#include "Replay/StereoReplayEngineDLL/entrypoints.h"
#include "workload_file_builder.h"
#include <chrono>
#include <thread>

namespace
{
    constexpr uint32_t N_CALLS_PER_FRAME = 2;
    constexpr uint32_t N_FRAMES          = 3;

    /* Pauses the replay, or skips the calls, as told. */
    class TestInterceptor : public Replay::StereoReplayEngineDLL::ICallInterceptor
    {
    public:
        TestInterceptor(Replay::StereoReplayEngineDLL::IPlaybackController* in_controller_ptr,
                                      const bool&                           in_should_pause_on_first_call,
                                      const bool&                           in_should_skip_calls)
            :m_controller_ptr            (in_controller_ptr),
             m_should_pause_on_first_call(in_should_pause_on_first_call),
             m_should_skip_calls         (in_should_skip_calls)
        {
            /* Stub */
        }

        void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                       const uint32_t&                     in_n_frame,
                                       const bool&                         in_was_executed) override
        {
            (void) in_call;
            (void) in_was_executed;

            if (m_should_pause_on_first_call &&
                in_n_frame == 0)
            {
                m_controller_ptr->pause();
            }
        }

        bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const uint32_t&                     in_n_frame) override
        {
            (void) in_call;
            (void) in_n_frame;

            return (m_should_skip_calls == false);
        }

    private:
        Replay::StereoReplayEngineDLL::IPlaybackController* m_controller_ptr;
        bool                                  m_should_pause_on_first_call;
        bool                                  m_should_skip_calls;
    };

    class ReplayEngineTest : public ::testing::Test
    {
    protected:
        void SetUp() override
        {
            std::vector<uint8_t> file_u8_vec;

            Tests::WorkloadFileBuilder::build_frames(&file_u8_vec,
                                                     N_FRAMES,
                                                     N_CALLS_PER_FRAME);
            Tests::WorkloadFileBuilder::write_file  (file_u8_vec);

            m_engine_ptr = Replay::StereoReplayEngineDLL::aiCreateReplayEngine();
        }

        void TearDown() override
        {
            Replay::StereoReplayEngineDLL::aiDestroyReplayEngine(m_engine_ptr);
            ::remove(Tests::WorkloadFileBuilder::get_filename_ptr() );
        }

        /* Polls: the replay runs on a thread of its own. */
        bool wait_for_state(const Replay::StereoReplayEngineDLL::PlaybackState& in_state)
        {
            constexpr auto TIMEOUT = std::chrono::seconds(10);

            const auto deadline = std::chrono::steady_clock::now() + TIMEOUT;
            bool       result   = false;

            while (std::chrono::steady_clock::now() < deadline)
            {
                if (m_engine_ptr->get_playback_controller_ptr()->get_status().state == in_state)
                {
                    result = true;

                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1) );
            }

            return result;
        }

        Replay::StereoReplayEngineDLL::IReplayEngine* m_engine_ptr = nullptr;
    };
}

TEST_F(ReplayEngineTest, LoadFailsForMissingFile)
{
    std::string error;

    EXPECT_FALSE(m_engine_ptr->load("this_file_does_not_exist.workload",
                                   &error) );
    EXPECT_FALSE(error.empty() );
}

TEST_F(ReplayEngineTest, PlaysTheWholeWorkload)
{
    ASSERT_TRUE(m_engine_ptr->load(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                   nullptr) );

    auto* controller_ptr = m_engine_ptr->get_playback_controller_ptr();

    EXPECT_EQ(controller_ptr->get_status().state,
              Replay::StereoReplayEngineDLL::PlaybackState::IDLE);

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Replay::StereoReplayEngineDLL::PlaybackState::FINISHED) );

    EXPECT_EQ(controller_ptr->get_status().n_calls_executed,
              static_cast<uint64_t>(N_FRAMES * (N_CALLS_PER_FRAME + 1) ));
    EXPECT_EQ(controller_ptr->get_status().n_frames_presented,
              N_FRAMES);
}

TEST_F(ReplayEngineTest, PausesAtTheNextFrameBoundary)
{
    ASSERT_TRUE(m_engine_ptr->load(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                   nullptr) );

    auto*           controller_ptr = m_engine_ptr->get_playback_controller_ptr();
    TestInterceptor interceptor    (controller_ptr,
                                    true,   /* in_should_pause_on_first_call */
                                    false); /* in_should_skip_calls          */

    m_engine_ptr->add_call_interceptor(&interceptor);

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Replay::StereoReplayEngineDLL::PlaybackState::PAUSED) );

    /* The first frame was replayed in full, and nothing of the second one. */
    EXPECT_EQ(controller_ptr->get_status().n_calls_executed,
              static_cast<uint64_t>(N_CALLS_PER_FRAME + 1) );
    EXPECT_EQ(controller_ptr->get_status().n_frames_presented,
              1u);

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Replay::StereoReplayEngineDLL::PlaybackState::FINISHED) );

    EXPECT_EQ(controller_ptr->get_status().n_frames_presented,
              N_FRAMES);

    m_engine_ptr->remove_call_interceptor(&interceptor);
}

TEST_F(ReplayEngineTest, ResetShowsTheFirstFrameAndWaits)
{
    ASSERT_TRUE(m_engine_ptr->load(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                   nullptr) );

    auto* controller_ptr = m_engine_ptr->get_playback_controller_ptr();

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Replay::StereoReplayEngineDLL::PlaybackState::FINISHED) );

    controller_ptr->reset();

    /* The first frame is replayed, and the replay waits for play(). */
    ASSERT_TRUE(wait_for_state(Replay::StereoReplayEngineDLL::PlaybackState::PAUSED) );

    EXPECT_EQ(controller_ptr->get_status().n_calls_executed,
              static_cast<uint64_t>(N_CALLS_PER_FRAME + 1) );
    EXPECT_EQ(controller_ptr->get_status().n_frames_presented,
              1u);

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Replay::StereoReplayEngineDLL::PlaybackState::FINISHED) );

    EXPECT_EQ(controller_ptr->get_status().n_frames_presented,
              N_FRAMES);
}

TEST_F(ReplayEngineTest, SkippedCallsAreNotExecuted)
{
    ASSERT_TRUE(m_engine_ptr->load(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                   nullptr) );

    auto*           controller_ptr = m_engine_ptr->get_playback_controller_ptr();
    TestInterceptor interceptor    (controller_ptr,
                                    false,  /* in_should_pause_on_first_call */
                                    true);  /* in_should_skip_calls          */

    m_engine_ptr->add_call_interceptor(&interceptor);

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Replay::StereoReplayEngineDLL::PlaybackState::FINISHED) );

    EXPECT_EQ(controller_ptr->get_status().n_calls_executed,
              0u);
    EXPECT_EQ(controller_ptr->get_status().n_frames_presented,
              N_FRAMES);

    m_engine_ptr->remove_call_interceptor(&interceptor);
}
