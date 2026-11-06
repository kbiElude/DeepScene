/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_D3D12_D3D12_QUEUE_WAITER_H
#define STEREO_D3D12_D3D12_QUEUE_WAITER_H

#include <stdint.h>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>

namespace Stereo
{
    namespace D3D12
    {
        /* Waits until what a queue has been given is done: a fence of the device, signalled on the queue, and an event to wait on.
         *
         * Not thread-safe: the thread which owns the queue waits.
         */
        class D3D12QueueWaiter
        {
        public:
            /* The device and the queue must outlive the waiter. */
            D3D12QueueWaiter(ID3D12Device*       in_device_ptr,
                             ID3D12CommandQueue* in_queue_ptr);
            ~D3D12QueueWaiter();

            D3D12QueueWaiter           (const D3D12QueueWaiter&) = delete;
            D3D12QueueWaiter& operator=(const D3D12QueueWaiter&) = delete;

            /* False, and the reason, if the waiter could not make what it needs. */
            bool is_ok(std::string* out_error_ptr) const;

            /* Returns when everything given to the queue before the call is done. False, and the reason, if the queue cannot be waited for. */
            bool wait(std::string* out_error_ptr);

        private:
            std::string                         m_error; /* what stopped the waiter from being made; empty if nothing did */
            HANDLE                              m_fence_event;
            Microsoft::WRL::ComPtr<ID3D12Fence> m_fence_ptr;
            uint64_t                            m_fence_value;
            ID3D12CommandQueue*                 m_queue_ptr;
        };
    }
}

#endif /* STEREO_D3D12_D3D12_QUEUE_WAITER_H */
