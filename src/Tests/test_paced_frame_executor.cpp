/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for PacedFrameExecutor: which frames a replay that seeks hurries through.
 */
#include <gtest/gtest.h>
#include "paced_frame_executor.h"

TEST(PacedFrameExecutor, NoFrameIsHurriedThroughUnlessOneIsAskedFor)
{
    const Stereo::Replay::EngineDLL::PacedFrameExecutor executor(nullptr);

    EXPECT_FALSE(executor.is_hurrying(0) );
    EXPECT_FALSE(executor.is_hurrying(1000) );
}

TEST(PacedFrameExecutor, TheFramesUpToTheOneAskedForAreHurriedThrough)
{
    Stereo::Replay::EngineDLL::PacedFrameExecutor executor(nullptr);

    executor.set_unpaced_frame(5);

    EXPECT_TRUE (executor.is_hurrying(0) );
    EXPECT_TRUE (executor.is_hurrying(5) );
    EXPECT_FALSE(executor.is_hurrying(6) );
}

TEST(PacedFrameExecutor, ClearingTheFrameBringsTheSpeedBack)
{
    Stereo::Replay::EngineDLL::PacedFrameExecutor executor(nullptr);

    executor.set_unpaced_frame(5);
    executor.clear_unpaced_frame();

    EXPECT_FALSE(executor.is_hurrying(0) );
    EXPECT_FALSE(executor.is_hurrying(5) );
}
