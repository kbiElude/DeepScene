/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_passer.h"
#include "own_call_scope.h"
#include <Windows.h>

Stereo::ConverterDLL::FramePasser::FramePasser(std::unique_ptr<Stereo::Transport::IFrameExporter> in_exporter_ptr,
                                               const SessionLog&                                  in_log)
    :m_log(in_log)
{
    LARGE_INTEGER start;

    ::QueryPerformanceCounter(&start);

    m_frame_state.exporter_ptr = std::move(in_exporter_ptr);
    m_frame_state.start_qpc    = start.QuadPart;
}

void Stereo::ConverterDLL::FramePasser::on_present(void* in_device_ptr,
                                                   void* in_left_surface_ptr,
                                                   void* in_right_surface_ptr)
{
    /* The host shows about this many images in a second; copying more would only slow the application down. */
    constexpr int64_t MIN_COPY_INTERVAL_N_MILLISECONDS = 8;
    constexpr double  REPORT_PERIOD_N_MILLISECONDS     = 1000.0;

    LARGE_INTEGER frequency;
    LARGE_INTEGER now;

    ::QueryPerformanceFrequency(&frequency);
    ::QueryPerformanceCounter  (&now);

    {
        std::lock_guard<std::mutex> lock(m_frame_state.mutex);

        const int64_t interval_qpc = frequency.QuadPart * MIN_COPY_INTERVAL_N_MILLISECONDS / 1000;
        const double  now_ms       = Utils::Statistics::SeriesStats::get_now();

        m_frame_state.present_interval.add_interval(now_ms);

        if (now.QuadPart - m_frame_state.last_copy_qpc >= interval_qpc)
        {
            /* The device is read through its methods, which are the application's to watch and not ours. */
            const OwnCallScope              own_call_scope;
            Stereo::Transport::ExportSource source;

            m_frame_state.last_copy_qpc = now.QuadPart;

            source.api_device_ptr    = in_device_ptr;
            source.left_surface_ptr  = in_left_surface_ptr;
            source.right_surface_ptr = in_right_surface_ptr;
            source.timestamp_seconds = static_cast<double>(now.QuadPart - m_frame_state.start_qpc) / static_cast<double>(frequency.QuadPart);

            const Stereo::Transport::ExportStatus status = m_frame_state.exporter_ptr->export_frame(m_frame_state.frame_index,
                                                                                                    source);

            switch (status)
            {
                case Stereo::Transport::ExportStatus::EXPORTED:
                {
                    const double done_ms = Utils::Statistics::SeriesStats::get_now();

                    m_frame_state.export_cost.add          (done_ms - now_ms);
                    m_frame_state.export_interval.add_interval(now_ms);

                    if (m_frame_state.has_logged_frame == false)
                    {
                        m_log.write("The first frame is passed on");

                        m_frame_state.has_logged_frame = true;
                    }

                    break;
                }

                case Stereo::Transport::ExportStatus::FAILED:
                {
                    if (m_frame_state.has_logged_failure == false)
                    {
                        m_log.write_failure("A frame cannot be passed on: the back buffer cannot be read, or its format is not converted");

                        m_frame_state.has_logged_failure = true;
                    }

                    break;
                }

                case Stereo::Transport::ExportStatus::NOT_READY:
                default:
                {
                    /* The program which shows the images has not said what it takes yet. */
                    break;
                }
            }
        }

        ++m_frame_state.frame_index;

        /* What the application and the copies do, a second at a time: what tells whether the images come evenly. */
        const bool is_log_enabled = m_log.is_enabled();

        if (is_log_enabled)
        {
            const bool is_report_due = Utils::Statistics::SeriesStats::take_period(&m_frame_state.last_report,
                                                                                   now_ms,
                                                                                   REPORT_PERIOD_N_MILLISECONDS);

            if (is_report_due)
            {
                m_log.write("Pacing (ms): "                                                          +
                            m_frame_state.present_interval.take_text("app presents")                 +
                            "; "                                                                    +
                            m_frame_state.export_interval.take_text("frames passed on")              +
                            "; "                                                                    +
                            m_frame_state.export_cost.take_text("copy cost")                        +
                            "; "                                                                    +
                            m_frame_state.exporter_ptr->take_timing_text() );
            }
        }
    }
}

void Stereo::ConverterDLL::FramePasser::wait_for_pace()
{
    /* Not under the lock of the state: the wait is as long as a frame. */
    m_frame_state.exporter_ptr->wait_for_pace();
}
