/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the upload of the eyes of a frame to textures of a device, on the GPU of the machine: what a producer wrote to the memory of
 * the channel is what is in the textures. A machine with no GPU which does Direct3D 12 skips them.
 */
#include <gtest/gtest.h>
#include "d3d12_device_fixture.h"
#include "data_fixtures.h"
#include "Headset/D3D12/d3d12_frame_uploader.h"
#include "Stereo/Transport/frame_channel.h"
#include "Stereo/Transport/host_memory_importer.h"
#include <vector>
#include <d3d12.h>
#include <wrl/client.h>

class D3D12FrameUploaderTest : public D3D12DeviceFixture
{
protected:
    /* What the producer writes for a pixel of an eye: it tells the eye, the column and the row, and the frame. */
    static void get_pixel(const uint32_t& in_eye,
                          const uint32_t& in_x,
                          const uint32_t& in_y,
                          const uint32_t& in_frame,
                          uint8_t*        out_pixel_ptr)
    {
        out_pixel_ptr[0] = static_cast<uint8_t>(in_x);
        out_pixel_ptr[1] = static_cast<uint8_t>(in_y);
        out_pixel_ptr[2] = static_cast<uint8_t>(in_eye * 100 + in_frame);
        out_pixel_ptr[3] = 255;
    }

    /* A frame of the eyes (one or two) as the producer writes it. */
    static void write_frame(Stereo::Transport::FrameChannel* in_channel_ptr,
                            const uint32_t&                in_n_eyes,
                            const uint32_t&                in_frame)
    {
        Stereo::Transport::FrameInfo info;

        info.frame_index = in_frame;
        info.height      = EYE_HEIGHT;
        info.n_eyes      = in_n_eyes;
        info.width       = EYE_WIDTH * in_n_eyes;

        uint8_t* const pixel_ptr = in_channel_ptr->begin_write_ptr(info);
        const uint32_t pitch = Stereo::Transport::FrameChannel::get_pitch(info.width);

        ASSERT_NE(pixel_ptr,
                  nullptr);

        for (uint32_t n_row = 0;
                      n_row < EYE_HEIGHT;
                    ++n_row)
        {
            for (uint32_t n_column = 0;
                          n_column < info.width;
                        ++n_column)
            {
                get_pixel(n_column / EYE_WIDTH,
                          n_column % EYE_WIDTH,
                          n_row,
                          in_frame,
                          pixel_ptr + static_cast<size_t>(n_row) * pitch + static_cast<size_t>(n_column) * 4);
            }
        }

        in_channel_ptr->end_write();
    }

    /* What the eye of the frame has to be in a texture. */
    static std::vector<uint8_t> make_expected(const uint32_t& in_eye,
                                              const uint32_t& in_frame)
    {
        std::vector<uint8_t> result_vec(static_cast<size_t>(EYE_WIDTH) * EYE_HEIGHT * 4);

        for (uint32_t n_row = 0;
                      n_row < EYE_HEIGHT;
                    ++n_row)
        {
            for (uint32_t n_column = 0;
                          n_column < EYE_WIDTH;
                        ++n_column)
            {
                get_pixel(in_eye,
                          n_column,
                          n_row,
                          in_frame,
                          result_vec.data() + (static_cast<size_t>(n_row) * EYE_WIDTH + n_column) * 4);
            }
        }

        return result_vec;
    }
};

TEST_F(D3D12FrameUploaderTest, TheEyesOfAFrameAreInTheTexturesAndTheTexturesAreInTheStateTheyWereIn)
{
    std::string error;
    const auto  session_id   = Tests::DataFixtures::make_session_id("eyes");
    auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);

    ASSERT_NE(producer_ptr,
              nullptr) << error;
    ASSERT_NE(consumer_ptr,
              nullptr) << error;

    Stereo::Transport::HostMemoryImporter importer(std::move(consumer_ptr) );
    Headset::D3D12::D3D12FrameUploader     uploader(m_device_ptr.Get(),
                                                   m_queue_ptr.Get() );
    Stereo::Transport::ImportedFrame      frame;
    auto                                  left_ptr  = create_texture(DXGI_FORMAT_B8G8R8A8_UNORM);
    auto                                  right_ptr = create_texture(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB);

    ASSERT_TRUE(uploader.is_ok(&error) ) << error;

    write_frame(producer_ptr.get(),
                2,
                7);

    ASSERT_TRUE(importer.acquire(&frame) );
    ASSERT_TRUE(uploader.upload(frame,
                                left_ptr.Get(),
                                right_ptr.Get(),
                                D3D12_RESOURCE_STATE_COPY_SOURCE,
                               &error) ) << error;

    importer.release();

    /* The right eye is not the left one, and the column of an eye starts at 0 whichever it is. */
    EXPECT_EQ(read_back(left_ptr.Get() ),
              make_expected(0,
                            7) );
    EXPECT_EQ(read_back(right_ptr.Get() ),
              make_expected(1,
                            7) );
}

