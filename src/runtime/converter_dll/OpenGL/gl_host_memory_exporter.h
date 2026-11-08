/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_HOST_MEMORY_EXPORTER_H
#define STEREO_CONVERTERDLL_GL_HOST_MEMORY_EXPORTER_H

#include "host_memory_exporter.h"
#include "types.h"
#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Passes the images of the eyes of an OpenGL application on by the memory of the channel. The images are already in the memory of the
         * application (the converter reads them from its framebuffers, see GlEyeRenderer), so this only turns the rows over (OpenGL has the bottom
         * one first) and the pixels to B, G, R, A.
         *
         * The source (see ExportSource) holds a GlFrameImages in the left surface, and anything that is not null in the right one.
         */
        class GlHostMemoryExporter : public HostMemoryExporter
        {
        public:
            /* The channel must outlive the exporter. The exporter says on it what it offers. */
            explicit GlHostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr);

            /* EXPORTED when the images are in the channel. NOT_READY until the other side has said that it takes the host memory kind; FAILED if the
             * images are larger than the channel takes.
             */
            Stereo::Transport::ExportStatus export_frame(const uint64_t&                        in_frame_index,
                                                         const Stereo::Transport::ExportSource& in_source) override;

            void wait_for_pace() override;

            /* A debugging aid: the images of the eyes, side by side, in a file of the type BMP. */
            static bool write_debug_image(const GlFrameImages& in_images,
                                          const std::string&   in_path);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_HOST_MEMORY_EXPORTER_H */
