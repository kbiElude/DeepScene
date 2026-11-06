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
        /* Records a list of commands, has a queue run it, and waits until it is done: the allocator and the list, and a fence of the device
         * with an event to wait on, which the things that copy to or clear textures on the GPU all need.
         *
         * Not thread-safe: the thread which owns the queue runs.
         */
        class D3D12CommandRunner
        {
        public:
            /* The device and the queue (a direct one) must outlive the runner. */
            D3D12CommandRunner(ID3D12Device*       in_device_ptr,
                               ID3D12CommandQueue* in_queue_ptr);
            ~D3D12CommandRunner();

            D3D12CommandRunner           (const D3D12CommandRunner&) = delete;
            D3D12CommandRunner& operator=(const D3D12CommandRunner&) = delete;

            /* Gives up the list that begin_list_ptr() gave, and nothing is run. */
            void abandon_list();

            /* A list to record the commands in, which is empty. Returns nullptr, and says why, if there is none. Every list is ended by
             * abandon_list() or finish_list().
             */
            ID3D12GraphicsCommandList* begin_list_ptr(std::string* out_error_ptr);

            /* Has the queue run the list, and returns when it is done. False, and the reason, if it cannot be. */
            bool finish_list(std::string* out_error_ptr);

            /* False, and the reason, if the runner could not make what it needs. */
            bool is_ok(std::string* out_error_ptr) const;

        private:
            /* Returns when everything given to the queue before the call is done. */
            bool wait(std::string* out_error_ptr);


            Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_allocator_ptr;
            Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_command_list_ptr;
            std::string                                       m_error; /* what stopped the runner from being made; empty if nothing did */
            HANDLE                                            m_fence_event;
            Microsoft::WRL::ComPtr<ID3D12Fence>               m_fence_ptr;
            uint64_t                                          m_fence_value;
            ID3D12CommandQueue*                               m_queue_ptr;
        };
    }
}

#endif /* UTILS_D3D12_D3D12_COMMAND_RUNNER_H */
