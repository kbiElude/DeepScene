/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the playback control of the replay engine: frame-boundary pausing, resetting and interception.
 */
#include <gtest/gtest.h>
#include "tools/Replay/StereoReplayEngineDLL/entrypoints.h"
#include "replay_test_doubles.h"
#include "workload_file_builder.h"
#include <chrono>
#include <thread>

class ReplayEngineTest : public ::testing::Test
{
protected:
    static const uint32_t N_CALLS_PER_FRAME;
    static const uint32_t N_FRAMES;

    void SetUp() override
    {
        std::vector<uint8_t> file_u8_vec;

        Tests::WorkloadFileBuilder::build_frames(&file_u8_vec,
                                                 N_FRAMES,
                                                 N_CALLS_PER_FRAME);
        Tests::WorkloadFileBuilder::write_file  (file_u8_vec);

        m_engine_ptr = Stereo::Replay::EngineDLL::aiCreateReplayEngine();
    }

    void TearDown() override
    {
        Stereo::Replay::EngineDLL::aiDestroyReplayEngine(m_engine_ptr);
        ::remove(Tests::WorkloadFileBuilder::get_filename_ptr() );
    }

    /* Polls: the replay runs on a thread of its own. */
    bool wait_for_state(const Stereo::Replay::EngineDLL::PlaybackState& in_state)
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

    Stereo::Replay::EngineDLL::IReplayEngine* m_engine_ptr = nullptr;
};

const uint32_t ReplayEngineTest::N_CALLS_PER_FRAME = 2;
const uint32_t ReplayEngineTest::N_FRAMES          = 3;

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
              Stereo::Replay::EngineDLL::PlaybackState::IDLE);

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Stereo::Replay::EngineDLL::PlaybackState::FINISHED) );

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
    Tests::PausingInterceptor interceptor    (controller_ptr,
                                    true,   /* in_should_pause_on_first_call */
                                    false); /* in_should_skip_calls          */

    m_engine_ptr->add_call_interceptor(&interceptor);

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Stereo::Replay::EngineDLL::PlaybackState::PAUSED) );

    /* The first frame was replayed in full, and nothing of the second one. */
    EXPECT_EQ(controller_ptr->get_status().n_calls_executed,
              static_cast<uint64_t>(N_CALLS_PER_FRAME + 1) );
    EXPECT_EQ(controller_ptr->get_status().n_frames_presented,
              1u);

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Stereo::Replay::EngineDLL::PlaybackState::FINISHED) );

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

    ASSERT_TRUE(wait_for_state(Stereo::Replay::EngineDLL::PlaybackState::FINISHED) );

    controller_ptr->reset();

    /* The first frame is replayed, and the replay waits for play(). */
    ASSERT_TRUE(wait_for_state(Stereo::Replay::EngineDLL::PlaybackState::PAUSED) );

    EXPECT_EQ(controller_ptr->get_status().n_calls_executed,
              static_cast<uint64_t>(N_CALLS_PER_FRAME + 1) );
    EXPECT_EQ(controller_ptr->get_status().n_frames_presented,
              1u);

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Stereo::Replay::EngineDLL::PlaybackState::FINISHED) );

    EXPECT_EQ(controller_ptr->get_status().n_frames_presented,
              N_FRAMES);
}

TEST_F(ReplayEngineTest, SkippedCallsAreNotExecuted)
{
    ASSERT_TRUE(m_engine_ptr->load(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                   nullptr) );

    auto*           controller_ptr = m_engine_ptr->get_playback_controller_ptr();
    Tests::PausingInterceptor interceptor    (controller_ptr,
                                    false,  /* in_should_pause_on_first_call */
                                    true);  /* in_should_skip_calls          */

    m_engine_ptr->add_call_interceptor(&interceptor);

    controller_ptr->play();

    ASSERT_TRUE(wait_for_state(Stereo::Replay::EngineDLL::PlaybackState::FINISHED) );

    EXPECT_EQ(controller_ptr->get_status().n_calls_executed,
              0u);
    EXPECT_EQ(controller_ptr->get_status().n_frames_presented,
              N_FRAMES);

    m_engine_ptr->remove_call_interceptor(&interceptor);
}
