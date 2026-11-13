/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D8_HOST_MEMORY_EXPORTER_H
#define STEREO_CONVERTERDLL_D3D8_HOST_MEMORY_EXPORTER_H

#include "interception/hooks/D3D8/types.h"
#include "host_memory_exporter.h"
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
        /* Passes the images of the eyes of a Direct3D 8 device on by the memory of the channel, which is all that Direct3D 8 can: it cannot
         * share a surface with the GPU of another program. Each surface is copied to one in system memory, which the exporter keeps from
         * frame to frame; the rows of that are copied to the channel by a thread of its own (and the rows of the right eye by another, at
         * the same time), while the application goes on with the next frame. The device is called through its virtual table, so what the
         * exporter itself calls is not seen by anything which watches the device; it is called only from the thread of the application.
         *
         * There are two sets of surfaces: the thread copies from the one of the frame before while the next frame is read into the other.
         * The application waits for the thread to be through with the frame before only when it has the next one to hand over, so that the
         * thread cannot fall behind.
         *
         * The source (see ExportSource) holds the device, and the surfaces of the eyes: no left surface is the back buffer of the device, no
         * right surface is a frame of one eye, which is what the channel then holds; with both the image is twice as wide.
         */
        class D3D8HostMemoryExporter : public HostMemoryExporter
        {
        public:
            /* The channel must outlive the exporter. The exporter says on it what it offers. */
            explicit D3D8HostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr);

            ~D3D8HostMemoryExporter() override;

            D3D8HostMemoryExporter(const D3D8HostMemoryExporter&)            = delete;
            D3D8HostMemoryExporter& operator=(const D3D8HostMemoryExporter&) = delete;

            /* EXPORTED when the frame has been read from the device, and is on its way to the channel. NOT_READY until the other side has said
             * that it takes the host memory kind; FAILED if a surface cannot be read.
             */
            Stereo::Transport::ExportStatus export_frame(const uint64_t&                        in_frame_index,
                                                         const Stereo::Transport::ExportSource& in_source) override;

            std::string take_timing_text() override;

            void wait_for_pace() override;

        private:
            static constexpr size_t N_EYES = 2;
            static constexpr size_t N_SETS = 2;

            /* The copy in system memory of the surface of an eye. */
            struct Staging
            {
                Interception::Hooks::D3D8::D3DSURFACE_DESC    description = {};
                bool                                          is_locked   = false;
                Interception::Hooks::D3D8::IDirect3DSurface8* surface_ptr = nullptr;
            };

            /* What the thread is to copy: the surfaces of a frame, locked, and where they go. */
            struct Job
            {
                std::array<Interception::Hooks::D3D8::D3DSURFACE_DESC, N_EYES> description_array;
                Stereo::Transport::FrameInfo                                   info;
                std::array<const uint8_t*, N_EYES>                             source_ptr_array   = {};
                std::array<uint32_t, N_EYES>                                   source_pitch_array = {};
            };

            /* A staging surface of the size and format of the description, which is the one there is if it fits. */
            bool ensure_staging(void*                                             in_device_ptr,
                                const Interception::Hooks::D3D8::D3DSURFACE_DESC& in_description,
                                Staging*                                          inout_staging_ptr);

            /* Locks the staging surface to read from; false if it cannot be locked. */
            bool lock_staging(Staging*        inout_staging_ptr,
                              const uint8_t** out_bits_ptr_ptr,
                              uint32_t*       out_pitch_ptr);

            void unlock_staging(Staging* inout_staging_ptr);

            /* Waits until the thread is through with the job, and unlocks its surfaces. */
            void wait_for_job();

            void release_staging(Staging* inout_staging_ptr);

            /* Copies the rows of the surface of an eye to the memory of the channel (the second eye is of the second thread). */
            bool write_eye(const uint32_t& in_eye,
                           uint8_t*        out_destination_ptr,
                           const uint32_t& in_pitch);

            /* Copies the rows of the surfaces of the job to the channel. */
            void write_job();

            /* Copies the rows of the right eye of the job to the channel, for the thread of the right eye. */
            void write_right_eye();

            std::array<std::vector<uint8_t>, N_EYES>        m_converted_u8_vec_array; /* of the threads: one for each eye                          */
            uint8_t*                                        m_eye_destination_ptr;    /* of the work of the thread of the right eye               */
            bool                                            m_eye_is_ok;
            uint32_t                                        m_eye_pitch;
            std::unique_ptr<Utils::Parallel::JobThread>     m_eye_thread_ptr;         /* copies the right eye while the other thread copies the left */
            Utils::Statistics::SeriesStats                  m_issue_cost;             /* of asking the device for the copies, ms                  */
            Job                                             m_job;
            uint32_t                                        m_job_set;                /* which set of surfaces the job is of                      */
            std::unique_ptr<Utils::Parallel::JobThread>     m_job_thread_ptr;         /* copies the frame to the channel                          */
            Utils::Statistics::SeriesStats                  m_job_wait;               /* of waiting for the thread, ms                            */
            Utils::Statistics::SeriesStats                  m_lock_cost;              /* of locking the surfaces, ms                              */
            uint32_t                                        m_next_set;
            Utils::Statistics::SeriesStats                  m_pace_wait;              /* of waiting for the screen, ms                            */
            std::mutex                                      m_series_mutex;           /* guards the series of timings, which the threads add to   */
            std::array<std::array<Staging, N_EYES>, N_SETS> m_staging_array;
            Utils::Statistics::SeriesStats                  m_write_cost;             /* of the copy to the channel, ms                           */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D8_HOST_MEMORY_EXPORTER_H */
