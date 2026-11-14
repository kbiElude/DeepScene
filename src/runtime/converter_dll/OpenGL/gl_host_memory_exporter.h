/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_HOST_MEMORY_EXPORTER_H
#define STEREO_CONVERTERDLL_GL_HOST_MEMORY_EXPORTER_H

#include "host_memory_exporter.h"
#include "types.h"
#include "utils/parallel/job_thread.h"
#include <memory>
#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Passes the images of the eyes of an OpenGL application on by the memory of the channel. The images are already in the memory of the application (the
         * converter reads them from its framebuffers into buffers it has mapped, see GLFrameReader), so this only turns the rows over (OpenGL has the bottom one first), and
         * a thread of its own does that while the application goes on with the next frame. A frame that is given while the thread is still at the one before is left out.
         *
         * The source (see ExportSource) holds a GLFrameImages in the left surface, and anything that is not null in the right one. The exporter keeps a copy of it, with
         * the token that says the memory is in use, until the thread is through.
         */
        class GLHostMemoryExporter : public HostMemoryExporter
        {
        public:
            /* The channel must outlive the exporter. The exporter says on it what it offers. */
            explicit GLHostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr);

            ~GLHostMemoryExporter() override;

            GLHostMemoryExporter(const GLHostMemoryExporter&)            = delete;
            GLHostMemoryExporter& operator=(const GLHostMemoryExporter&) = delete;

            /* EXPORTED when the thread has the frame to copy to the channel. NOT_READY until the other side has said that it takes the host memory kind, and while the thread is
             * at the frame before; FAILED if the images are larger than the channel takes.
             */
            Stereo::Transport::ExportStatus export_frame(const uint64_t&                        in_frame_index,
                                                         const Stereo::Transport::ExportSource& in_source) override;

            void wait_for_pace() override;

            /* A debugging aid: the images of the eyes, side by side, in a file of the type BMP. */
            static bool write_debug_image(const GLFrameImages& in_images,
                                          const std::string&   in_path);

        private:
            static const uint32_t BYTES_PER_PIXEL;
            static const uint32_t N_EYES;

            /* What the thread is to copy. */
            struct Job
            {
                uint64_t      frame_index       = 0;
                GLFrameImages images;
                double        timestamp_seconds = 0.0;
            };

            /* What the thread does for a frame: copies the rows of the images of the job to the channel. */
            void write_job();

            /* Puts the rows of an image of an eye (B, G, R, A of 8 bits, from the bottom up as OpenGL gives them) into the image of both eyes, where the eye is the place of the
             * image across; the top row of the destination is the last of the source if the rows are turned over, and the first if not.
             */
            static void write_eye_rows(const uint8_t*  in_bgra_ptr,
                                       const uint32_t& in_width,
                                       const uint32_t& in_height,
                                       const uint32_t& in_eye,
                                       const bool&     in_is_turned_over,
                                       uint8_t*        out_destination_ptr,
                                       const uint32_t& in_pitch);

            Job                                         m_job;
            std::unique_ptr<Utils::Parallel::JobThread> m_job_thread_ptr;
            int64_t                                     m_n_frames_per_second; /* the application is held to this many frames a second when no screen keeps time (APISTEREO_FRAME_RATE); 0: it is not */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_HOST_MEMORY_EXPORTER_H */
