/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "headset_sink.h"
#include <math.h>
#include <Windows.h>
#include <mmsystem.h>

Stereo::Presenter::HeadsetSink::HeadsetSink(Stereo::Transport::IFrameImporter*                  inout_importer_ptr,
                                            std::unique_ptr<Headset::XR::HeadsetSession>        in_session_ptr,
                                            std::unique_ptr<Headset::D3D12::D3D12FrameUploader> in_uploader_ptr,
                                            const ExitFunc&                                     in_exit_func,
                                            const LogFunc&                                      in_log_func,
                                            const bool&                                         in_is_pacing)
    :m_exit_func        (in_exit_func),
     m_frame_period     (0.0),
     m_frame_start      (0.0),
     m_ipd_scale_sent   (0.0f),
     m_is_pacing        (in_is_pacing),
     m_pace_delay       (0.0),
     m_has_frame        (false),
     m_is_stopping      (false),
     m_importer_ptr     (inout_importer_ptr),
     m_last_frame_index (0),
     m_has_shown_frame  (false),
     m_shown_frame_index(0),
     m_shown_timestamp  (0.0),
     m_log_func         (in_log_func),
     m_session_ptr      (std::move(in_session_ptr) ),
     m_uploader_ptr     (std::move(in_uploader_ptr) )
{
    const auto on_frame_func = [this](const double& in_period_n_milliseconds)
    {
        on_compositor_frame(in_period_n_milliseconds);
    };

    m_session_ptr->set_frame_func(on_frame_func);

    const auto run_frames_func = [this]()
    {
        run_frames();
    };

    m_thread = std::thread(run_frames_func);

    const auto run_uploads_func = [this]()
    {
        run_uploads();
    };

    m_upload_thread = std::thread(run_uploads_func);
}

Stereo::Presenter::HeadsetSink::~HeadsetSink()
{
    m_is_stopping = true;

    m_session_ptr->request_exit();

    if (m_upload_thread.joinable() )
    {
        m_upload_thread.join();
    }

    if (m_thread.joinable() )
    {
        m_thread.join();
    }
}

std::unique_ptr<Stereo::Presenter::HeadsetSink> Stereo::Presenter::HeadsetSink::create_ptr(Stereo::Transport::IFrameImporter* inout_importer_ptr,
                                                                                           const ExitFunc&                    in_exit_func,
                                                                                           const LogFunc&                     in_log_func,
                                                                                           const bool&                        in_is_pacing,
                                                                                           std::string*                       out_error_ptr)
{
    std::unique_ptr<HeadsetSink>                        result_ptr;
    auto                                                session_ptr  = Headset::XR::HeadsetSession::create_ptr(out_error_ptr);
    std::unique_ptr<Headset::D3D12::D3D12FrameUploader> uploader_ptr;

    if (session_ptr == nullptr)
    {
        goto end;
    }

    uploader_ptr.reset(new Headset::D3D12::D3D12FrameUploader(session_ptr->get_device_ptr(),
                                                              session_ptr->get_queue_ptr () ));

    if (uploader_ptr->is_ok(out_error_ptr) == false)
    {
        goto end;
    }

    in_log_func("The headset is " + session_ptr->get_system_name() );

    result_ptr.reset(new HeadsetSink(inout_importer_ptr,
                                     std::move(session_ptr),
                                     std::move(uploader_ptr),
                                     in_exit_func,
                                     in_log_func,
                                     in_is_pacing) );

end:
    return result_ptr;
}

bool Stereo::Presenter::HeadsetSink::get_shown_frame(uint64_t* out_frame_index_ptr,
                                                     double*   out_timestamp_seconds_ptr) const
{
    const bool result = m_has_shown_frame;

    if (result)
    {
        *out_frame_index_ptr       = m_shown_frame_index;
        *out_timestamp_seconds_ptr = m_shown_timestamp;
    }

    return result;
}

