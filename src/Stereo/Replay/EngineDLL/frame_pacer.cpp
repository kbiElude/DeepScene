/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_pacer.h"
#include <algorithm>
#include <Windows.h>

Stereo::Replay::EngineDLL::FramePacer::FramePacer()
    :m_average_gap_seconds(0.0),
     m_is_anchored       (false),
     m_last_timestamp_qpc(0),
     m_target_wall_ticks (0),
     m_speed             (1.0)
{
    /* Stub */
}

uint64_t Stereo::Replay::EngineDLL::FramePacer::get_wall_frequency()
{
    /* It does not change while the system runs. */
    static const uint64_t result = []
    {
        LARGE_INTEGER frequency;

        ::QueryPerformanceFrequency(&frequency);

        return static_cast<uint64_t>(frequency.QuadPart);
    }();

    return result;
}

uint64_t Stereo::Replay::EngineDLL::FramePacer::get_wall_ticks()
{
    LARGE_INTEGER ticks;

    ::QueryPerformanceCounter(&ticks);

    return static_cast<uint64_t>(ticks.QuadPart);
}

void Stereo::Replay::EngineDLL::FramePacer::reset()
{
    m_average_gap_seconds = 0.0;
    m_is_anchored         = false;
}

void Stereo::Replay::EngineDLL::FramePacer::set_speed(const double& in_speed)
{
    m_speed = in_speed;
}

void Stereo::Replay::EngineDLL::FramePacer::wait_for_frame(const uint64_t& in_timestamp_qpc,
                                                           const uint64_t& in_qpc_frequency)
{
    /* Sleep() is exact to a millisecond at best; the rest of the wait is spent spinning. */
    constexpr double GAP_AVERAGE_FACTOR    = 4.0;
    constexpr double GAP_AVERAGE_WEIGHT    = 0.1;
    constexpr double MAX_FRAME_GAP_SECONDS = 0.04;
    constexpr double RESYNC_LATE_SECONDS   = 0.1;
    constexpr double SPIN_SECONDS          = 0.0015;

    const uint64_t wall_frequency = get_wall_frequency();
    const double   speed          = m_speed;
    uint64_t       now_ticks      = get_wall_ticks    ();

    if (m_is_anchored    == false ||
        in_qpc_frequency == 0     ||
        speed            <= 0.0)
    {
        m_is_anchored        = true;
        m_last_timestamp_qpc = in_timestamp_qpc;
        m_target_wall_ticks  = now_ticks;

        goto end;
    }

    {
        double gap_seconds = (in_timestamp_qpc > m_last_timestamp_qpc) ? static_cast<double>(in_timestamp_qpc - m_last_timestamp_qpc) / static_cast<double>(in_qpc_frequency)
                                                                       : 0.0;

        const double usual_limit_seconds = m_average_gap_seconds * GAP_AVERAGE_FACTOR;
        const double longest_gap_seconds = (m_average_gap_seconds > 0.0 &&
                                            usual_limit_seconds   < MAX_FRAME_GAP_SECONDS) ? usual_limit_seconds
                                                                                           : MAX_FRAME_GAP_SECONDS;

        if (gap_seconds > longest_gap_seconds)
        {
            gap_seconds = longest_gap_seconds;
        }
        else
        {
            /* The average gap of the frames which were not cut. */
            m_average_gap_seconds = (m_average_gap_seconds > 0.0) ? m_average_gap_seconds * (1.0 - GAP_AVERAGE_WEIGHT) + gap_seconds * GAP_AVERAGE_WEIGHT
                                                                  : gap_seconds;
        }

        m_last_timestamp_qpc = in_timestamp_qpc;
        m_target_wall_ticks += static_cast<uint64_t>(gap_seconds / speed * static_cast<double>(wall_frequency) );

        /* Resync when too far behind. */
        if (now_ticks > m_target_wall_ticks + static_cast<uint64_t>(RESYNC_LATE_SECONDS * static_cast<double>(wall_frequency) ))
        {
            m_target_wall_ticks = now_ticks;
        }
    }

    while (now_ticks < m_target_wall_ticks)
    {
        const double remaining_seconds = static_cast<double>(m_target_wall_ticks - now_ticks) / static_cast<double>(wall_frequency);

        if (remaining_seconds > SPIN_SECONDS)
        {
            ::Sleep(1);
        }

        now_ticks = get_wall_ticks();
    }

end:
    return;
}
