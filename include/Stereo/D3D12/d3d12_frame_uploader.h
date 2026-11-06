/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_D3D12_D3D12_FRAME_UPLOADER_H
#define STEREO_D3D12_D3D12_FRAME_UPLOADER_H

#include "Stereo/D3D12/d3d12_command_runner.h"
#include "Stereo/Transport/types.h"
#include <map>
#include <string>
#include <d3d12.h>
#include <wrl/client.h>

namespace Stereo
{
    namespace D3D12
    {
        /* Puts the eyes of a frame, which the producer wrote to the memory of the channel, into textures of a device, and the CPU does not
         * touch the pixels: the memory is opened as a heap of the device (what lets it read what another process wrote, for a GPU
         * which can read system memory), and the GPU copies from it. A frame is copied from the memory it is in, so that a frame which the
         * channel hands over is good to be released as soon as upload() returns.
         *
         * Not thread-safe: the thread which owns the queue uploads.
         */
        class D3D12FrameUploader
        {
        public:
            /* The device and the queue (a direct one: it copies to textures) must outlive the uploader. The queue is the one that the
             * textures are used on, so that what is copied is there when they are.
             */
            D3D12FrameUploader(ID3D12Device*       in_device_ptr,
                               ID3D12CommandQueue* in_queue_ptr);

            D3D12FrameUploader           (const D3D12FrameUploader&) = delete;
            D3D12FrameUploader& operator=(const D3D12FrameUploader&) = delete;

            /* False, and the reason, if the uploader could not make what it needs (the device cannot open memory as a heap, say). */
            bool is_ok(std::string* out_error_ptr) const;

            /* Copies the left eye of the frame into the left texture and the right eye into the right one; a frame of one eye is copied to
             * both. A texture is of the format of a pixel of the frame (see PixelFormat), or of a format of its family which has the same
             * bytes, with the size of an eye or more, and is in the state given; it is in it again when the copy is done, and the call
             * returns when it is. Returns false, and the reason, if it cannot be done.
             */
            bool upload(const Stereo::Transport::ImportedFrame& in_frame,
                        ID3D12Resource*                         in_left_texture_ptr,
                        ID3D12Resource*                         in_right_texture_ptr,
                        const D3D12_RESOURCE_STATES&            in_texture_state,
                        std::string*                            out_error_ptr);

        private:
            /* The memory of the channel, as the device has it: a heap of all of it (it can be opened only from the start of what was
             * mapped), and a buffer over all of the heap.
             */
            struct Region
            {
                Microsoft::WRL::ComPtr<ID3D12Resource> buffer_ptr;
                Microsoft::WRL::ComPtr<ID3D12Heap>     heap_ptr;
            };

            /* The region of the memory which holds the frame, made when it is first seen, and where in it the pixels start. Returns nullptr,
             * and says why, if it cannot be.
             */
            Region* get_region_ptr(const uint8_t* in_pixel_ptr,
                                   uint64_t*      out_offset_ptr,
                                   std::string*   out_error_ptr);


            Microsoft::WRL::ComPtr<ID3D12Device3> m_device_ptr;
            std::string                           m_error; /* what stopped the uploader from being made; empty if nothing did */
            std::map<const void*, Region>         m_region_map; /* by where what was mapped starts */
            D3D12CommandRunner                    m_runner;     /* last: it is the first to go, and waits for what the GPU reads of the rest */
        };
    }
}

#endif /* STEREO_D3D12_D3D12_FRAME_UPLOADER_H */
