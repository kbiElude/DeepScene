/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the channel which passes images from an application to the program which shows them: what is written is read, the
 * newest image is the one which is read, an image which is being read is never written over, and the images of one session do
 * not reach another.
 */
#include <gtest/gtest.h>
#include "data_fixtures.h"
#include "stereo/transport/frame_channel.h"
#include <atomic>
#include <chrono>
#include <string.h>
#include <thread>

static Stereo::Transport::FrameInfo make_info(const uint64_t& in_frame_index,
                                              const uint32_t& in_width,
                                              const uint32_t& in_height)
{
    Stereo::Transport::FrameInfo result;

    result.frame_index       = in_frame_index;
    result.height            = in_height;
    result.timestamp_seconds = static_cast<double>(in_frame_index) * 0.5;
    result.width             = in_width;

    return result;
}

/* Writes an image of which every byte is the value. */
static bool write_frame(Stereo::Transport::FrameChannel* inout_channel_ptr,
                        const uint64_t&                  in_frame_index,
                        const uint32_t&                  in_width,
                        const uint32_t&                  in_height,
                        const uint8_t&                   in_value)
{
    uint8_t* const pixel_ptr = inout_channel_ptr->begin_write_ptr(make_info(in_frame_index,
                                                                            in_width,
                                                                            in_height) );

    if (pixel_ptr != nullptr)
    {
        ::memset(pixel_ptr,
                 in_value,
                 static_cast<size_t>(in_width) * in_height * 4);

        inout_channel_ptr->end_write();
    }

    return (pixel_ptr != nullptr);
}

TEST(FrameChannelTest, AnImageWhichIsWrittenIsRead)
{
    std::string                  error;
    const std::string            session      = Tests::DataFixtures::make_session_id("roundtrip");
    const auto                   consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                                          &error);
    const auto                   producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                                          &error);
    Stereo::Transport::FrameInfo info;
    const uint8_t*               pixel_ptr    = nullptr;

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    EXPECT_FALSE(consumer_ptr->begin_read(&info,
                                          &pixel_ptr) );

    uint8_t* const write_ptr = producer_ptr->begin_write_ptr(make_info(7,
                                                                       4,
                                                                       2) );

    ASSERT_NE(write_ptr,
              nullptr);

    for (uint32_t n_byte = 0;
                  n_byte < 4 * 2 * 4;
                ++n_byte)
    {
        write_ptr[n_byte] = static_cast<uint8_t>(n_byte);
    }

    /* Not seen before it is finished. */
    EXPECT_FALSE(consumer_ptr->begin_read(&info,
                                          &pixel_ptr) );

    producer_ptr->end_write();

    ASSERT_TRUE(consumer_ptr->begin_read(&info,
                                         &pixel_ptr) );

    EXPECT_EQ(info.frame_index,
              7u);
    EXPECT_EQ(info.timestamp_seconds,
              3.5);
    EXPECT_EQ(info.height,
              2u);
    EXPECT_EQ(info.width,
              4u);

    for (uint32_t n_byte = 0;
                  n_byte < 4 * 2 * 4;
                ++n_byte)
    {
        EXPECT_EQ(pixel_ptr[n_byte],
                  static_cast<uint8_t>(n_byte) );
    }

    consumer_ptr->end_read();

    EXPECT_EQ(producer_ptr->get_n_frames_published(),
              1u);
}

TEST(FrameChannelTest, TheNewestImageIsTheOneWhichIsRead)
{
    std::string                  error;
    const std::string            session      = Tests::DataFixtures::make_session_id("newest");
    const auto                   consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                                          &error);
    const auto                   producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                                          &error);
    Stereo::Transport::FrameInfo info;
    const uint8_t*               pixel_ptr    = nullptr;

    ASSERT_NE(producer_ptr,
              nullptr) << error;

    for (uint32_t n_frame = 0;
                  n_frame < 10;
                ++n_frame)
    {
        ASSERT_TRUE(write_frame(producer_ptr.get(),
                                n_frame,
                                2,
                                2,
                                static_cast<uint8_t>(n_frame + 1) ) );
    }

    ASSERT_TRUE(consumer_ptr->begin_read(&info,
                                         &pixel_ptr) );

    EXPECT_EQ(info.frame_index,
              9u);
    EXPECT_EQ(pixel_ptr[0],
              10u);

    consumer_ptr->end_read();
}

