/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the ledger of the depth of the frames: which frame the depth that is given is of, for a read that is waited for and for one that is not, and how what the passes
 * of a frame say is put together.
 */
#include <gtest/gtest.h>
#include "stereo/core/frame_depth_ledger.h"

namespace
{
    Stereo::Core::DepthRange make_range(const float& in_near,
                                        const float& in_median,
                                        const float& in_far)
    {
        Stereo::Core::DepthRange result;

        result.far_depth    = in_far;
        result.has_depth    = true;
        result.median_depth = in_median;
        result.near_depth   = in_near;

        return result;
    }
}

TEST(FrameDepthLedgerTest, ADepthThatWasWaitedForIsOfTheFrameThatEndedWhenThereIsNoLag)
{
    Stereo::Core::FrameDepthLedger ledger;
    Stereo::Core::DepthRange       range;
    float                          x_scale = 0.0f;

    ledger.add_result_now(make_range(1.0f,
                                     5.0f,
                                     20.0f),
                          1.5f);
    ledger.end_frame(0);

    ASSERT_TRUE(ledger.take_ready(&range,
                                  &x_scale) );

    EXPECT_FLOAT_EQ(range.near_depth,
                    1.0f);
    EXPECT_FLOAT_EQ(range.median_depth,
                    5.0f);
    EXPECT_FLOAT_EQ(range.far_depth,
                    20.0f);
    EXPECT_FLOAT_EQ(x_scale,
                    1.5f);

    /* It is given once. */
    EXPECT_FALSE(ledger.take_ready(&range,
                                   &x_scale) );
}

TEST(FrameDepthLedgerTest, TheDepthOfAFrameIsReadyAFrameLaterWhenTheResultsComeLater)
{
    Stereo::Core::FrameDepthLedger ledger;
    Stereo::Core::DepthRange       range;
    Stereo::Core::PendingDepthRead read;
    float                          x_scale = 0.0f;

    /* Frame 0: a read is started. Nothing of it can be known when it ends, and nothing is ready (there is no frame before it). */
    ledger.add_pending(3,
                       2.0f,
                       -1.0f,
                       -0.5f);

    EXPECT_FALSE(ledger.pop_pending(true,
                                    &read) );

    ledger.end_frame(1);

    EXPECT_FALSE(ledger.take_ready(&range,
                                   &x_scale) );

    /* Frame 1: a read is started, and when the frame ends the one of frame 0 is old enough to be taken. */
    ledger.add_pending(4,
                       2.5f,
                       -1.0f,
                       -0.5f);

    ASSERT_TRUE(ledger.pop_pending(true,
                                   &read) );

    EXPECT_EQ(read.frame,
              0u);
    EXPECT_EQ(read.slot,
              3u);

    ledger.add_result(read,
                      make_range(2.0f,
                                 6.0f,
                                 30.0f) );

    /* The read of frame 1 is of the frame that ends: not taken yet. */
    EXPECT_FALSE(ledger.pop_pending(true,
                                    &read) );

    ledger.end_frame(1);

    ASSERT_TRUE(ledger.take_ready(&range,
                                  &x_scale) );

    EXPECT_FLOAT_EQ(range.near_depth,
                    2.0f);
    EXPECT_FLOAT_EQ(range.far_depth,
                    30.0f);
    EXPECT_FLOAT_EQ(x_scale,
                    2.0f);
    EXPECT_EQ(ledger.get_n_pending(),
              1u);
}

TEST(FrameDepthLedgerTest, ThePassesOfAFrameAreTheFarthestAndTheNearestAndTheMiddleMedianAndTheLastScale)
{
    Stereo::Core::FrameDepthLedger ledger;
    Stereo::Core::DepthRange       range;
    float                          x_scale = 0.0f;

    ledger.add_result_now(make_range(2.0f,
                                     10.0f,
                                     50.0f),
                          1.0f);
    ledger.add_result_now(make_range(1.0f,
                                     30.0f,
                                     40.0f),
                          2.0f);
    ledger.add_result_now(make_range(3.0f,
                                     20.0f,
                                     90.0f),
                          3.0f);
    ledger.end_frame(0);

    ASSERT_TRUE(ledger.take_ready(&range,
                                  &x_scale) );

    EXPECT_FLOAT_EQ(range.near_depth,
                    1.0f);
    EXPECT_FLOAT_EQ(range.far_depth,
                    90.0f);

    /* The medians are 10, 30 and 20: the one in the middle of them is 20. */
    EXPECT_FLOAT_EQ(range.median_depth,
                    20.0f);
    EXPECT_FLOAT_EQ(x_scale,
                    3.0f);
}

