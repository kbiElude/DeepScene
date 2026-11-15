/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_D3D12_D3D12_COMMAND_RUNNER_H
#define UTILS_D3D12_D3D12_COMMAND_RUNNER_H

#include <stdint.h>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>

namespace Utils
{
    namespace D3D12
    {
        /* Records a command list, has a queue execute it, and waits until it has finished. It owns the allocator, the list, and a fence with an event
         * to wait on. Everything that copies to or clears textures on the GPU needs these.
         *
         * Not thread-safe: only the thread that owns the queue may use it.
         */
        class D3D12CommandRunner
        {
        public:
            /* The device and the queue (a direct queue) must outlive the runner. */
            D3D12CommandRunner(ID3D12Device*       in_device_ptr,
                               ID3D12CommandQueue* in_queue_ptr);
            ~D3D12CommandRunner();

            D3D12CommandRunner           (const D3D12CommandRunner&) = delete;
            D3D12CommandRunner& operator=(const D3D12CommandRunner&) = delete;

            /* Discards the list from begin_list_ptr() without running it. */
            void abandon_list();

            /* Returns an empty command list to record into, or nullptr and sets the error if none is available. Every list must be ended with
             * abandon_list() or finish_list().
             */
            ID3D12GraphicsCommandList* begin_list_ptr(std::string* out_error_ptr);

            /* Has the queue execute the list and returns when it has finished. Returns false and sets the error on failure. */
            bool finish_list(std::string* out_error_ptr);

            /* Returns false and sets the error if the runner could not create the objects it needs. */
            bool is_ok(std::string* out_error_ptr) const;

        private:
            /* Returns when everything submitted to the queue before the call has finished. */
            bool wait(std::string* out_error_ptr);


            Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_allocator_ptr;
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_command_list_ptr;
            std::string                                       m_error; /* why construction failed; empty on success */
            HANDLE                                            m_fence_event;
            Microsoft::WRL::ComPtr<ID3D12Fence>               m_fence_ptr;
            uint64_t                                          m_fence_value;
            ID3D12CommandQueue*                               m_queue_ptr;
        };
    }
}

#endif /* UTILS_D3D12_D3D12_COMMAND_RUNNER_H */