TEST(FrameChannelTest, AnImageWhichIsBeingReadIsNotWrittenOver)
{
    std::string                  error;
    const std::string            session      = Tests::DataFixtures::make_session_id("held");
    const auto                   consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                                          &error);
    const auto                   producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                                          &error);
    Stereo::Transport::FrameInfo info;
    const uint8_t*               pixel_ptr    = nullptr;

    ASSERT_NE(producer_ptr,
              nullptr) << error;

    ASSERT_TRUE(write_frame(producer_ptr.get(),
                            0,
                            8,
                            8,
                            0xA5) );
    ASSERT_TRUE(consumer_ptr->begin_read(&info,
                                         &pixel_ptr) );

    /* The consumer is slow: the producer goes on, and the image which is read stays as it was. */
    for (uint32_t n_frame = 1;
                  n_frame < 20;
                ++n_frame)
    {
        ASSERT_TRUE(write_frame(producer_ptr.get(),
                                n_frame,
                                8,
                                8,
                                static_cast<uint8_t>(n_frame) ) );
    }

    for (uint32_t n_byte = 0;
                  n_byte < 8 * 8 * 4;
                ++n_byte)
    {
        ASSERT_EQ(pixel_ptr[n_byte],
                  0xA5u) << n_byte;
    }

    consumer_ptr->end_read();
}

TEST(FrameChannelTest, AnImageWhichDoesNotFitIsRefused)
{
    std::string error;
    const auto  producer_ptr = Stereo::Transport::FrameChannel::open_ptr(Tests::DataFixtures::make_session_id("big"),
                                                                        &error);

    ASSERT_NE(producer_ptr,
              nullptr) << error;

    EXPECT_EQ(producer_ptr->begin_write_ptr(make_info(0,
                                                      Stereo::Transport::FrameChannel::get_max_width() + 1,
                                                      4) ),
              nullptr);
    EXPECT_EQ(producer_ptr->begin_write_ptr(make_info(0,
                                                      4,
                                                      Stereo::Transport::FrameChannel::get_max_height() + 1) ),
              nullptr);
    EXPECT_EQ(producer_ptr->begin_write_ptr(make_info(0,
                                                      0,
                                                      4) ),
              nullptr);

    /* The largest image does fit. */
    EXPECT_TRUE(write_frame(producer_ptr.get(),
                            0,
                            Stereo::Transport::FrameChannel::get_max_width(),
                            Stereo::Transport::FrameChannel::get_max_height(),
                            1) );
}

TEST(FrameChannelTest, SessionsDoNotShareImages)
{
    std::string                  error;
    const auto                   first_ptr  = Stereo::Transport::FrameChannel::open_ptr(Tests::DataFixtures::make_session_id("first"),
                                                                                       &error);
    const auto                   second_ptr = Stereo::Transport::FrameChannel::open_ptr(Tests::DataFixtures::make_session_id("second"),
                                                                                       &error);
    Stereo::Transport::FrameInfo info;
    const uint8_t*               pixel_ptr  = nullptr;

    ASSERT_NE(first_ptr,
              nullptr) << error;
    ASSERT_NE(second_ptr,
              nullptr) << error;

    ASSERT_TRUE(write_frame(first_ptr.get(),
                            0,
                            2,
                            2,
                            9) );

    EXPECT_FALSE(second_ptr->begin_read(&info,
                                        &pixel_ptr) );
}

