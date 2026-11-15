/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_D3D12_D3D12_TEXTURE_CLEARER_H
#define UTILS_D3D12_D3D12_TEXTURE_CLEARER_H

#include "utils/d3d12/d3d12_command_runner.h"
#include <string>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace Utils
{
    namespace D3D12
    {
        /* Fills textures of a device with black. The GPU does the clearing, and the call returns when it has finished. It is meant for the images of a
         * swap chain, which contain arbitrary memory until something is copied into them.
         *
         * Not thread-safe: only the thread that owns the queue may use it.
         */
        class D3D12TextureClearer
        {
        public:
            /* The device and the queue must outlive the clearer. The queue must be a direct queue because it clears render targets. It must also be the
             * queue the textures are used on, so that the clear has finished before they are used.
             */
            D3D12TextureClearer(ID3D12Device*       in_device_ptr,
                                ID3D12CommandQueue* in_queue_ptr);

            D3D12TextureClearer           (const D3D12TextureClearer&) = delete;
            D3D12TextureClearer& operator=(const D3D12TextureClearer&) = delete;

            /* Makes the texture black (colour channels 0, alpha 1). The texture must allow render-target use and must be in in_texture_state. It is in that
             * state again after the clear. in_view_format is the format the pixels are interpreted as, which a typeless texture needs. Returns false
             * and sets the error on failure.
             */
            bool clear_to_black(ID3D12Resource*              in_texture_ptr,
                                const DXGI_FORMAT&           in_view_format,
                                const D3D12_RESOURCE_STATES& in_texture_state,
                                std::string*                 out_error_ptr);

            /* Returns false and sets the error if the clearer could not create the objects it needs. */
            bool is_ok(std::string* out_error_ptr) const;

        private:
            Microsoft::WRL::ComPtr<ID3D12Device>         m_device_ptr;
            std::string                                  m_error; /* why construction failed; empty on success */
            D3D12CommandRunner                           m_runner;
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_view_heap_ptr;
        };
    }
}

#endif /* UTILS_D3D12_D3D12_TEXTURE_CLEARER_H */
