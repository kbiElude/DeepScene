/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef HEADSET_D3D12_D3D12_FRAME_UPLOADER_H
#define HEADSET_D3D12_D3D12_FRAME_UPLOADER_H

#include "stereo/transport/types.h"
#include "utils/d3d12/d3d12_command_runner.h"
#include <map>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>

namespace Headset
{
    namespace D3D12
    {
        /* Copies the eyes of a frame that the producer wrote to the memory of the channel into textures of a D3D12 device, without the CPU
         * touching the pixels. The memory is opened as a heap of the device (this lets the device read what another process wrote, on a
         * GPU that can read system memory), and the GPU copies from it. The copy reads the frame where it is in memory, so a frame that
         * the channel hands over can be released as soon as upload() returns.
         *
         * Not thread-safe: only the thread that owns the queue may upload.
         */
        class D3D12FrameUploader
        {
        public:
            /* The device and the queue must outlive the uploader. The queue must be a direct queue, because it copies to textures. It is
             * the queue on which the textures are used, so that the copied pixels are there when the textures are used.
             */
            D3D12FrameUploader(ID3D12Device*       in_device_ptr,
                               ID3D12CommandQueue* in_queue_ptr);

            D3D12FrameUploader           (const D3D12FrameUploader&) = delete;
            D3D12FrameUploader& operator=(const D3D12FrameUploader&) = delete;

            /* Returns false and sets out_error_ptr to the reason if the uploader could not create what it needs (for example, the device
             * cannot open memory as a heap).
             */
            bool is_ok(std::string* out_error_ptr) const;

            /* Copies the left eye of the frame into the left texture and the right eye into the right texture. A frame of one eye is copied
             * to both. Each texture has the pixel format of the frame (see PixelFormat), or a format of the same family with the same bytes.
             * It is at least as large as one eye and is in the state in_texture_state. It is back in that state when the copy is done, and
             * the call returns when the copy is done. Returns false and sets out_error_ptr to the reason if the upload cannot be done.
             */
            bool upload(const Stereo::Transport::ImportedFrame& in_frame,
                        ID3D12Resource*                         in_left_texture_ptr,
                        ID3D12Resource*                         in_right_texture_ptr,
                        const D3D12_RESOURCE_STATES&            in_texture_state,
                        std::string*                            out_error_ptr);

        private:
            /* The memory of the channel as the device sees it: a heap over all of it (a heap can be opened only from the start of the mapped
             * allocation) and a buffer over the whole heap.
             */
            struct Region
            {
                Microsoft::WRL::ComPtr<ID3D12Resource> buffer_ptr;
                Microsoft::WRL::ComPtr<ID3D12Heap>     heap_ptr;
            };

            /* Returns the region of the memory that holds the frame, and creates it when the frame is first seen. Sets out_offset_ptr to
             * where the pixels start in the region. Returns nullptr and sets out_error_ptr to the reason if the region cannot be created.
             */
            Region* get_region_ptr(const uint8_t* in_pixel_ptr,
                                   uint64_t*      out_offset_ptr,
                                   std::string*   out_error_ptr);


            Microsoft::WRL::ComPtr<ID3D12Device3> m_device_ptr;
            std::string                           m_error;      /* the reason the uploader could not be set up; empty if setup succeeded */
            std::map<const void*, Region>         m_region_map; /* keyed by the start of the mapped allocation */
            Utils::D3D12::D3D12CommandRunner      m_runner;     /* last, so it is destroyed first and waits for the GPU */
        };
    }
}

#endif /* HEADSET_D3D12_D3D12_FRAME_UPLOADER_H */