TEST(FrameChannelTest, AnImageIsNeverSeenHalfWrittenWhileTheProducerRuns)
{
    std::atomic<bool> is_running(true);
    std::string       error;
    const std::string session      = Tests::DataFixtures::make_session_id("stress");
    const auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                               &error);
    const auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                               &error);
    uint64_t          n_reads      = 0;
    bool              is_torn      = false;

    ASSERT_NE(producer_ptr,
              nullptr) << error;

    /* Every byte of an image is its own index, so an image in which two values meet is one which was written over. */
    std::thread producer([&]()
                         {
                             for (uint64_t n_frame = 0;
                                           is_running;
                                         ++n_frame)
                             {
                                 write_frame(producer_ptr.get(),
                                             n_frame,
                                             64,
                                             64,
                                             static_cast<uint8_t>(n_frame) );
                             }
                         });

    const auto start = std::chrono::steady_clock::now();

    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(300) )
    {
        Stereo::Transport::FrameInfo info;
        const uint8_t*               pixel_ptr = nullptr;

        if (consumer_ptr->begin_read(&info,
                                     &pixel_ptr) )
        {
            for (uint32_t n_byte = 1;
                          n_byte < 64 * 64 * 4;
                        ++n_byte)
            {
                is_torn = is_torn || (pixel_ptr[n_byte] != pixel_ptr[0]);
            }

            is_torn = is_torn || (pixel_ptr[0] != static_cast<uint8_t>(info.frame_index) );

            consumer_ptr->end_read();

            ++n_reads;
        }
    }

    is_running = false;

    producer.join();

    EXPECT_FALSE(is_torn);
    EXPECT_GT(n_reads,
              0u);
}

TEST(FrameChannelTest, TheNumberOfEyesOfAnImageIsRead)
{
    std::string                  error;
    auto                         channel_ptr = Stereo::Transport::FrameChannel::open_ptr(Tests::DataFixtures::make_session_id("eyes"),
                                                                                        &error);
    Stereo::Transport::FrameInfo info        = make_info(7,
                                                         8,
                                                         2);
    Stereo::Transport::FrameInfo read_info;
    const uint8_t*               pixel_ptr   = nullptr;

    ASSERT_NE(channel_ptr,
              nullptr) << error;

    /* Two eyes side by side: the width is of both. */
    info.n_eyes = 2;

    ASSERT_NE(channel_ptr->begin_write_ptr(info),
              nullptr);

    channel_ptr->end_write();

    ASSERT_TRUE(channel_ptr->begin_read(&read_info,
                                        &pixel_ptr) );
    EXPECT_EQ  (read_info.n_eyes,
                2u);
    EXPECT_EQ  (read_info.width,
                8u);

    channel_ptr->end_read();
}

TEST(FrameChannelTest, AnImageWhoseEyesDoNotShareItsWidthIsRefused)
{
    std::string                  error;
    auto                         channel_ptr = Stereo::Transport::FrameChannel::open_ptr(Tests::DataFixtures::make_session_id("badeyes"),
                                                                                        &error);
    Stereo::Transport::FrameInfo info        = make_info(1,
                                                         9,
                                                         2);

    ASSERT_NE(channel_ptr,
              nullptr) << error;

    info.n_eyes = 2;

    EXPECT_EQ(channel_ptr->begin_write_ptr(info),
              nullptr);

    info.n_eyes = 3;
    info.width  = 9;

    EXPECT_EQ(channel_ptr->begin_write_ptr(info),
              nullptr);

    info.n_eyes = 0;

    EXPECT_EQ(channel_ptr->begin_write_ptr(info),
              nullptr);
}

TEST(FrameChannelTest, TheEyeSeparationIsNotThereUntilItIsSetAndThenReachesTheOtherSide)
{
    std::string error;
    const auto  session_id     = Tests::DataFixtures::make_session_id("separation");
    auto        consumer_ptr   = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                          &error);
    auto        producer_ptr   = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                          &error);
    float       eye_separation = 0.0f;

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    EXPECT_FALSE(producer_ptr->get_eye_separation(&eye_separation) );

    consumer_ptr->set_eye_separation(0.125f);

    ASSERT_TRUE(producer_ptr->get_eye_separation(&eye_separation) );
    EXPECT_FLOAT_EQ(eye_separation,
                    0.125f);

    /* It can be said again while images are passed. */
    consumer_ptr->set_eye_separation(2.5f);

    ASSERT_TRUE(producer_ptr->get_eye_separation(&eye_separation) );
    EXPECT_FLOAT_EQ(eye_separation,
                    2.5f);
}

