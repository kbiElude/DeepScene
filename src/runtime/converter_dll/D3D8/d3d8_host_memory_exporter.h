/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D8_HOST_MEMORY_EXPORTER_H
#define STEREO_CONVERTERDLL_D3D8_HOST_MEMORY_EXPORTER_H

#include "interception/hooks/D3D8/types.h"
#include "host_memory_exporter_base.h"
#include "types.h"
#include "utils/parallel/job_thread.h"
#include "utils/statistics/series_stats.h"
#include <array>
#include <memory>
#include <mutex>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Passes the eye images of a Direct3D 8 device to the channel through host memory. Direct3D 8 cannot share a surface with the GPU of
         * another program, so host memory is the only way. Each eye surface is copied to a surface in system memory (a staging surface) that
         * the exporter keeps from frame to frame. A thread of its own copies the rows of the left eye to the channel, and a second thread
         * copies the rows of the right eye at the same time, while the application goes on with the next frame. The exporter calls the device
         * through its virtual table, so anything that watches the device does not see these calls. It is called only from the application's
         * thread.
         *
         * There are two sets of staging surfaces. The thread copies from the set of the previous frame while the next frame is read into the
         * other set. The application waits for the thread to finish the previous frame only when it has the next frame to hand over, so the
         * thread cannot fall behind.
         *
         * The source (see ExportSource) holds the device and the eye surfaces. If the left surface is null, the back buffer of the device is
         * the left eye. If the right surface is null, the frame has one eye. With both, the channel image is twice as wide.
         */
        class D3D8HostMemoryExporter : public HostMemoryExporterBase
        {
        public:
            /* The channel must outlive the exporter. The exporter says on it what it offers. */
            D3D8HostMemoryExporter(Stereo::Transport::FrameChannel*     inout_channel_ptr,
                                   const Analysis::Recipe::FramePacing& in_frame_pacing);

            ~D3D8HostMemoryExporter() override;

            D3D8HostMemoryExporter(const D3D8HostMemoryExporter&)            = delete;
            D3D8HostMemoryExporter& operator=(const D3D8HostMemoryExporter&) = delete;

            /* EXPORTED: the frame was read from the device and is on its way to the channel. NOT_READY: the other side has not said that it
             * accepts the host memory kind. FAILED: a surface cannot be read.
             */
            Stereo::Transport::ExportStatus export_frame(const uint64_t&                        in_frame_index,
                                                         const Stereo::Transport::ExportSource& in_source) override;

            std::string take_timing_text() override;

            void wait_for_pace() override;

        private:
            static constexpr size_t N_EYES = 2;
            static constexpr size_t N_SETS = 2;

            /* The system-memory copy of the surface of one eye. */
            struct Staging
            {
                Interception::Hooks::D3D8::D3DSURFACE_DESC    description = {};
                bool                                          is_locked   = false;
                Interception::Hooks::D3D8::IDirect3DSurface8* surface_ptr = nullptr;
            };

            /* What the thread copies: the locked staging surfaces of one frame, and the frame info for the channel. */
            struct Job
            {
                std::array<Interception::Hooks::D3D8::D3DSURFACE_DESC, N_EYES> description_array;
                Stereo::Transport::FrameInfo                                   info;
                std::array<const uint8_t*, N_EYES>                             source_ptr_array   = {};
                std::array<uint32_t, N_EYES>                                   source_pitch_array = {};
            };

            /* Makes sure the staging surface has the size and format of the description. Keeps the existing surface if it already fits. */
            bool ensure_staging(void*                                             in_device_ptr,
                                const Interception::Hooks::D3D8::D3DSURFACE_DESC& in_description,
                                Staging*                                          inout_staging_ptr);

            /* Locks the staging surface for reading. Returns false if it cannot be locked. */
            bool lock_staging(Staging*        inout_staging_ptr,
                              const uint8_t** out_bits_ptr_ptr,
                              uint32_t*       out_pitch_ptr);

            void unlock_staging(Staging* inout_staging_ptr);

            /* Waits until the thread has finished the job, then unlocks the surfaces of the job. */
            void wait_for_job();

            void release_staging(Staging* inout_staging_ptr);

            /* Copies the rows of the surface of one eye to the channel memory. The right eye is copied by the second thread. */
            bool write_eye(const uint32_t& in_eye,
                           uint8_t*        out_destination_ptr,
                           const uint32_t& in_pitch);

            /* Copies the rows of the surfaces of the job to the channel. */
            void write_job();

            /* Copies the rows of the right eye of the job to the channel. Runs on the thread of the right eye. */
            void write_right_eye();

            std::array<std::vector<uint8_t>, N_EYES>        m_converted_u8_vec_array; /* one buffer per eye, used by the copying threads */
            uint8_t*                                        m_eye_destination_ptr;    /* where the thread of the right eye writes */
            bool                                            m_eye_is_ok;
            uint32_t                                        m_eye_pitch;
            std::unique_ptr<Utils::Parallel::JobThread>     m_eye_thread_ptr;         /* copies the right eye while the job thread copies the left */
            Utils::Statistics::SeriesStats                  m_issue_cost;             /* time spent asking the device for the copies, ms */
            Job                                             m_job;
            uint32_t                                        m_job_set;                /* the set of surfaces that the job reads */
            std::unique_ptr<Utils::Parallel::JobThread>     m_job_thread_ptr;         /* copies the frame to the channel */
            Utils::Statistics::SeriesStats                  m_job_wait;               /* time spent waiting for the job thread, ms */
            Utils::Statistics::SeriesStats                  m_lock_cost;              /* time spent locking the surfaces, ms */
            uint32_t                                        m_next_set;
            Utils::Statistics::SeriesStats                  m_pace_wait;              /* time spent waiting for the screen, ms */
            std::mutex                                      m_series_mutex;           /* guards the timing series, which both threads add to */
            std::array<std::array<Staging, N_EYES>, N_SETS> m_staging_array;
            Utils::Statistics::SeriesStats                  m_write_cost;             /* time spent copying to the channel, ms */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D8_HOST_MEMORY_EXPORTER_H */
