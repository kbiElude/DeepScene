/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/D3D12/d3d12_queue_waiter.h"

Stereo::D3D12::D3D12QueueWaiter::D3D12QueueWaiter(ID3D12Device*       in_device_ptr,
                                                  ID3D12CommandQueue* in_queue_ptr)
    :m_fence_event(nullptr),
     m_fence_value(0),
     m_queue_ptr  (in_queue_ptr)
{
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

Stereo::D3D12::D3D12QueueWaiter::~D3D12QueueWaiter()
{
    std::string error;

    /* What the GPU may still be reading is not released before it is done. */
    if (m_fence_event != nullptr)
    {
        wait(&error);

        ::CloseHandle(m_fence_event);
    }
}

bool Stereo::D3D12::D3D12QueueWaiter::is_ok(std::string* out_error_ptr) const
{
    *out_error_ptr = m_error;

    return m_error.empty();
}

bool Stereo::D3D12::D3D12QueueWaiter::wait(std::string* out_error_ptr)
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
