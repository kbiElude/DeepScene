/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the choice of the frames that a contact sheet shows.
 */
#include <gtest/gtest.h>
#include "tools/stereo_replay/inspector/contact_sheet_frame_picker.h"

TEST(ContactSheetFramePicker, FramesAreSpreadEvenlyFromTheFirstToTheLast)
{
    const std::vector<uint32_t> expected_frame_vec = {0, 25, 50, 75, 100};

    EXPECT_EQ(Stereo::Replay::Inspector::ContactSheetFramePicker::sample(0,
                                                                         100,
                                                                         5),
              expected_frame_vec);
}

TEST(ContactSheetFramePicker, TheFirstFrameIsTheFirstOfTheRangeNotZero)
{
    const std::vector<uint32_t> expected_frame_vec = {10, 20, 30};

    EXPECT_EQ(Stereo::Replay::Inspector::ContactSheetFramePicker::sample(10,
                                                                         30,
                                                                         3),
              expected_frame_vec);
}

TEST(ContactSheetFramePicker, AskingForOneFrameGivesTheFirst)
{
    const std::vector<uint32_t> expected_frame_vec = {7};

    EXPECT_EQ(Stereo::Replay::Inspector::ContactSheetFramePicker::sample(7,
                                                                         900,
                                                                         1),
              expected_frame_vec);
}

TEST(ContactSheetFramePicker, ARangeWithFewerFramesThanAskedForGivesAllOfThem)
{
    const std::vector<uint32_t> expected_frame_vec = {4, 5, 6};

    EXPECT_EQ(Stereo::Replay::Inspector::ContactSheetFramePicker::sample(4,
                                                                         6,
                                                                         10),
              expected_frame_vec);
}

TEST(ContactSheetFramePicker, FramesAreNeverRepeated)
{
    const auto frame_vec = Stereo::Replay::Inspector::ContactSheetFramePicker::sample(0,
                                                                                      11,
                                                                                      10);

    ASSERT_EQ(frame_vec.size(),
              10u);

    for (size_t n_frame = 1;
                n_frame < frame_vec.size();
              ++n_frame)
    {
        EXPECT_GT(frame_vec.at(n_frame),
                  frame_vec.at(n_frame - 1) );
    }

    EXPECT_EQ(frame_vec.back(),
              11u);
}

TEST(ContactSheetFramePicker, NothingIsAskedForOrTheRangeIsBackwardsGivesNoFrames)
{
    EXPECT_TRUE(Stereo::Replay::Inspector::ContactSheetFramePicker::sample(0,
                                                                           100,
                                                                           0).empty() );
    EXPECT_TRUE(Stereo::Replay::Inspector::ContactSheetFramePicker::sample(5,
                                                                           4,
                                                                           3).empty() );
}