TEST(FrameChannelTest, ARowStartsAtAMultipleOf256BytesAndIsReadBackWithItsPitch)
{
    std::string error;
    auto        channel_ptr = Stereo::Transport::FrameChannel::open_ptr(Tests::DataFixtures::make_session_id("pitch"),
                                                                       &error);

    ASSERT_NE(channel_ptr,
              nullptr) << error;

    /* A row is rounded up to what a GPU takes for a row of a buffer it copies to a texture. */
    EXPECT_EQ(Stereo::Transport::FrameChannel::get_pitch(1),
              256u);
    EXPECT_EQ(Stereo::Transport::FrameChannel::get_pitch(64),
              256u);
    EXPECT_EQ(Stereo::Transport::FrameChannel::get_pitch(65),
              512u);
    EXPECT_EQ(Stereo::Transport::FrameChannel::get_pitch(800),
              3328u);
    EXPECT_EQ(Stereo::Transport::FrameChannel::get_pitch(1600),
              6400u);

    Stereo::Transport::FrameInfo info      = make_info(1,
                                                       800,
                                                       4);
    Stereo::Transport::FrameInfo read_info;
    const uint8_t*               pixel_ptr = nullptr;

    ASSERT_NE(channel_ptr->begin_write_ptr(info),
              nullptr);

    channel_ptr->end_write();

    ASSERT_TRUE(channel_ptr->begin_read(&read_info,
                                        &pixel_ptr) );
    EXPECT_EQ(read_info.pitch,
              3328u);
    EXPECT_EQ(read_info.kind,
              Stereo::Transport::TransportKind::HOST_MEMORY);
    EXPECT_EQ(read_info.pixel_format,
              Stereo::Transport::PixelFormat::B8G8R8A8);

    channel_ptr->end_read();
}

TEST(FrameChannelTest, TheImagesStartWhereAGPUPlacesTheBuffersOfAHeap)
{
    std::string error;
    auto        channel_ptr = Stereo::Transport::FrameChannel::open_ptr(Tests::DataFixtures::make_session_id("alignment"),
                                                                       &error);

    ASSERT_NE(channel_ptr,
              nullptr) << error;

    const uint8_t* const first_ptr = channel_ptr->begin_write_ptr(make_info(1,
                                                                            16,
                                                                            4) );

    channel_ptr->end_write();

    const uint8_t* const second_ptr = channel_ptr->begin_write_ptr(make_info(2,
                                                                             16,
                                                                             4) );

    ASSERT_NE(first_ptr,
              nullptr);
    ASSERT_NE(second_ptr,
              nullptr);
    ASSERT_NE(first_ptr,
              second_ptr);

    /* Slots are a multiple of 64 KB apart, and the first one is 64 KB into the memory, which the heap of a GPU starts at. */
    EXPECT_EQ((second_ptr > first_ptr ? second_ptr - first_ptr : first_ptr - second_ptr) % 65536,
              0);
}

TEST(FrameChannelTest, NothingIsAgreedOnUntilBothSidesHaveSaidWhatTheyCan)
{
    constexpr Stereo::Transport::TransportKindMask HOST_MEMORY = static_cast<Stereo::Transport::TransportKindMask>(Stereo::Transport::TransportKind::HOST_MEMORY);

    std::string error;
    const auto  session_id   = Tests::DataFixtures::make_session_id("negotiation");
    auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    EXPECT_EQ(producer_ptr->get_negotiated_kind(),
              Stereo::Transport::TransportKind::NONE);

    producer_ptr->set_offered_kinds(HOST_MEMORY);

    EXPECT_EQ(consumer_ptr->get_offered_kinds(),
              HOST_MEMORY);
    EXPECT_EQ(producer_ptr->get_negotiated_kind(),
              Stereo::Transport::TransportKind::NONE);

    consumer_ptr->set_accepted_kinds(HOST_MEMORY);

    EXPECT_EQ(producer_ptr->get_accepted_kinds(),
              HOST_MEMORY);
    EXPECT_EQ(producer_ptr->get_negotiated_kind(),
              Stereo::Transport::TransportKind::HOST_MEMORY);
    EXPECT_EQ(consumer_ptr->get_negotiated_kind(),
              Stereo::Transport::TransportKind::HOST_MEMORY);
}