TEST(FrameDepthLedgerTest, AResultIsAddedToTheFrameItWasStartedInAndNotToTheOneThatIsBeingMade)
{
    Stereo::Core::FrameDepthLedger ledger;
    Stereo::Core::DepthRange       range;
    Stereo::Core::PendingDepthRead read;
    float                          x_scale = 0.0f;

    ledger.add_pending(0,
                       1.0f,
                       -1.0f,
                       -1.0f);
    ledger.end_frame(1);

    /* Frame 1 has a pass that was read at once, and the result of frame 0 comes in: they are two frames. */
    ledger.add_result_now(make_range(100.0f,
                                     200.0f,
                                     300.0f),
                          9.0f);

    ASSERT_TRUE(ledger.pop_pending(true,
                                   &read) );

    ledger.add_result(read,
                      make_range(1.0f,
                                 2.0f,
                                 3.0f) );
    ledger.end_frame(1);

    ASSERT_TRUE(ledger.take_ready(&range,
                                  &x_scale) );

    EXPECT_FLOAT_EQ(range.near_depth,
                    1.0f);
    EXPECT_FLOAT_EQ(range.far_depth,
                    3.0f);
    EXPECT_FLOAT_EQ(x_scale,
                    1.0f);
}

TEST(FrameDepthLedgerTest, ThePendingReadsComeOutOldestFirstAndTheOnesOfTheFrameThatIsBeingMadeOnlyIfAskedFor)
{
    Stereo::Core::FrameDepthLedger ledger;
    Stereo::Core::PendingDepthRead read;

    ledger.add_pending(10,
                       0.0f,
                       0.0f,
                       0.0f);
    ledger.add_pending(11,
                       0.0f,
                       0.0f,
                       0.0f);
    ledger.end_frame(1);
    ledger.add_pending(12,
                       0.0f,
                       0.0f,
                       0.0f);

    ASSERT_TRUE(ledger.pop_pending(true,
                                   &read) );
    EXPECT_EQ(read.slot,
              10u);
    ASSERT_TRUE(ledger.pop_pending(true,
                                   &read) );
    EXPECT_EQ(read.slot,
              11u);

    /* The third was started in the frame that is being made. */
    EXPECT_FALSE(ledger.pop_pending(true,
                                    &read) );
    ASSERT_TRUE(ledger.pop_pending(false,
                                   &read) );
    EXPECT_EQ(read.slot,
              12u);
    EXPECT_EQ(ledger.get_n_pending(),
              0u);
}

TEST(FrameDepthLedgerTest, AFrameWithNoResultHasNoDepthAndADepthThatWasNotTakenIsDropped)
{
    Stereo::Core::FrameDepthLedger ledger;
    Stereo::Core::DepthRange       range;
    float                          x_scale = 0.0f;

    ledger.end_frame(0);

    EXPECT_FALSE(ledger.take_ready(&range,
                                   &x_scale) );

    ledger.add_result_now(make_range(1.0f,
                                     2.0f,
                                     3.0f),
                          1.0f);
    ledger.end_frame(0);

    /* The next frame ends with nothing: what was ready before it is no longer. */
    ledger.end_frame(0);

    EXPECT_FALSE(ledger.take_ready(&range,
                                   &x_scale) );
}

TEST(FrameDepthLedgerTest, ThePendingReadsThatAreForgottenGiveNothingAndTheResultOfAnOldFrameIsNotKept)
{
    Stereo::Core::FrameDepthLedger ledger;
    Stereo::Core::DepthRange       range;
    Stereo::Core::PendingDepthRead read;
    float                          x_scale = 0.0f;

    ledger.add_pending(1,
                       1.0f,
                       0.0f,
                       0.0f);
    ledger.end_frame(1);

    ledger.add_pending(2,
                       1.0f,
                       0.0f,
                       0.0f);

    ASSERT_TRUE(ledger.pop_pending(true,
                                   &read) );

    ledger.end_frame(1);
    ledger.forget_pending();

    EXPECT_EQ(ledger.get_n_pending(),
              0u);
    EXPECT_FALSE(ledger.pop_pending(false,
                                    &read) );

    /* A result of frame 0 that comes in after frame 0 was given up is not added to anything. */
    ledger.add_result(read,
                      make_range(1.0f,
                                 2.0f,
                                 3.0f) );
    ledger.end_frame(1);

    EXPECT_FALSE(ledger.take_ready(&range,
                                   &x_scale) );
}