void Stereo::Presenter::HeadsetSink::log_once(const std::string& in_text)
{
    bool is_new = false;

    {
        std::lock_guard<std::mutex> lock(m_log_mutex);

        is_new = m_logged_text_set.insert(in_text).second;
    }

    if (is_new)
    {
        m_log_func(in_text);
    }
}

void Stereo::Presenter::HeadsetSink::on_compositor_frame(const double& in_period_n_milliseconds)
{
    /* The natural eye separation is computed for eyes this far apart (a typical distance). The scale is the person's eye distance relative to
     * that, limited to a plausible range. It is sent only when it has changed enough to be noticed.
     */
    constexpr float CHANGE_THAT_IS_SAID  = 0.005f;
    constexpr float REFERENCE_IPD_METRES = 0.064f;
    constexpr float SCALE_MAX            = 1.4f;
    constexpr float SCALE_MIN            = 0.7f;

    const float ipd = m_session_ptr->get_ipd();

    if (ipd > 0.0f)
    {
        const float ratio = ipd / REFERENCE_IPD_METRES;
        const float scale = (ratio < SCALE_MIN) ? SCALE_MIN
                          : (ratio > SCALE_MAX) ? SCALE_MAX
                                                : ratio;

        if (::fabsf(scale - m_ipd_scale_sent) > CHANGE_THAT_IS_SAID)
        {
            m_importer_ptr->set_ipd_scale(scale);

            m_ipd_scale_sent = scale;
        }
    }

    m_frame_period = in_period_n_milliseconds;
    m_frame_start  = Utils::Statistics::SeriesStats::get_now();

    if (m_is_pacing)
    {
        m_importer_ptr->signal_pace();
    }
}

void Stereo::Presenter::HeadsetSink::run_frames()
{
    std::string error;

    while (m_session_ptr->run_frame(&error) )
    {
        /* The log lines of the session are written as they occur, because they show what the headset did. */
        for (const auto& line : m_session_ptr->take_log_lines() )
        {
            m_log_func(line);
        }
    }

    for (const auto& line : m_session_ptr->take_log_lines() )
    {
        m_log_func(line);
    }

    if (m_is_stopping == false)
    {
        log_once("The session of the headset has ended");

        m_exit_func();
    }
}

void Stereo::Presenter::HeadsetSink::run_uploads()
{
    /* The frames of the application come from other processes, which have no event to wake this thread. It therefore polls the memory of the
     * channel about every millisecond, which is the finest sleep granularity available once the system timer is set to tick that fast.
     */
    constexpr UINT TIMER_N_MILLISECONDS = 1;

    ::timeBeginPeriod(TIMER_N_MILLISECONDS);

    while (m_is_stopping == false)
    {
        bool                             is_new = false;
        Stereo::Transport::ImportedFrame frame;

        if (m_importer_ptr->acquire(&frame) )
        {
            is_new = (m_has_frame            == false               ||
                      frame.info.frame_index != m_last_frame_index);

            if (is_new)
            {
                upload_frame(frame);
            }

            m_importer_ptr->release();
        }

        if (is_new == false)
        {
            ::Sleep(1);
        }
    }

    ::timeEndPeriod(TIMER_N_MILLISECONDS);
}

