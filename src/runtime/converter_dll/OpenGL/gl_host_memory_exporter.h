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
        /* Passes the eye images of an OpenGL application to the channel through host memory. The images are already in memory: the converter
         * reads them from its framebuffers into mapped buffers (see GLFrameReader). The exporter therefore only flips the rows, because OpenGL
         * stores the bottom row first. A thread of its own does this while the application goes on with the next frame. A frame that is
         * handed over while the thread is still working on the previous one is left out.
         *
         * The source (see ExportSource) holds a GLFrameImages in the left surface, and any non-null pointer in the right surface. The exporter
         * keeps a copy of the images, with the token that marks the memory as in use, until the thread has finished.
         */
        class GLHostMemoryExporter : public HostMemoryExporter
        {
        public:
            /* The channel must outlive the exporter. The exporter says on it what it offers. */
            explicit GLHostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr);

            ~GLHostMemoryExporter() override;

            GLHostMemoryExporter(const GLHostMemoryExporter&)            = delete;
            GLHostMemoryExporter& operator=(const GLHostMemoryExporter&) = delete;

            /* EXPORTED: the thread accepted the frame to copy to the channel. NOT_READY: the other side has not said that it accepts the host
             * memory kind, or the thread is still working on the previous frame. FAILED: the images are larger than the channel accepts.
             */
            Stereo::Transport::ExportStatus export_frame(const uint64_t&                        in_frame_index,
                                                         const Stereo::Transport::ExportSource& in_source) override;

            void wait_for_pace() override;

            /* A debugging aid: writes the eye images, side by side, to a BMP file. */
            static bool write_debug_image(const GLFrameImages& in_images,
                                          const std::string&   in_path);

        private:
            static const uint32_t BYTES_PER_PIXEL;
            static const uint32_t N_EYES;

            /* What the thread copies. */
            struct Job
            {
                uint64_t      frame_index       = 0;
                GLFrameImages images;
                double        timestamp_seconds = 0.0;
            };

            /* What the thread does for each frame: copies the rows of the images of the job to the channel. */
            void write_job();

            /* Writes the rows of the image of one eye (8-bit B, G, R, A, bottom row first as OpenGL gives them) into the image of both eyes.
             * in_eye is the position of the eye across that image. If the rows are turned over, the last source row becomes the top destination
             * row. Otherwise the first source row does.
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
            int64_t                                     m_n_frames_per_second; /* when no screen keeps time, the application is held to this many frames a second (APISTEREO_FRAME_RATE); 0: no limit */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_HOST_MEMORY_EXPORTER_H */
