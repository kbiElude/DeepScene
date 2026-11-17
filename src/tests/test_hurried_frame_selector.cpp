/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for HurriedFrameSelector and PresentSkipper: which frames a seek goes through quickly, and that it presents none of them.
 */
#include <gtest/gtest.h>
#include "dumped_call_factory.h"
#include "hurried_frame_selector.h"
#include "present_skipper.h"

using Tests::make_call;

TEST(HurriedFrameSelector, NoFrameIsHurriedThroughUnlessOneIsAskedFor)
{
    const Stereo::Replay::EngineDLL::HurriedFrameSelector hurry;

    EXPECT_FALSE(hurry.is_hurrying(0) );
    EXPECT_FALSE(hurry.is_hurrying(1000) );
}

TEST(HurriedFrameSelector, TheFramesBeforeTheOneAskedForAreHurriedThroughAndItIsNot)
{
    Stereo::Replay::EngineDLL::HurriedFrameSelector hurry;

    hurry.set_until_frame(5);

    EXPECT_TRUE (hurry.is_hurrying(0) );
    EXPECT_TRUE (hurry.is_hurrying(4) );
    EXPECT_FALSE(hurry.is_hurrying(5) );
    EXPECT_FALSE(hurry.is_hurrying(6) );
}

TEST(HurriedFrameSelector, ClearingItBringsTheFramesBack)
{
    Stereo::Replay::EngineDLL::HurriedFrameSelector hurry;

    hurry.set_until_frame(5);
    hurry.clear();

    EXPECT_FALSE(hurry.is_hurrying(0) );
}

TEST(PresentSkipper, ThePresentOfAHurriedFrameIsLeftOutAndOtherCallsAreNot)
{
    Stereo::Replay::EngineDLL::HurriedFrameSelector hurry;
    Stereo::Replay::EngineDLL::PresentSkipper       skipper(&hurry);
    const auto                                      flip   = make_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP);
    const auto                                      state  = make_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE);

    hurry.set_until_frame(10);

    EXPECT_FALSE(skipper.should_execute_call(flip,
                                             3) );
    EXPECT_TRUE (skipper.should_execute_call(state,
                                             3) );
}

TEST(PresentSkipper, ThePresentOfTheFrameTheSeekGoesToIsMadeAndSoAreThoseOfAReplayWithNoSeek)
{
    Stereo::Replay::EngineDLL::HurriedFrameSelector hurry;
    Stereo::Replay::EngineDLL::PresentSkipper       skipper(&hurry);
    const auto                                      flip = make_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP);

    EXPECT_TRUE(skipper.should_execute_call(flip,
                                            3) );

    hurry.set_until_frame(10);

    EXPECT_TRUE(skipper.should_execute_call(flip,
                                            10) );
}
