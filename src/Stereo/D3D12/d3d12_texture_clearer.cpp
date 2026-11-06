/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/D3D12/d3d12_texture_clearer.h"

Stereo::D3D12::D3D12TextureClearer::D3D12TextureClearer(ID3D12Device*       in_device_ptr,
                                                        ID3D12CommandQueue* in_queue_ptr)
    :m_device_ptr(in_device_ptr),
     m_queue_ptr (in_queue_ptr),
     m_waiter    (in_device_ptr,
                  in_queue_ptr)
{
    D3D12_DESCRIPTOR_HEAP_DESC view_heap_desc = {};

    if (FAILED(m_device_ptr->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    IID_PPV_ARGS(m_allocator_ptr.GetAddressOf() ))) )
    {
        m_error = "The command allocator cannot be made";

        goto end;
    }

    if (FAILED(m_device_ptr->CreateCommandList(0, /* in_node_mask */
                                               D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               m_allocator_ptr.Get(),
                                               nullptr, /* in_initial_state_ptr */
                                               IID_PPV_ARGS(m_command_list_ptr.GetAddressOf() ))) )
    {
        m_error = "The command list cannot be made";

        goto end;
    }

    /* A list is made recording; it is closed until the first clear. */
    m_command_list_ptr->Close();

    view_heap_desc.NumDescriptors = 1;
    view_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

    if (FAILED(m_device_ptr->CreateDescriptorHeap(&view_heap_desc,
                                                  IID_PPV_ARGS(m_view_heap_ptr.GetAddressOf() ))) )
    {
        m_error = "The heap for the view of a texture cannot be made";

        goto end;
    }

    if (m_waiter.is_ok(&m_error) == false)
    {
        goto end;
    }

end:
    return;
}

bool Stereo::D3D12::D3D12TextureClearer::clear_to_black(ID3D12Resource*              in_texture_ptr,
                                                        const DXGI_FORMAT&           in_view_format,
                                                        const D3D12_RESOURCE_STATES& in_texture_state,
                                                        std::string*                 out_error_ptr)
{
    constexpr float BLACK_FP32VEC4[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    bool result = false;

    if (m_error.empty() == false)
    {
        *out_error_ptr = m_error;

        goto end;
    }

    if (FAILED(m_allocator_ptr->Reset() ) )
    {
        *out_error_ptr = "The command allocator cannot be reset";

        goto end;
    }

    if (FAILED(m_command_list_ptr->Reset(m_allocator_ptr.Get(),
                                         nullptr) ))
    {
        *out_error_ptr = "The command list cannot be reset";

        goto end;
    }

    {
        D3D12_RENDER_TARGET_VIEW_DESC view_desc   = {};
        D3D12_CPU_DESCRIPTOR_HANDLE   view_handle = m_view_heap_ptr->GetCPUDescriptorHandleForHeapStart();

        view_desc.Format        = in_view_format;
        view_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        m_device_ptr->CreateRenderTargetView(in_texture_ptr,
                                            &view_desc,
                                             view_handle);

        if (in_texture_state != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            D3D12_RESOURCE_BARRIER barrier = {};

            barrier.Transition.pResource   = in_texture_ptr;
            barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateBefore = in_texture_state;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

            m_command_list_ptr->ResourceBarrier(1,
                                               &barrier);
        }

        m_command_list_ptr->ClearRenderTargetView(view_handle,
                                                  BLACK_FP32VEC4,
                                                  0, /* in_n_rects */
                                                  nullptr);

        if (in_texture_state != D3D12_RESOURCE_STATE_RENDER_TARGET)
        {
            D3D12_RESOURCE_BARRIER barrier = {};

            barrier.Transition.pResource   = in_texture_ptr;
            barrier.Transition.StateAfter  = in_texture_state;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

            m_command_list_ptr->ResourceBarrier(1,
                                               &barrier);
        }
    }

    if (FAILED(m_command_list_ptr->Close() ))
    {
        *out_error_ptr = "The command list cannot be closed";

        goto end;
    }

    {
        ID3D12CommandList* const command_list_ptr_array[] = {m_command_list_ptr.Get()};

        m_queue_ptr->ExecuteCommandLists(1,
                                         command_list_ptr_array);
    }

    result = m_waiter.wait(out_error_ptr);

end:
    return result;
}

bool Stereo::D3D12::D3D12TextureClearer::is_ok(std::string* out_error_ptr) const
{
    *out_error_ptr = m_error;

    return m_error.empty();
}
