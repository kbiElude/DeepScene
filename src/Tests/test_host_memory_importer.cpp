/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the importer of the host memory kind: it says what it takes, and hands over the frames which a producer writes.
 */
#include <gtest/gtest.h>
#include "data_fixtures.h"
#include "Stereo/StereoTransport/frame_channel.h"
#include "Stereo/StereoTransport/host_memory_importer.h"
#include <string.h>

TEST(HostMemoryImporterTest, ItSaysOnTheChannelThatItTakesHostMemory)
{
    std::string error;
    const auto  session_id   = Tests::DataFixtures::make_session_id("accepts");
    auto        producer_ptr = Stereo::StereoTransport::FrameChannel::open_ptr(session_id,
                                                                              &error);
    auto        consumer_ptr = Stereo::StereoTransport::FrameChannel::open_ptr(session_id,
                                                                              &error);

    ASSERT_NE(producer_ptr,
              nullptr) << error;
    ASSERT_NE(consumer_ptr,
              nullptr) << error;

    producer_ptr->set_offered_kinds(static_cast<Stereo::StereoTransport::TransportKindMask>(Stereo::StereoTransport::TransportKind::HOST_MEMORY) );

    EXPECT_EQ(producer_ptr->get_negotiated_kind(),
              Stereo::StereoTransport::TransportKind::NONE);

    const Stereo::StereoTransport::HostMemoryImporter importer(std::move(consumer_ptr) );

    EXPECT_EQ(producer_ptr->get_negotiated_kind(),
              Stereo::StereoTransport::TransportKind::HOST_MEMORY);
}

TEST(HostMemoryImporterTest, TheFramesAProducerWritesAreHandedOverWithWhatTheyAre)
{
    std::string error;
    const auto  session_id   = Tests::DataFixtures::make_session_id("frames");
    auto        producer_ptr = Stereo::StereoTransport::FrameChannel::open_ptr(session_id,
                                                                              &error);
    auto        consumer_ptr = Stereo::StereoTransport::FrameChannel::open_ptr(session_id,
                                                                              &error);

    ASSERT_NE(producer_ptr,
              nullptr) << error;
    ASSERT_NE(consumer_ptr,
              nullptr) << error;

    Stereo::StereoTransport::HostMemoryImporter importer(std::move(consumer_ptr) );
    Stereo::StereoTransport::ImportedFrame      frame;

    /* No frame yet. */
    EXPECT_FALSE(importer.acquire(&frame) );

    Stereo::StereoTransport::FrameInfo info;

    info.frame_index = 42;
    info.height      = 3;
    info.n_eyes      = 2;
    info.width       = 8;

    uint8_t* const write_ptr = producer_ptr->begin_write_ptr(info);

    ASSERT_NE(write_ptr,
              nullptr);

    ::memset(write_ptr,
             0x5a,
             static_cast<size_t>(Stereo::StereoTransport::FrameChannel::get_pitch(info.width) ) * info.height);

    producer_ptr->end_write();

    ASSERT_TRUE(importer.acquire(&frame) );
    EXPECT_EQ  (frame.info.frame_index,
                42u);
    EXPECT_EQ  (frame.info.n_eyes,
                2u);
    EXPECT_EQ  (frame.info.width,
                8u);
    EXPECT_EQ  (frame.info.height,
                3u);
    EXPECT_EQ  (frame.info.pitch,
                Stereo::StereoTransport::FrameChannel::get_pitch(8) );
    EXPECT_EQ  (frame.info.kind,
                Stereo::StereoTransport::TransportKind::HOST_MEMORY);
    ASSERT_NE  (frame.pixel_ptr,
                nullptr);
    EXPECT_EQ  (frame.pixel_ptr[0],
                0x5a);

    importer.release();
}
