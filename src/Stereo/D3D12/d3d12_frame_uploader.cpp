/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/D3D12/d3d12_frame_uploader.h"

Stereo::D3D12::D3D12FrameUploader::D3D12FrameUploader(ID3D12Device*       in_device_ptr,
                                                      ID3D12CommandQueue* in_queue_ptr)
    :m_runner(in_device_ptr,
              in_queue_ptr)
{
    if (FAILED(in_device_ptr->QueryInterface(IID_PPV_ARGS(m_device_ptr.GetAddressOf() ))) )
    {
        m_error = "The device cannot open memory as a heap (it is not a device of the interface 3 or later)";

        goto end;
    }

    if (m_runner.is_ok(&m_error) == false)
    {
        goto end;
    }

end:
    return;
}

Stereo::D3D12::D3D12FrameUploader::Region* Stereo::D3D12::D3D12FrameUploader::get_region_ptr(const uint8_t* in_pixel_ptr,
                                                                                             uint64_t*      out_offset_ptr,
                                                                                             std::string*   out_error_ptr)
{
    MEMORY_BASIC_INFORMATION memory = {};
    Region*                  result_ptr = nullptr;

    if (::VirtualQuery(in_pixel_ptr,
                      &memory,
                       sizeof(memory) ) == 0)
    {
        *out_error_ptr = "The memory of the images cannot be asked about";

        goto end;
    }

    *out_offset_ptr = static_cast<uint64_t>(in_pixel_ptr - static_cast<const uint8_t*>(memory.AllocationBase) );

    {
        const auto region_iterator = m_region_map.find(memory.AllocationBase);

        if (region_iterator != m_region_map.end() )
        {
            result_ptr = &region_iterator->second;

            goto end;
        }
    }

    {
        D3D12_HEAP_DESC     heap_desc   = {};
        D3D12_RESOURCE_DESC buffer_desc = {};
        Region              region;

        if (FAILED(m_device_ptr->OpenExistingHeapFromAddress(memory.AllocationBase,
                                                             IID_PPV_ARGS(region.heap_ptr.GetAddressOf() ))) )
        {
            *out_error_ptr = "The memory of the images cannot be opened as a heap of the device (it must be committed memory which "
                             "is mapped from a page, with a size which is a multiple of 64 KB)";

            goto end;
        }

        heap_desc = region.heap_ptr->GetDesc();

        buffer_desc.Alignment        = 0;
        buffer_desc.DepthOrArraySize = 1;
        buffer_desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer_desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER; /* what the memory of another process or adapter needs */
        buffer_desc.Format           = DXGI_FORMAT_UNKNOWN;
        buffer_desc.Height           = 1;
        buffer_desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        buffer_desc.MipLevels        = 1;
        buffer_desc.SampleDesc.Count = 1;
        buffer_desc.Width            = heap_desc.SizeInBytes;

        if (FAILED(m_device_ptr->CreatePlacedResource(region.heap_ptr.Get(),
                                                      0, /* in_heap_offset */
                                                     &buffer_desc,
                                                      D3D12_RESOURCE_STATE_GENERIC_READ,
                                                      nullptr, /* in_optimized_clear_value_ptr */
                                                      IID_PPV_ARGS(region.buffer_ptr.GetAddressOf() ))) )
        {
            *out_error_ptr = "The buffer over the memory of the images cannot be made";

            goto end;
        }

        result_ptr = &(m_region_map[memory.AllocationBase] = region);
    }

end:
    return result_ptr;
}

bool Stereo::D3D12::D3D12FrameUploader::is_ok(std::string* out_error_ptr) const
{
    *out_error_ptr = m_error;

    return m_error.empty();
}

bool Stereo::D3D12::D3D12FrameUploader::upload(const Stereo::Transport::ImportedFrame& in_frame,
                                               ID3D12Resource*                         in_left_texture_ptr,
                                               ID3D12Resource*                         in_right_texture_ptr,
                                               const D3D12_RESOURCE_STATES&            in_texture_state,
                                               std::string*                            out_error_ptr)
{
    constexpr uint32_t N_EYES = 2;

    const uint32_t             eye_width           = (in_frame.info.n_eyes > 1) ? in_frame.info.width / in_frame.info.n_eyes
                                                                                : in_frame.info.width;
    ID3D12GraphicsCommandList* list_ptr            = nullptr;
    uint64_t                   pixel_offset        = 0;
    Region*                    region_ptr          = nullptr;
    bool                       result              = false;
    ID3D12Resource* const      texture_ptr_array[] = {in_left_texture_ptr, in_right_texture_ptr};

    if (m_error.empty() == false)
    {
        *out_error_ptr = m_error;

        goto end;
    }

    if (in_frame.pixel_ptr == nullptr)
    {
        *out_error_ptr = "The frame has no pixels in memory";

        goto end;
    }

    region_ptr = get_region_ptr(in_frame.pixel_ptr,
                               &pixel_offset,
                                out_error_ptr);

    if (region_ptr == nullptr)
    {
        goto end;
    }

    list_ptr = m_runner.begin_list_ptr(out_error_ptr);

    if (list_ptr == nullptr)
    {
        goto end;
    }

    for (uint32_t n_eye = 0;
                  n_eye < N_EYES;
                ++n_eye)
    {
        D3D12_TEXTURE_COPY_LOCATION destination  = {};
        const uint32_t              source_eye   = (in_frame.info.n_eyes > 1) ? n_eye
                                                                              : 0;
        D3D12_BOX                   source_box   = {};
        D3D12_TEXTURE_COPY_LOCATION source       = {};
        const D3D12_RESOURCE_DESC   texture_desc = texture_ptr_array[n_eye]->GetDesc();

        if (texture_desc.Width  < eye_width         ||
            texture_desc.Height < in_frame.info.height)
        {
            *out_error_ptr = "A texture is smaller than an eye of the frame";

            m_runner.abandon_list();

            goto end;
        }

        source.pResource                          = region_ptr->buffer_ptr.Get();
        source.PlacedFootprint.Footprint.Depth    = 1;
        source.PlacedFootprint.Footprint.Format   = texture_desc.Format;
        source.PlacedFootprint.Footprint.Height   = in_frame.info.height;
        source.PlacedFootprint.Footprint.RowPitch = in_frame.info.pitch;
        source.PlacedFootprint.Footprint.Width    = in_frame.info.width;
        source.PlacedFootprint.Offset             = pixel_offset;
        source.Type                               = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

        destination.pResource        = texture_ptr_array[n_eye];
        destination.SubresourceIndex = 0;
        destination.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

        source_box.back   = 1;
        source_box.bottom = in_frame.info.height;
        source_box.front  = 0;
        source_box.left   = source_eye * eye_width;
        source_box.right  = source_box.left + eye_width;
        source_box.top    = 0;

        if (in_texture_state != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier = {};

            barrier.Transition.pResource   = texture_ptr_array[n_eye];
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_COPY_DEST;
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
                                              &source_box);

        if (in_texture_state != D3D12_RESOURCE_STATE_COPY_DEST)
        {
            D3D12_RESOURCE_BARRIER barrier = {};

            barrier.Transition.pResource   = texture_ptr_array[n_eye];
            barrier.Transition.StateAfter  = in_texture_state;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

            list_ptr->ResourceBarrier(1,
                                               &barrier);
        }
    }

    result = m_runner.finish_list(out_error_ptr);

end:
    return result;
}
