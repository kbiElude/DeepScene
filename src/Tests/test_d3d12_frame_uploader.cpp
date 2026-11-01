/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the upload of the eyes of a frame to textures of a device, on the GPU of the machine: what a producer wrote to the memory of
 * the channel is what is in the textures. A machine with no GPU which does Direct3D 12 skips them.
 */
#include <gtest/gtest.h>
#include "data_fixtures.h"
#include "Stereo/D3D12/d3d12_frame_uploader.h"
#include "Stereo/Transport/frame_channel.h"
#include "Stereo/Transport/host_memory_importer.h"
#include <vector>
#include <d3d12.h>
#include <wrl/client.h>

class D3D12FrameUploaderTest : public ::testing::Test
{
protected:
    static constexpr uint32_t EYE_HEIGHT = 40;
    static constexpr uint32_t EYE_WIDTH  = 100; /* a row of an eye is 400 bytes, which is not a multiple of the 256 of a row pitch */

    void SetUp() override
    {
        D3D12_COMMAND_QUEUE_DESC queue_desc = {};

        queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        const HRESULT device_result = ::D3D12CreateDevice(nullptr, /* in_adapter_ptr */
                                                          D3D_FEATURE_LEVEL_11_0,
                                                          IID_PPV_ARGS(m_device_ptr.GetAddressOf() ));
        bool          is_ready      = false;

        if (SUCCEEDED(device_result) )
        {
            const HRESULT queue_result = m_device_ptr->CreateCommandQueue(&queue_desc,
                                                                          IID_PPV_ARGS(m_queue_ptr.GetAddressOf() ));

            is_ready = SUCCEEDED(queue_result);
        }

        if (is_ready == false)
        {
            GTEST_SKIP() << "No Direct3D 12 device";
        }
    }

    /* A texture of the size of an eye, which the test reads back from. */
    Microsoft::WRL::ComPtr<ID3D12Resource> create_texture(const DXGI_FORMAT& in_format) const
    {
        D3D12_HEAP_PROPERTIES                  heap_properties = {};
        Microsoft::WRL::ComPtr<ID3D12Resource> result_ptr;
        D3D12_RESOURCE_DESC                    texture_desc    = {};

        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;

        texture_desc.DepthOrArraySize = 1;
        texture_desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture_desc.Format           = in_format;
        texture_desc.Height           = EYE_HEIGHT;
        texture_desc.MipLevels        = 1;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Width            = EYE_WIDTH;

        m_device_ptr->CreateCommittedResource(&heap_properties,
                                              D3D12_HEAP_FLAG_NONE,
                                             &texture_desc,
                                              D3D12_RESOURCE_STATE_COPY_SOURCE,
                                              nullptr, /* in_optimized_clear_value_ptr */
                                              IID_PPV_ARGS(result_ptr.GetAddressOf() ));

        return result_ptr;
    }

