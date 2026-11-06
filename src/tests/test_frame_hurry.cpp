/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for FrameHurry and PresentSkipper: which frames a seek goes through quickly, and that it presents none of them.
 */
#include <gtest/gtest.h>
#include "frame_hurry.h"
#include "present_skipper.h"

namespace
{
    Capture::APIDumpLoader::DumpedAPICall make_call(const APIInterceptor::APIFunction& in_function)
    {
        Capture::APIDumpLoader::DumpedAPICall result;

        result.func = in_function;

        return result;
    }
}

TEST(FrameHurry, NoFrameIsHurriedThroughUnlessOneIsAskedFor)
{
    const Stereo::Replay::EngineDLL::FrameHurry hurry;

    EXPECT_FALSE(hurry.is_hurrying(0) );
    EXPECT_FALSE(hurry.is_hurrying(1000) );
}

TEST(FrameHurry, TheFramesBeforeTheOneAskedForAreHurriedThroughAndItIsNot)
{
    Stereo::Replay::EngineDLL::FrameHurry hurry;

    hurry.set_until_frame(5);

    EXPECT_TRUE (hurry.is_hurrying(0) );
    EXPECT_TRUE (hurry.is_hurrying(4) );
    EXPECT_FALSE(hurry.is_hurrying(5) );
    EXPECT_FALSE(hurry.is_hurrying(6) );
}

TEST(FrameHurry, ClearingItBringsTheFramesBack)
{
    Stereo::Replay::EngineDLL::FrameHurry hurry;

    hurry.set_until_frame(5);
    hurry.clear();

    EXPECT_FALSE(hurry.is_hurrying(0) );
}

TEST(PresentSkipper, ThePresentOfAHurriedFrameIsLeftOutAndOtherCallsAreNot)
{
    Stereo::Replay::EngineDLL::FrameHurry     hurry;
    Stereo::Replay::EngineDLL::PresentSkipper skipper(&hurry);
    const auto                                flip   = make_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP);
    const auto                                state  = make_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE);

    hurry.set_until_frame(10);

    EXPECT_FALSE(skipper.should_execute_call(flip,
                                             3) );
    EXPECT_TRUE (skipper.should_execute_call(state,
                                             3) );
}

TEST(PresentSkipper, ThePresentOfTheFrameTheSeekGoesToIsMadeAndSoAreThoseOfAReplayWithNoSeek)
{
    Stereo::Replay::EngineDLL::FrameHurry     hurry;
    Stereo::Replay::EngineDLL::PresentSkipper skipper(&hurry);
    const auto                                flip = make_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP);

    EXPECT_TRUE(skipper.should_execute_call(flip,
                                            3) );

    hurry.set_until_frame(10);

    EXPECT_TRUE(skipper.should_execute_call(flip,
                                            10) );
}