TEST_F(D3D12FrameUploaderTest, FramesOfOtherSlotsAndOfOneEyeAreUploadedToo)
{
    std::string error;
    const auto  session_id   = Tests::DataFixtures::make_session_id("slots");
    auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);

    ASSERT_NE(producer_ptr,
              nullptr) << error;
    ASSERT_NE(consumer_ptr,
              nullptr) << error;

    Stereo::Transport::HostMemoryImporter importer(std::move(consumer_ptr) );
    Headset::D3D12::D3D12FrameUploader     uploader(m_device_ptr.Get(),
                                                   m_queue_ptr.Get() );
    Stereo::Transport::ImportedFrame      frame;
    auto                                  left_ptr  = create_texture(DXGI_FORMAT_B8G8R8A8_UNORM);
    auto                                  right_ptr = create_texture(DXGI_FORMAT_B8G8R8A8_UNORM);

    /* Each frame is written to the next slot of the channel, so these are three different pieces of memory, and then the first again. */
    for (uint32_t n_frame = 1;
                  n_frame <= 4;
                ++n_frame)
    {
        const uint32_t n_eyes = (n_frame == 3) ? 1 : 2;

        write_frame(producer_ptr.get(),
                    n_eyes,
                    n_frame);

        ASSERT_TRUE(importer.acquire(&frame) );
        ASSERT_TRUE(uploader.upload(frame,
                                    left_ptr.Get(),
                                    right_ptr.Get(),
                                    D3D12_RESOURCE_STATE_COPY_SOURCE,
                                   &error) ) << error;

        importer.release();

        EXPECT_EQ(read_back(left_ptr.Get() ),
                  make_expected(0,
                                n_frame) ) << "left, frame " << n_frame;
        EXPECT_EQ(read_back(right_ptr.Get() ),
                  make_expected((n_eyes == 1) ? 0 : 1,
                                n_frame) ) << "right, frame " << n_frame;
    }
}

TEST_F(D3D12FrameUploaderTest, ATextureSmallerThanAnEyeIsRefused)
{
    std::string error;
    const auto  session_id   = Tests::DataFixtures::make_session_id("small");
    auto        producer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);
    auto        consumer_ptr = Stereo::Transport::FrameChannel::open_ptr(session_id,
                                                                        &error);

    ASSERT_NE(producer_ptr,
              nullptr) << error;
    ASSERT_NE(consumer_ptr,
              nullptr) << error;

    Stereo::Transport::HostMemoryImporter importer(std::move(consumer_ptr) );
    Headset::D3D12::D3D12FrameUploader     uploader(m_device_ptr.Get(),
                                                   m_queue_ptr.Get() );
    Stereo::Transport::ImportedFrame      frame;
    auto                                  texture_ptr = create_texture(DXGI_FORMAT_B8G8R8A8_UNORM);

    /* An eye which is wider than the texture. */
    Stereo::Transport::FrameInfo info;

    info.height = EYE_HEIGHT;
    info.n_eyes = 2;
    info.width  = 2 * (EYE_WIDTH + 10);

    ASSERT_NE(producer_ptr->begin_write_ptr(info),
              nullptr);

    producer_ptr->end_write();

    ASSERT_TRUE(importer.acquire(&frame) );

    EXPECT_FALSE(uploader.upload(frame,
                                 texture_ptr.Get(),
                                 texture_ptr.Get(),
                                 D3D12_RESOURCE_STATE_COPY_SOURCE,
                                &error) );
    EXPECT_FALSE(error.empty() );

    importer.release();
}
