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
        /* Passes the eye images of a Direct3D 7 application to the channel through host memory. The exporter locks a surface for reading
         * and copies its rows into the channel, converting them to 8-bit B, G, R and A. The application's thread does this work. Direct3D 7
         * images are small, so this is the simplest way to pass a frame on. The surfaces are called through their virtual tables, as the
         * application calls them, so nothing that watches the surfaces sees the exporter's calls.
         *
         * The source (see ExportSource) holds the eye surfaces. The left surface is required. Without a right surface the frame has one
         * eye and the channel holds a single image. With both surfaces the image is twice as wide.
         */
        class D3D7HostMemoryExporter : public HostMemoryExporter
        {
        public:
            /* The channel must outlive the exporter. The exporter says on it what it offers. */
            explicit D3D7HostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr);

            ~D3D7HostMemoryExporter() override;

            /* Returns EXPORTED when the frame is in the channel. Returns NOT_READY if the other side has not said that it takes the host memory
             * kind. Returns FAILED if a surface cannot be read, its format is not converted, or the eyes have different sizes.
             */
            Stereo::Transport::ExportStatus export_frame(const uint64_t&                        in_frame_index,
                                                         const Stereo::Transport::ExportSource& in_source) override;

            void wait_for_pace() override;

            /* A debugging aid: writes both eye images side by side to a BMP file with 32 bits per pixel. The right surface may be nullptr.
             * Returns false if a surface cannot be read or its format is not converted.
             */
            static bool write_debug_image(void*              in_left_surface_ptr,
                                          void*              in_right_surface_ptr,
                                          const std::string& in_path);

        private:
            static constexpr size_t N_EYES = 2;

            /* A surface that is locked for reading. */
            struct LockedSurface
            {
                Interception::Hooks::D3D7::DDSURFACEDESC2       description = {};
                bool                                            is_locked   = false;
                Interception::Hooks::D3D7::IDirectDrawSurface7* surface_ptr = nullptr;
            };

            /* The Direct3D 8 format (see PixelConverter) that matches the pixel format of the surface; 0 if no converted format matches. */
            static uint32_t get_pixel_format(const Interception::Hooks::D3D7::DDPIXELFORMAT& in_pixel_format);

            static bool lock_surface(void*          in_surface_ptr,
                                     LockedSurface* out_locked_ptr);

            static void unlock_surface(LockedSurface* inout_locked_ptr);

            /* Copies the rows of an eye's surface to the memory of the channel as 8-bit B, G, R and A pixels. */
            bool write_eye(const LockedSurface& in_locked,
                           const uint32_t&      in_eye,
                           uint8_t*             out_destination_ptr,
                           const uint32_t&      in_pitch);


            std::array<std::vector<uint8_t>, N_EYES> m_converted_u8_vec_array; /* per eye: conversion buffers for formats that are not 8-bit BGRA
 */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D7_HOST_MEMORY_EXPORTER_H */
