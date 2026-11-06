/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the pacing of the frames of a replay.
 */
#include <gtest/gtest.h>
#include "frame_pacer.h"
#include <chrono>

constexpr uint64_t QPC_FREQUENCY = 1000; /* a tick is a millisecond */

/* Waits for n_frames frames, gap_ticks apart. Returns the milliseconds that took. */
static double wait_for_frames(Stereo::Replay::EngineDLL::FramePacer* inout_pacer_ptr,
                              const uint32_t&                        in_n_frames,
                              const uint64_t&                        in_gap_ticks)
{
    const auto start = std::chrono::steady_clock::now();

    for (uint32_t n_frame = 0;
                  n_frame < in_n_frames;
                ++n_frame)
    {
        inout_pacer_ptr->wait_for_frame(n_frame * in_gap_ticks,
                                        QPC_FREQUENCY);
    }

    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

TEST(FramePacer, FramesComeAtTheRecordedPace)
{
    Stereo::Replay::EngineDLL::FramePacer pacer;

    /* The first frame does not wait: 10 frames, 9 gaps of 10 ms. */
    EXPECT_GE(wait_for_frames(&pacer,
                              10,
                              10),
              85.0);
}

TEST(FramePacer, TwiceTheSpeedHalvesTheTime)
{
    Stereo::Replay::EngineDLL::FramePacer pacer;

    pacer.set_speed(2.0);

    const double elapsed_ms = wait_for_frames(&pacer,
                                              11,
                                              10);

    EXPECT_GE(elapsed_ms,
              45.0);
    EXPECT_LT(elapsed_ms,
              90.0);
}

TEST(FramePacer, ZeroSpeedDoesNotWait)
{
    Stereo::Replay::EngineDLL::FramePacer pacer;

    pacer.set_speed(0.0);

    EXPECT_LT(wait_for_frames(&pacer,
                              10,
                              1000),
              50.0);
}

TEST(FramePacer, ALongGapIsCutShort)
{
    Stereo::Replay::EngineDLL::FramePacer pacer;

    /* Two frames, 10 seconds apart. */
    EXPECT_LT(wait_for_frames(&pacer,
                              2,
                              10000),
              500.0);
}

TEST(FramePacer, AShortFirstGapDoesNotHoldBackTheFramesAfterIt)
{
    Stereo::Replay::EngineDLL::FramePacer pacer;
    uint64_t                              timestamp_ticks = 0;

    /* The first gaps are short (1 ms), as the start of a capture has them; then the frames come 10 ms apart. */
    for (uint32_t n_frame = 0;
                  n_frame < 3;
                ++n_frame)
    {
        pacer.wait_for_frame(timestamp_ticks,
                             QPC_FREQUENCY);

        timestamp_ticks += 1;
    }

    const auto frames_start = std::chrono::steady_clock::now();

    for (uint32_t n_frame = 0;
                  n_frame < 40;
                ++n_frame)
    {
        pacer.wait_for_frame(timestamp_ticks,
                             QPC_FREQUENCY);

        timestamp_ticks += 10;
    }

    const double elapsed_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frames_start).count();

    /* 40 frames of 10 ms: the first few are held a little, and the rest come at the recorded pace, which a pacer that stays held would not. */
    EXPECT_GE(elapsed_ms,
              300.0);
}
