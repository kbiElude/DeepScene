/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D7_HOST_MEMORY_EXPORTER_H
#define STEREO_CONVERTERDLL_D3D7_HOST_MEMORY_EXPORTER_H

#include "interception/hooks/D3D7/types.h"
#include "host_memory_exporter.h"
#include <array>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Passes the images of the eyes of a Direct3D 7 application on by the memory of the channel. A surface is locked to be read, and
         * its rows are converted to B, G, R and A of 8 bits as they are copied to the channel, by the thread of the application: the
         * images of Direct3D 7 are small, and this is as simple as passing a frame on can be. The surfaces are called through their
         * virtual tables, as the application does, so what the exporter itself calls is not seen by anything which watches them.
         *
         * The source (see ExportSource) holds the surfaces of the eyes: the left one must be given; no right surface is a frame of one
         * eye, which is what the channel then holds; with both the image is twice as wide.
         */
        class D3D7HostMemoryExporter : public HostMemoryExporter
        {
        public:
            /* The channel must outlive the exporter. The exporter says on it what it offers. */
            explicit D3D7HostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr);

            ~D3D7HostMemoryExporter() override;

            /* EXPORTED when the frame is in the channel. NOT_READY until the other side has said that it takes the host memory kind;
             * FAILED if a surface cannot be read, its format is not converted, or the eyes are of different sizes.
             */
            Stereo::Transport::ExportStatus export_frame(const uint64_t&                        in_frame_index,
                                                         const Stereo::Transport::ExportSource& in_source) override;

            void wait_for_pace() override;

            /* A debugging aid: writes the images of the eyes side by side to a file of 32 bits a pixel (BMP); the right surface may be nullptr. Returns
             * false if a surface cannot be read or its format is not converted.
             */
            static bool write_debug_image(void*              in_left_surface_ptr,
                                          void*              in_right_surface_ptr,
                                          const std::string& in_path);

        private:
            static constexpr size_t N_EYES = 2;

            /* A surface which is locked to be read. */
            struct LockedSurface
            {
                Interception::Hooks::D3D7::DDSURFACEDESC2       description = {};
                bool                                            is_locked   = false;
                Interception::Hooks::D3D7::IDirectDrawSurface7* surface_ptr = nullptr;
            };

            /* The format of Direct3D 8 (see PixelConverter) of the pixels of a surface; 0 if they are of none that is converted. */
            static uint32_t get_pixel_format(const Interception::Hooks::D3D7::DDPIXELFORMAT& in_pixel_format);

            static bool lock_surface(void*          in_surface_ptr,
                                     LockedSurface* out_locked_ptr);

            static void unlock_surface(LockedSurface* inout_locked_ptr);

            /* Copies the rows of the surface of an eye, as pixels of 8 bits each of B, G, R and A, to the memory of the channel. */
            bool write_eye(const LockedSurface& in_locked,
                           const uint32_t&      in_eye,
                           uint8_t*             out_destination_ptr,
                           const uint32_t&      in_pitch);


            std::array<std::vector<uint8_t>, N_EYES> m_converted_u8_vec_array; /* of the formats that are not as they are wanted */
            int64_t                                  m_last_frame_qpc;         /* of the last frame that was waited for, with no one to show it to */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D7_HOST_MEMORY_EXPORTER_H */
