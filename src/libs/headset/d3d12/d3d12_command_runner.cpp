/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "headset/d3d12/d3d12_command_runner.h"

Headset::D3D12::D3D12CommandRunner::D3D12CommandRunner(ID3D12Device*       in_device_ptr,
                                                      ID3D12CommandQueue* in_queue_ptr)
    :m_fence_event(nullptr),
     m_fence_value(0),
     m_queue_ptr  (in_queue_ptr)
{
    if (FAILED(in_device_ptr->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                     IID_PPV_ARGS(m_allocator_ptr.GetAddressOf() ))) )
    {
        m_error = "The command allocator cannot be made";

        goto end;
    }

    if (FAILED(in_device_ptr->CreateCommandList(0, /* in_node_mask */
                                                D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                m_allocator_ptr.Get(),
                                                nullptr, /* in_initial_state_ptr */
                                                IID_PPV_ARGS(m_command_list_ptr.GetAddressOf() ))) )
    {
        m_error = "The command list cannot be made";

        goto end;
    }

    /* A list is made recording; it is closed until the first one is begun. */
    m_command_list_ptr->Close();

    if (FAILED(in_device_ptr->CreateFence(0, /* in_initial_value */
                                          D3D12_FENCE_FLAG_NONE,
                                          IID_PPV_ARGS(m_fence_ptr.GetAddressOf() ))) )
    {
        m_error = "The fence cannot be made";

        goto end;
    }

    m_fence_event = ::CreateEventW(nullptr,
                                   FALSE, /* in_manual_reset */
                                   FALSE, /* in_initial_state */
                                   nullptr);

    if (m_fence_event == nullptr)
    {
        m_error = "The event for the fence cannot be made";
    }

end:
    return;
}

Headset::D3D12::D3D12CommandRunner::~D3D12CommandRunner()
{
    std::string error;

    /* What the GPU may still be reading is not released before it is done. */
    if (m_fence_event != nullptr)
    {
        wait(&error);

        ::CloseHandle(m_fence_event);
    }
}

void Headset::D3D12::D3D12CommandRunner::abandon_list()
{
    m_command_list_ptr->Close();
}

ID3D12GraphicsCommandList* Headset::D3D12::D3D12CommandRunner::begin_list_ptr(std::string* out_error_ptr)
{
    ID3D12GraphicsCommandList* result_ptr = nullptr;

    if (m_error.empty() == false)
    {
        *out_error_ptr = m_error;

        goto end;
    }

    if (FAILED(m_allocator_ptr->Reset() ))
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

    result_ptr = m_command_list_ptr.Get();

end:
    return result_ptr;
}

bool Headset::D3D12::D3D12CommandRunner::finish_list(std::string* out_error_ptr)
{
    bool result = false;

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

    result = wait(out_error_ptr);

end:
    return result;
}

bool Headset::D3D12::D3D12CommandRunner::is_ok(std::string* out_error_ptr) const
{
    *out_error_ptr = m_error;

    return m_error.empty();
}

bool Headset::D3D12::D3D12CommandRunner::wait(std::string* out_error_ptr)
{
    bool result = false;

    m_fence_value++;

    if (FAILED(m_queue_ptr->Signal(m_fence_ptr.Get(),
                                   m_fence_value) ))
    {
        *out_error_ptr = "The queue cannot be signalled";

        goto end;
    }

    if (m_fence_ptr->GetCompletedValue() < m_fence_value)
    {
        if (FAILED(m_fence_ptr->SetEventOnCompletion(m_fence_value,
                                                     m_fence_event) ))
        {
            *out_error_ptr = "The fence cannot be waited for";

            goto end;
        }

        ::WaitForSingleObject(m_fence_event,
                              INFINITE);
    }

    result = true;

end:
    return result;
}
