/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for EyeImageBuffer: the two eyes of a frame come out together, and the user is woken up once for the images that wait.
 */
#include <gtest/gtest.h>
#include "eye_image_buffer.h"

constexpr uint32_t HEIGHT = 2;
constexpr uint32_t WIDTH  = 3;

/* Every byte of the image is the value. */
static bool submit(Replay::StereoReplayPlayer::EyeImageBuffer* inout_buffer_ptr,
                         const uint32_t&                        in_n_execution,
                         const uint8_t&                         in_value)
{
    const std::vector<uint8_t>         pixel_vec(WIDTH * HEIGHT * sizeof(uint32_t),
                                                 in_value);
    Replay::StereoReplayEngineDLL::FrameImage        image;

    image.height    = HEIGHT;
    image.pixel_ptr = pixel_vec.data();
    image.width     = WIDTH;

    return inout_buffer_ptr->submit(in_n_execution,
                                    image);
}

class EyeImageBufferTest : public ::testing::Test
{
protected:
    bool take()
    {
        return m_buffer.take(&m_image_array,
                             &m_is_new_array);
    }

    Replay::StereoReplayPlayer::EyeImageBuffer                                           m_buffer = Replay::StereoReplayPlayer::EyeImageBuffer(Replay::StereoReplayPlayer::EyeMode::BOTH);
    std::array<Replay::StereoReplayPlayer::EyeImageBuffer::Image, Replay::StereoReplayPlayer::EyeImageBuffer::N_EYES> m_image_array;
    std::array<bool, Replay::StereoReplayPlayer::EyeImageBuffer::N_EYES>                 m_is_new_array = {};
};

TEST_F(EyeImageBufferTest, TheLeftImageWaitsForTheRightOne)
{
    EXPECT_FALSE(submit(&m_buffer,
                        0,
                        10) );
    EXPECT_FALSE(take() );

    EXPECT_TRUE(submit(&m_buffer,
                       1,
                       20) );
    ASSERT_TRUE(take() );

    EXPECT_TRUE(m_is_new_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_LEFT) );
    EXPECT_TRUE(m_is_new_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_RIGHT) );
    EXPECT_EQ(m_image_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_LEFT).pixel_u8_vec.front(),
                    10);
    EXPECT_EQ(m_image_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_RIGHT).pixel_u8_vec.front(),
                    20);
    EXPECT_EQ(m_image_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_LEFT).width,
                    WIDTH);
    EXPECT_EQ(m_image_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_LEFT).height,
                    HEIGHT);
}

TEST_F(EyeImageBufferTest, TheLeftImageOfTheNextFrameNeverComesOutWithTheRightImageOfTheOneBefore)
{
    submit(&m_buffer, 0, 10);
    submit(&m_buffer, 1, 20);

    ASSERT_TRUE(take() );

    submit(&m_buffer, 0, 30);

    EXPECT_FALSE(take() );

    submit(&m_buffer, 1, 40);

    ASSERT_TRUE(take() );
    EXPECT_EQ(m_image_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_LEFT).pixel_u8_vec.front(),
                    30);
    EXPECT_EQ(m_image_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_RIGHT).pixel_u8_vec.front(),
                    40);
}

TEST_F(EyeImageBufferTest, FramesWhichWereNotTakenAreReplacedByTheLastOne)
{
    submit(&m_buffer, 0, 10);
    submit(&m_buffer, 1, 20);
    submit(&m_buffer, 0, 30);
    submit(&m_buffer, 1, 40);

    ASSERT_TRUE(take() );
    EXPECT_EQ(m_image_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_LEFT).pixel_u8_vec.front(),
                    30);
    EXPECT_EQ(m_image_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_RIGHT).pixel_u8_vec.front(),
                    40);
    EXPECT_FALSE(take() );
}

TEST_F(EyeImageBufferTest, TheUserIsWokenUpOnceForTheImagesWhichWait)
{
    submit(&m_buffer, 0, 10);

    EXPECT_TRUE(submit(&m_buffer,
                       1,
                       20) );

    submit(&m_buffer, 0, 30);

    EXPECT_FALSE(submit(&m_buffer,
                        1,
                        40) );

    ASSERT_TRUE(take() );

    submit(&m_buffer, 0, 50);

    EXPECT_TRUE(submit(&m_buffer,
                       1,
                       60) );
}

TEST_F(EyeImageBufferTest, ARequestToWakeUpTheUserWhichFailedIsMadeAgain)
{
    submit(&m_buffer, 0, 10);

    ASSERT_TRUE(submit(&m_buffer,
                       1,
                       20) );

    m_buffer.on_post_failed();

    submit(&m_buffer, 0, 30);

    EXPECT_TRUE(submit(&m_buffer,
                       1,
                       40) );
}

TEST_F(EyeImageBufferTest, OneEyeIsShownFromTheFirstExecution)
{
    Replay::StereoReplayPlayer::EyeImageBuffer right_buffer(Replay::StereoReplayPlayer::EyeMode::RIGHT);

    EXPECT_TRUE(submit(&right_buffer,
                       0,
                       70) );
    ASSERT_TRUE(right_buffer.take(&m_image_array,
                                  &m_is_new_array) );

    EXPECT_FALSE(m_is_new_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_LEFT) );
    EXPECT_TRUE(m_is_new_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_RIGHT) );
    EXPECT_EQ(m_image_array.at(Replay::StereoReplayPlayer::EyeImageBuffer::EYE_RIGHT).pixel_u8_vec.front(),
                    70);
}

TEST_F(EyeImageBufferTest, AnExecutionOfNoEyeIsIgnored)
{
    EXPECT_FALSE(submit(&m_buffer,
                        2,
                        10) );
    EXPECT_FALSE(take() );
}