void Stereo::Presenter::HeadsetSink::tune_pace(const double& in_released_n_milliseconds)
{
    /* The fraction of the timing error that is corrected per frame. It is small because the effect of a correction shows only in the images of the following frames. */
    constexpr double GAIN                         = 0.1;
    constexpr double TARGET                       = 0.3; /* of a frame: the images should be ready this far into it; the rest is margin for a late copy */
    constexpr double MICROSECONDS_PER_MILLISECOND = 1000.0;

    const double period = m_frame_period;
    const double start  = m_frame_start;

    if (m_is_pacing       &&
        period      > 0.0 &&
        start       > 0.0)
    {
        const double phase = ::fmod(in_released_n_milliseconds - start,
                                    period);

        /* Images that are ready too early give a positive error, which makes the delay longer. The phase wraps around every frame, so the error is taken the short way around. */
        double error = TARGET * period - phase;

        if (error > period / 2.0)
        {
            error -= period;
        }

        if (error < -period / 2.0)
        {
            error += period;
        }

        m_pace_delay += GAIN * error;

        if (m_pace_delay < 0.0)
        {
            m_pace_delay += period;
        }

        if (m_pace_delay >= period)
        {
            m_pace_delay -= period;
        }

        m_importer_ptr->set_pace_delay_us(static_cast<uint32_t>(m_pace_delay * MICROSECONDS_PER_MILLISECOND) );

        m_timing.phase.add(phase);
    }
}

void Stereo::Presenter::HeadsetSink::upload_frame(const Stereo::Transport::ImportedFrame& in_frame)
{
    constexpr double REPORT_PERIOD_N_MILLISECONDS = 1000.0;

    std::string     error;
    ID3D12Resource* left_texture_ptr  = nullptr;
    ID3D12Resource* right_texture_ptr = nullptr;
    const double    start_ms          = Utils::Statistics::SeriesStats::get_now();

    /* The headset already shows this frame. */
    if (m_has_frame                                      &&
        in_frame.info.frame_index == m_last_frame_index)
    {
        goto end;
    }

    m_timing.arrival.add_interval(start_ms);

    if (m_session_ptr->acquire_eye_textures((in_frame.info.n_eyes > 1) ? in_frame.info.width / in_frame.info.n_eyes
                                                                       : in_frame.info.width,
                                            in_frame.info.height,
                                           &left_texture_ptr,
                                           &right_texture_ptr,
                                           &error) == false)
    {
        /* Do not log that the session is not running yet. Log any other reason why the headset cannot show the frame. */
        const size_t has_not_given_position = error.find("has not given");
        const size_t not_running_position   = error.find("not running");

        if (has_not_given_position == std::string::npos &&
            not_running_position   == std::string::npos)
        {
            log_once(error);
        }

        goto end;
    }

    const double acquired_ms = Utils::Statistics::SeriesStats::get_now();

    m_timing.acquire_cost.add(acquired_ms - start_ms);

    if (m_uploader_ptr->upload(in_frame,
                               left_texture_ptr,
                               right_texture_ptr,
                               Headset::XR::HeadsetSession::get_texture_state(),
                              &error) == false)
    {
        log_once(error);
    }

    m_timing.upload_cost.add(Utils::Statistics::SeriesStats::get_now() - acquired_ms);

    m_session_ptr->release_eye_textures();

    tune_pace(Utils::Statistics::SeriesStats::get_now() );

    m_has_frame        = true;
    m_last_frame_index = in_frame.info.frame_index;

    m_shown_frame_index = in_frame.info.frame_index;
    m_shown_timestamp   = in_frame.info.timestamp_seconds;
    m_has_shown_frame   = true;

    if (Utils::Statistics::SeriesStats::take_period(&m_timing.last_report,
                                                    start_ms,
                                                    REPORT_PERIOD_N_MILLISECONDS) )
    {
        m_log_func("Frames to the headset (ms): "                                 +
                   m_timing.arrival.take_text     ("arrivals")                    +
                   "; "                                                           +
                   m_timing.phase.take_text       ("images ready into the frame") +
                   ", pace delay "                                                +
                   std::to_string(m_pace_delay)                                   +
                   ", the eyes of the person are "                                +
                   std::to_string(m_session_ptr->get_ipd() * 1000.0f)             +
                   " mm apart"                                                    +
                   "; "                                                           +
                   m_timing.acquire_cost.take_text("taking the images")           +
                   "; "                                                           +
                   m_timing.upload_cost.take_text ("copy to the images") );
    }

end:
    return;
}
