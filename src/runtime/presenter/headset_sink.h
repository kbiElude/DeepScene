/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_PRESENTER_HEADSET_SINK_H
#define STEREO_PRESENTER_HEADSET_SINK_H

#include "headset/d3d12/d3d12_frame_uploader.h"
#include "utils/statistics/series_stats.h"
#include "stereo/transport/i_frame_importer.h"
#include "headset/xr/headset_session.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

namespace Stereo
{
    namespace Presenter
    {
        /* Shows the eyes in the headset of an OpenXR runtime (see HeadsetSession): the GPU copies them to the swap chains from the memory
         * of the channel (see D3D12FrameUploader). A thread runs the frames of the compositor, which wait for it; another takes the frames
         * of the application from the importer as they are made, so that nothing else (a window that waits for the screen to refresh) sets
         * the pace of them; what is not seen is given again, so that the screen stays where it is when the application is slow.
         *
         * The sink is the one which reads from the importer, for as long as it lives: the channel has room for one reader.
         */
        class HeadsetSink
        {
        public:
            typedef std::function<void()>                           ExitFunc;
            typedef std::function<void(const std::string& in_text)> LogFunc;

            /* Returns nullptr, and says why, if there is no runtime, or no headset. The exit function is called, from another thread, when the
             * user takes the application out of the headset, or the runtime ends the session.
             */
            static std::unique_ptr<HeadsetSink> create_ptr(Stereo::Transport::IFrameImporter* inout_importer_ptr,
                                                           const ExitFunc&                    in_exit_func,
                                                           const LogFunc&                     in_log_func,
                                                           const bool&                        in_is_pacing,
                                                           std::string*                       out_error_ptr);

            ~HeadsetSink();

            HeadsetSink(const HeadsetSink&)            = delete;
            HeadsetSink& operator=(const HeadsetSink&) = delete;

            /* Of the frame that was copied to the headset last; any thread. False before the first. */
            bool get_shown_frame(uint64_t* out_frame_index_ptr,
                                 double*   out_timestamp_seconds_ptr) const;

        private:
            HeadsetSink(Stereo::Transport::IFrameImporter*                  inout_importer_ptr,
                        std::unique_ptr<Headset::XR::HeadsetSession>        in_session_ptr,
                        std::unique_ptr<Headset::D3D12::D3D12FrameUploader> in_uploader_ptr,
                        const ExitFunc&                                     in_exit_func,
                        const LogFunc&                                      in_log_func,
                        const bool&                                         in_is_pacing);

            /* Says it once: the same thing every frame is of no use to the person who reads the log. */
            void log_once(const std::string& in_text);

            /* The compositor wants a frame: the application is told, and where in the frame the images came is looked at. */
            void on_compositor_frame(const double& in_period_n_milliseconds);

            /* Moves the moment at which the application is let go toward the one that has its images ready half a frame before they are
             * wanted: the time of the copy to the headset, and of the frame before it, is not known beforehand.
             */
            void tune_pace(const double& in_released_n_milliseconds);

            /* What the threads do. */
            void run_frames ();
            void run_uploads();

            /* Copies the frame to the images of the eyes, if it is not the one that is shown. */
            void upload_frame(const Stereo::Transport::ImportedFrame& in_frame);

            /* What the frames that the window passes on take, for the log, a second at a time. Of the thread of the window. */
            struct Timing
            {
                Utils::Statistics::SeriesStats acquire_cost = Utils::Statistics::SeriesStats(4.0);  /* of taking the images of the eyes, ms  */
                Utils::Statistics::SeriesStats arrival      = Utils::Statistics::SeriesStats(20.0); /* between the frames that are new, ms   */
                Utils::Statistics::SeriesStats phase        = Utils::Statistics::SeriesStats(0.0);  /* how far into a frame of the headset the images are ready, ms */
                double                         last_report  = 0.0;
                Utils::Statistics::SeriesStats upload_cost  = Utils::Statistics::SeriesStats(4.0);  /* of the copy to the images, ms         */
            };

            std::atomic<double>                                 m_frame_start;           /* when the compositor asked for the last frame, ms */
            ExitFunc                                            m_exit_func;
            std::atomic<double>                                 m_frame_period;          /* of the headset, ms                               */
            bool                                                m_is_pacing;
            bool                                                m_has_frame;
            std::atomic<bool>                                   m_is_stopping;
            std::atomic<uint64_t>                               m_shown_frame_index;     /* for the window, if m_has_shown_frame */
            std::atomic<bool>                                   m_has_shown_frame;
            std::atomic<double>                                 m_shown_timestamp;       /* of that frame, seconds               */
            uint64_t                                            m_last_frame_index;      /* of the frame which was copied last, if m_has_frame */
            Stereo::Transport::IFrameImporter*                  m_importer_ptr;
            float                                               m_ipd_scale_sent;        /* of the thread of the compositor; 0: none yet */
            double                                              m_pace_delay;            /* in ms: of the upload thread                      */
            LogFunc                                             m_log_func;
            std::mutex                                          m_log_mutex;             /* of the set of what was said: two threads say things */
            std::set<std::string>                               m_logged_text_set;
            std::unique_ptr<Headset::XR::HeadsetSession>        m_session_ptr;
            std::thread                                         m_thread;
            std::thread                                         m_upload_thread;
            Timing                                              m_timing;
            std::unique_ptr<Headset::D3D12::D3D12FrameUploader> m_uploader_ptr;
        };
    }
}

#endif /* STEREO_PRESENTER_HEADSET_SINK_H */
