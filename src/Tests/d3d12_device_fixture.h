/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The base of the tests which use a Direct3D 12 device, on the GPU of the machine: a device and a queue, textures of the size of an eye, and a read back
 * of what is in them. A machine with no GPU which does Direct3D 12 skips them.
 */
#ifndef TESTS_D3D12_DEVICE_FIXTURE_H
#define TESTS_D3D12_DEVICE_FIXTURE_H

#include <gtest/gtest.h>
#include "Stereo/Transport/frame_channel.h"
#include <string.h>
#include <vector>
#include <d3d12.h>
#include <wrl/client.h>

class D3D12DeviceFixture : public ::testing::Test
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

    /* A texture of the size of an eye, which the test reads back from: it is in the state of a copy source, unless another is said. */
    Microsoft::WRL::ComPtr<ID3D12Resource> create_texture(const DXGI_FORMAT&           in_format,
                                                          const D3D12_RESOURCE_FLAGS&  in_flags = D3D12_RESOURCE_FLAG_NONE,
                                                          const D3D12_RESOURCE_STATES& in_state = D3D12_RESOURCE_STATE_COPY_SOURCE) const
    {
        D3D12_HEAP_PROPERTIES                  heap_properties = {};
        Microsoft::WRL::ComPtr<ID3D12Resource> result_ptr;
        D3D12_RESOURCE_DESC                    texture_desc    = {};

        heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;

        texture_desc.DepthOrArraySize = 1;
        texture_desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture_desc.Flags            = in_flags;
        texture_desc.Format           = in_format;
        texture_desc.Height           = EYE_HEIGHT;
        texture_desc.MipLevels        = 1;
        texture_desc.SampleDesc.Count = 1;
        texture_desc.Width            = EYE_WIDTH;

        m_device_ptr->CreateCommittedResource(&heap_properties,
                                              D3D12_HEAP_FLAG_NONE,
                                             &texture_desc,
                                              in_state,
                                              nullptr, /* in_optimized_clear_value_ptr */
                                              IID_PPV_ARGS(result_ptr.GetAddressOf() ));

        return result_ptr;
    }

    /* The pixels of a texture which is in the state of a copy source, rows of EYE_WIDTH * 4 bytes. */
    std::vector<uint8_t> read_back(ID3D12Resource*              in_texture_ptr,
                                   const D3D12_RESOURCE_STATES& in_texture_state = D3D12_RESOURCE_STATE_COPY_SOURCE) const
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

        if (in_texture_state != D3D12_RESOURCE_STATE_COPY_SOURCE)
        {
            D3D12_RESOURCE_BARRIER barrier = {};

            barrier.Transition.pResource   = in_texture_ptr;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barrier.Transition.StateBefore = in_texture_state;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

            list_ptr->ResourceBarrier(1,
                                     &barrier);
        }

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

    Microsoft::WRL::ComPtr<ID3D12Device>       m_device_ptr;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue_ptr;
};

#endif /* TESTS_D3D12_DEVICE_FIXTURE_H */
