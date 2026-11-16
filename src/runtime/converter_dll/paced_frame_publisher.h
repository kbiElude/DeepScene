/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_PACED_FRAME_PUBLISHER_H
#define STEREO_CONVERTERDLL_PACED_FRAME_PUBLISHER_H

#include "stereo/transport/i_frame_exporter.h"
#include "utils/statistics/series_stats.h"
#include "session_log.h"
#include "stereo/core/types.h"
#include <functional>
#include <memory>
#include <mutex>
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Passes the frames that the application is about to present to the viewer (see IFrameExporter), at the pace at which the viewer shows
         * them.
         */
        class PacedFramePublisher
        {
        public:
            /* Where the application's picture is in the eye images. Empty (or no size): all of it. */
            typedef std::function<Stereo::Core::PixelRect()> PictureFunc;

            PacedFramePublisher(std::unique_ptr<Stereo::Transport::IFrameExporter> in_exporter_ptr,
                                const PictureFunc&                                  in_picture_func,
                                const SessionLog&                                  in_log);

            /* Passes one frame on. If in_left_surface_ptr is nullptr, the left image is the back buffer of the device. in_right_surface_ptr is the
             * twin of the back buffer, or nullptr if the back buffer has none.
             */
            void on_present(void* in_device_ptr,
                            void* in_left_surface_ptr,
                            void* in_right_surface_ptr);

            /* Called on the thread of the application after the present. Waits until the screen wants the next frame, if the viewer reports that. */
            void wait_for_pace();

        private:
            /* The state that presenting a frame reads and writes. Lock order: no other lock is taken while its mutex is held. */
            struct FrameState
            {
                Utils::Statistics::SeriesStats                     export_cost        = Utils::Statistics::SeriesStats(4.0);  /* time that a copy costs the application, in ms
 */
                Utils::Statistics::SeriesStats                     export_interval    = Utils::Statistics::SeriesStats(20.0); /* time between the frames that are passed on, in ms
 */
                std::unique_ptr<Stereo::Transport::IFrameExporter> exporter_ptr;
                uint64_t                                           frame_index        = 0;
                bool                                               has_logged_failure = false;
                bool                                               has_logged_frame   = false;
                int64_t                                            last_copy_qpc      = 0;
                double                                             last_report        = 0.0;
                std::mutex                                         mutex;
                int64_t                                            start_qpc          = 0; /* value of the performance counter when the passer was created; the frame timestamps count from it
 */
                Utils::Statistics::SeriesStats                     present_interval   = Utils::Statistics::SeriesStats(20.0); /* time between the presents of the application, in ms
 */
            };

            PictureFunc       m_picture_func;
            const SessionLog& m_log;
            FrameState        m_frame_state;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_PACED_FRAME_PUBLISHER_H */
