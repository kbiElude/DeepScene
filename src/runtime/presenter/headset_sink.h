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
        /* Shows the eyes in the headset of an OpenXR runtime (see HeadsetSession). The GPU copies them to the swap chains from the memory of the
         * channel (see D3D12FrameUploader). One thread runs the frames of the compositor, which wait for it. Another thread takes the frames of the
         * application from the importer as they are produced, so that nothing else (such as a window that waits for the screen to refresh) sets
         * their pace. A frame that has not been shown is given to the headset again, so that the picture stays in place when the application is slow.
         *
         * While it lives, the sink is the only reader of the importer, because the channel has room for one reader.
         */
        class HeadsetSink
        {
        public:
            typedef std::function<void()>                           ExitFunc;
            typedef std::function<void(const std::string& in_text)> LogFunc;

            /* Returns nullptr and sets the error if there is no runtime or no headset. The exit function is called, from another thread, when the user
             * takes the application out of the headset or the runtime ends the session.
             */
            static std::unique_ptr<HeadsetSink> create_ptr(Stereo::Transport::IFrameImporter* inout_importer_ptr,
                                                           const ExitFunc&                    in_exit_func,
                                                           const LogFunc&                     in_log_func,
                                                           const bool&                        in_is_pacing,
                                                           std::string*                       out_error_ptr);

            ~HeadsetSink();

            HeadsetSink(const HeadsetSink&)            = delete;
            HeadsetSink& operator=(const HeadsetSink&) = delete;

            /* Returns the index and the timestamp of the frame that was copied to the headset last. Callable from any thread. Returns false before the first frame. */
            bool get_shown_frame(uint64_t* out_frame_index_ptr,
                                 double*   out_timestamp_seconds_ptr) const;

        private:
            HeadsetSink(Stereo::Transport::IFrameImporter*                  inout_importer_ptr,
                        std::unique_ptr<Headset::XR::HeadsetSession>        in_session_ptr,
                        std::unique_ptr<Headset::D3D12::D3D12FrameUploader> in_uploader_ptr,
                        const ExitFunc&                                     in_exit_func,
                        const LogFunc&                                      in_log_func,
                        const bool&                                         in_is_pacing);

            /* Logs each text only once, because the same message on every frame does not help the reader of the log. */
            void log_once(const std::string& in_text);

            /* Handles the request of the compositor for a frame: updates the eye-distance scale, records the frame timing, and, when pacing, lets the application render. */
            void on_compositor_frame(const double& in_period_n_milliseconds);

            /* Adjusts the delay before the application is let go so that its images are ready about 0.3 of a frame into each headset frame. The delay
             * is adjusted step by step because the time of the copy to the headset, and of the frame before it, is not known beforehand.
             */
            void tune_pace(const double& in_released_n_milliseconds);

            /* The thread functions. */
            void run_frames ();
            void run_uploads();

            /* Copies the frame to the eye images, unless it is the frame that is already shown. */
            void upload_frame(const Stereo::Transport::ImportedFrame& in_frame);

            /* The time that the frames sent to the headset take, for the log, one second at a time. Used by the upload thread. */
            struct Timing
            {
                Utils::Statistics::SeriesStats acquire_cost = Utils::Statistics::SeriesStats(4.0);  /* cost of taking the eye images, ms     */
                Utils::Statistics::SeriesStats arrival      = Utils::Statistics::SeriesStats(20.0); /* time between new frames, ms           */
                Utils::Statistics::SeriesStats phase        = Utils::Statistics::SeriesStats(0.0);  /* how far into a headset frame the images are ready, ms */
                double                         last_report  = 0.0;
                Utils::Statistics::SeriesStats upload_cost  = Utils::Statistics::SeriesStats(4.0);  /* cost of the copy to the eye images, ms */
            };

            std::atomic<double>                                 m_frame_start;           /* when the compositor asked for the last frame, ms */
            ExitFunc                                            m_exit_func;
            std::atomic<double>                                 m_frame_period;          /* frame period of the headset, ms                  */
            bool                                                m_is_pacing;
            bool                                                m_has_frame;
            std::atomic<bool>                                   m_is_stopping;
            std::atomic<uint64_t>                               m_shown_frame_index;     /* index of the shown frame, for the window; valid if m_has_shown_frame */
            std::atomic<bool>                                   m_has_shown_frame;
            std::atomic<double>                                 m_shown_timestamp;       /* timestamp of that frame, seconds     */
            uint64_t                                            m_last_frame_index;      /* index of the frame copied last; valid if m_has_frame */
            Stereo::Transport::IFrameImporter*                  m_importer_ptr;
            float                                               m_ipd_scale_sent;        /* last scale sent, used by the compositor thread; 0: none sent yet */
            double                                              m_pace_delay;            /* delay in ms, used by the upload thread           */
            LogFunc                                             m_log_func;
            std::mutex                                          m_log_mutex;             /* guards m_logged_text_set, because two threads log */
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