TEST(FrameChannelTest, TheBestKindBothSidesHaveIsTheOneAgreedOn)
{
    std::string error;
    auto        channel_ptr = Stereo::Transport::FrameChannel::open_ptr(Tests::DataFixtures::make_session_id("best"),
                                                                       &error);

    ASSERT_NE(channel_ptr,
              nullptr) << error;

    /* A producer which can share the GPU memory (bit 4) and a consumer which can too, or take host memory (bit 1). */
    channel_ptr->set_offered_kinds(1 | 4);
    channel_ptr->set_accepted_kinds(1 | 2 | 4);

    EXPECT_EQ(static_cast<uint32_t>(channel_ptr->get_negotiated_kind() ),
              4u);

    /* A consumer which cannot take that one is passed images by host memory. */
    channel_ptr->set_accepted_kinds(1 | 2);

    EXPECT_EQ(channel_ptr->get_negotiated_kind(),
              Stereo::Transport::TransportKind::HOST_MEMORY);

    /* No kind in common is none. */
    channel_ptr->set_accepted_kinds(2);

    EXPECT_EQ(channel_ptr->get_negotiated_kind(),
              Stereo::Transport::TransportKind::NONE);
}

TEST(FrameChannelTest, TheTargetSizeOfAnEyeIsNotThereUntilItIsSetAndThenReachesTheOtherSide)
{
    std::string error;
    const auto  session_id    = Tests::DataFixtures::make_session_id("scale");
    auto        consumer_ptr  = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                         &error);
    auto        producer_ptr  = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                         &error);
    uint32_t    target_height = 0;
    uint32_t    target_width  = 0;

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    EXPECT_FALSE(producer_ptr->get_target_eye_size(&target_width,
                                                   &target_height) );

    consumer_ptr->set_target_eye_size(2064,
                                      2208);

    ASSERT_TRUE(producer_ptr->get_target_eye_size(&target_width,
                                                  &target_height) );
    EXPECT_EQ(target_width,
              2064u);
    EXPECT_EQ(target_height,
              2208u);

    /* It does not disturb the other settings. */
    float eye_separation = 0.0f;

    EXPECT_FALSE(producer_ptr->get_eye_separation(&eye_separation) );
}

TEST(FrameChannelTest, TheConvergenceDistanceIsNotThereUntilItIsSetAndThenReachesTheOtherSide)
{
    std::string error;
    const auto  session_id   = Tests::DataFixtures::make_session_id("convergence");
    auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    float       distance     = 0.0f;

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    EXPECT_FALSE(producer_ptr->get_convergence_distance(&distance) );

    consumer_ptr->set_convergence_distance(12.5f);

    ASSERT_TRUE(producer_ptr->get_convergence_distance(&distance) );
    EXPECT_FLOAT_EQ(distance,
                    12.5f);

    /* It does not disturb the other settings. */
    float eye_separation = 0.0f;

    EXPECT_FALSE(producer_ptr->get_eye_separation(&eye_separation) );
}

TEST(FrameChannelTest, TheStrengthOfThe3DIsNotThereUntilItIsSetAndThenReachesTheOtherSide)
{
    std::string error;
    const auto  session_id   = Tests::DataFixtures::make_session_id("strength");
    auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    float       strength     = 0.0f;

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    EXPECT_FALSE(producer_ptr->get_depth_strength(&strength) );

    consumer_ptr->set_depth_strength(1.5f);

    ASSERT_TRUE(producer_ptr->get_depth_strength(&strength) );
    EXPECT_FLOAT_EQ(strength,
                    1.5f);

    /* It does not disturb the other settings. */
    float eye_separation = 0.0f;

    EXPECT_FALSE(producer_ptr->get_eye_separation(&eye_separation) );
}

