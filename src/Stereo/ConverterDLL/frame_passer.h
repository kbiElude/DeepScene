/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_FRAME_PASSER_H
#define STEREO_CONVERTERDLL_FRAME_PASSER_H

#include "Stereo/Transport/i_frame_exporter.h"
#include "Stereo/Transport/series_stats.h"
#include "session_log.h"
#include <memory>
#include <mutex>
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Passes on the frames which the application is about to present to the program which shows them (see IFrameExporter), at the
         * pace that program shows them at.
         */
        class FramePasser
        {
        public:
            FramePasser(std::unique_ptr<Stereo::Transport::IFrameExporter> in_exporter_ptr,
                        const SessionLog&                                  in_log);

            /* The right surface is the twin of the back buffer, if it has one. */
            void on_present(void* in_device_ptr,
                            void* in_right_surface_ptr);

            /* Called by the application's thread after the present: waits for the screen to want the next frame, if it says when. */
            void wait_for_pace();

        private:
            /* Everything which a presented frame touches. Lock order: no other lock is taken while it is held. */
            struct FrameState
            {
                Stereo::Transport::SeriesStats                     export_cost        = Stereo::Transport::SeriesStats(4.0);  /* what a copy takes the application, ms */
                Stereo::Transport::SeriesStats                     export_interval    = Stereo::Transport::SeriesStats(20.0); /* between the frames that are passed on */
                std::unique_ptr<Stereo::Transport::IFrameExporter> exporter_ptr;
                uint64_t                                           frame_index        = 0;
                bool                                               has_logged_failure = false;
                bool                                               has_logged_frame   = false;
                int64_t                                            last_copy_qpc      = 0;
                double                                             last_report        = 0.0;
                std::mutex                                         mutex;
                int64_t                                            start_qpc          = 0; /* of the converter, the clock of the timestamps */
                Stereo::Transport::SeriesStats                     present_interval   = Stereo::Transport::SeriesStats(20.0); /* between the presents of the application */
            };

            const SessionLog& m_log;
            FrameState        m_frame_state;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_FRAME_PASSER_H */