    /* The pixels of a texture which is in the state of a copy source, rows of EYE_WIDTH * 4 bytes. */
    std::vector<uint8_t> read_back(ID3D12Resource* in_texture_ptr) const
    {
        constexpr uint32_t BYTES_PER_PIXEL = 4;

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    allocator_ptr;
        D3D12_TEXTURE_COPY_LOCATION                       destination     = {};
        Microsoft::WRL::ComPtr<ID3D12Fence>               fence_ptr;
        D3D12_HEAP_PROPERTIES                             heap_properties = {};
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_ptr;
        const uint32_t                                    pitch           = Stereo::Transport::FrameChannel::get_pitch(EYE_WIDTH);
        Microsoft::WRL::ComPtr<ID3D12Resource>            readback_ptr;
        D3D12_RESOURCE_DESC                               readback_desc   = {};
        std::vector<uint8_t>                              result_vec;
        D3D12_TEXTURE_COPY_LOCATION                       source          = {};

        heap_properties.Type = D3D12_HEAP_TYPE_READBACK;

        readback_desc.DepthOrArraySize = 1;
        readback_desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        readback_desc.Format           = DXGI_FORMAT_UNKNOWN;
        readback_desc.Height           = 1;
        readback_desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        readback_desc.MipLevels        = 1;
        readback_desc.SampleDesc.Count = 1;
        readback_desc.Width            = static_cast<uint64_t>(pitch) * EYE_HEIGHT;

        m_device_ptr->CreateCommittedResource(&heap_properties,
                                              D3D12_HEAP_FLAG_NONE,
                                             &readback_desc,
                                              D3D12_RESOURCE_STATE_COPY_DEST,
                                              nullptr, /* in_optimized_clear_value_ptr */
                                              IID_PPV_ARGS(readback_ptr.GetAddressOf() ));
        m_device_ptr->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                             IID_PPV_ARGS(allocator_ptr.GetAddressOf() ));
        m_device_ptr->CreateCommandList(0, /* in_node_mask */
                                        D3D12_COMMAND_LIST_TYPE_DIRECT,
                                        allocator_ptr.Get(),
                                        nullptr, /* in_initial_state_ptr */
                                        IID_PPV_ARGS(list_ptr.GetAddressOf() ));
        m_device_ptr->CreateFence(0, /* in_initial_value */
                                  D3D12_FENCE_FLAG_NONE,
                                  IID_PPV_ARGS(fence_ptr.GetAddressOf() ));

        destination.pResource                          = readback_ptr.Get();
        destination.PlacedFootprint.Footprint.Depth    = 1;
        destination.PlacedFootprint.Footprint.Format   = in_texture_ptr->GetDesc().Format;
        destination.PlacedFootprint.Footprint.Height   = EYE_HEIGHT;
        destination.PlacedFootprint.Footprint.RowPitch = pitch;
        destination.PlacedFootprint.Footprint.Width    = EYE_WIDTH;
        destination.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

        source.pResource        = in_texture_ptr;
        source.SubresourceIndex = 0;
        source.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

        list_ptr->CopyTextureRegion(&destination,
                                     0, /* in_dst_x */
                                     0, /* in_dst_y */
                                     0, /* in_dst_z */
                                    &source,
                                     nullptr); /* in_src_box_ptr */
        list_ptr->Close();

        {
            ID3D12CommandList* const list_ptr_array[] = {list_ptr.Get()};

            m_queue_ptr->ExecuteCommandLists(1,
                                             list_ptr_array);
        }

        m_queue_ptr->Signal(fence_ptr.Get(),
                            1);

        while (fence_ptr->GetCompletedValue() < 1)
        {
            ::Sleep(1);
        }

        {
            void* data_ptr = nullptr;

            readback_ptr->Map(0, /* in_subresource */
                              nullptr, /* in_read_range_ptr */
                             &data_ptr);

            result_vec.resize(static_cast<size_t>(EYE_WIDTH) * EYE_HEIGHT * BYTES_PER_PIXEL);

            for (uint32_t n_row = 0;
                          n_row < EYE_HEIGHT;
                        ++n_row)
            {
                ::memcpy(result_vec.data() + static_cast<size_t>(n_row) * EYE_WIDTH * BYTES_PER_PIXEL,
                         static_cast<const uint8_t*>(data_ptr) + static_cast<size_t>(n_row) * pitch,
                         static_cast<size_t>(EYE_WIDTH) * BYTES_PER_PIXEL);
            }

            readback_ptr->Unmap(0, /* in_subresource */
                                nullptr); /* in_written_range_ptr */
        }

        return result_vec;
    }

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

    Microsoft::WRL::ComPtr<ID3D12Device>       m_device_ptr;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue_ptr;
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
    Stereo::D3D12::D3D12FrameUploader     uploader(m_device_ptr.Get(),
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
    Stereo::D3D12::D3D12FrameUploader     uploader(m_device_ptr.Get(),
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
    Stereo::D3D12::D3D12FrameUploader     uploader(m_device_ptr.Get(),
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