TEST(FrameChannelTest, AProducerDoesNotWaitForAConsumerThatHasNotSignalled)
{
    std::string       error;
    const std::string session      = Tests::DataFixtures::make_session_id("pace_none");
    const auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                              &error);
    const auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                              &error);

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    const auto start = std::chrono::steady_clock::now();

    EXPECT_FALSE(producer_ptr->wait_for_pace(1000) );

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count(),
              500);
}

TEST(FrameChannelTest, AProducerWaitsForTheSignalOfTheConsumerAndThenForItsDelay)
{
    std::string       error;
    const std::string session      = Tests::DataFixtures::make_session_id("pace_wait");
    const auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                              &error);
    const auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                              &error);

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    /* A signal tells the producer that the consumer is there; the first one is taken at once, and with no delay there is nothing to wait for. */
    consumer_ptr->signal_pace();

    EXPECT_FALSE(producer_ptr->wait_for_pace(1000) );

    consumer_ptr->set_pace_delay_us(30000);

    std::thread signal_thread([&consumer_ptr]()
                              {
                                  std::this_thread::sleep_for(std::chrono::milliseconds(40) );

                                  consumer_ptr->signal_pace();
                              });

    const auto start = std::chrono::steady_clock::now();

    EXPECT_TRUE(producer_ptr->wait_for_pace(1000) );

    const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();

    signal_thread.join();

    /* The signal came after 40 ms, and the delay is 30 ms more. */
    EXPECT_GE(waited,
              65);
    EXPECT_LT(waited,
              400);
}

TEST(FrameChannelTest, TheDelayIsCountedFromTheSignalAndNotFromTheMomentTheProducerGetsIt)
{
    std::string       error;
    const std::string session      = Tests::DataFixtures::make_session_id("pace_late");
    const auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                              &error);
    const auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session,
                                                                              &error);

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    consumer_ptr->set_pace_delay_us(50000);
    consumer_ptr->signal_pace();

    /* The producer is busy for longer than the delay: the moment is past when it gets the signal, and it goes on at once. */
    std::this_thread::sleep_for(std::chrono::milliseconds(80) );

    const auto start = std::chrono::steady_clock::now();

    EXPECT_FALSE(producer_ptr->wait_for_pace(1000) );

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count(),
              30);

    /* The signal is there and the delay has not passed: the producer waits for what is left of it. */
    consumer_ptr->signal_pace();

    const auto delay_start = std::chrono::steady_clock::now();

    EXPECT_TRUE(producer_ptr->wait_for_pace(1000) );

    EXPECT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - delay_start).count(),
              40);

    /* Nothing comes in the time asked: the producer goes on, and says that it did not get a signal. */
    EXPECT_FALSE(producer_ptr->wait_for_pace(30) );
}

TEST(FrameChannelTest, TheScaleOfTheEyesOfThePersonIsNotThereUntilItIsSetAndThenReachesTheOtherSide)
{
    std::string error;
    const auto  session_id   = Tests::DataFixtures::make_session_id("ipd");
    auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    float       ipd_scale    = 0.0f;

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    EXPECT_FALSE(producer_ptr->get_ipd_scale(&ipd_scale) );

    consumer_ptr->set_ipd_scale(1.07f);

    ASSERT_TRUE(producer_ptr->get_ipd_scale(&ipd_scale) );
    EXPECT_FLOAT_EQ(ipd_scale,
                    1.07f);
}

TEST(FrameChannelTest, TheOverlayIsNotPushedUntilTheConsumerSaysSoAndThenItIs)
{
    std::string error;
    const auto  session_id   = Tests::DataFixtures::make_session_id("overlay");
    auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);

    ASSERT_NE(consumer_ptr,
              nullptr) << error;
    ASSERT_NE(producer_ptr,
              nullptr) << error;

    EXPECT_FALSE(producer_ptr->get_is_overlay_pushed() );

    consumer_ptr->set_is_overlay_pushed(true);

    EXPECT_TRUE(producer_ptr->get_is_overlay_pushed() );

    consumer_ptr->set_is_overlay_pushed(false);

    EXPECT_FALSE(producer_ptr->get_is_overlay_pushed() );
}
