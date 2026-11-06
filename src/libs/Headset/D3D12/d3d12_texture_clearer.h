/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef HEADSET_D3D12_D3D12_TEXTURE_CLEARER_H
#define HEADSET_D3D12_D3D12_TEXTURE_CLEARER_H

#include "Headset/D3D12/d3d12_command_runner.h"
#include <string>
#include <d3d12.h>
#include <dxgi.h>
#include <wrl/client.h>

namespace Headset
{
    namespace D3D12
    {
        /* Fills textures of a device with black: the GPU clears them, and the call returns when it is done. For the images of a swap chain,
         * which hold whatever was in the memory until something is copied to them.
         *
         * Not thread-safe: the thread which owns the queue clears.
         */
        class D3D12TextureClearer
        {
        public:
            /* The device and the queue (a direct one: it clears render targets) must outlive the clearer. The queue is the one that the
             * textures are used on, so that what is cleared is there when they are.
             */
            D3D12TextureClearer(ID3D12Device*       in_device_ptr,
                                ID3D12CommandQueue* in_queue_ptr);

            D3D12TextureClearer           (const D3D12TextureClearer&) = delete;
            D3D12TextureClearer& operator=(const D3D12TextureClearer&) = delete;

            /* Makes the texture black (the colour channels 0, and the alpha 1). The texture can be a render target, and is in the state
             * given; it is in it again when it is cleared. The view format is the one that the pixels of the texture are seen as, which a
             * texture of a typeless format needs. Returns false, and the reason, if it cannot be done.
             */
            bool clear_to_black(ID3D12Resource*              in_texture_ptr,
                                const DXGI_FORMAT&           in_view_format,
                                const D3D12_RESOURCE_STATES& in_texture_state,
                                std::string*                 out_error_ptr);

            /* False, and the reason, if the clearer could not make what it needs. */
            bool is_ok(std::string* out_error_ptr) const;

        private:
            Microsoft::WRL::ComPtr<ID3D12Device>         m_device_ptr;
            std::string                                  m_error; /* what stopped the clearer from being made; empty if nothing did */
            D3D12CommandRunner                           m_runner;
            Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_view_heap_ptr;
        };
    }
}

#endif /* HEADSET_D3D12_D3D12_TEXTURE_CLEARER_H */
