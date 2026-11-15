/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "host_memory_exporter_base.h"
#include <windows.h>

Stereo::ConverterDLL::HostMemoryExporterBase::HostMemoryExporterBase(Stereo::Transport::FrameChannel* inout_channel_ptr)
    :m_channel_ptr   (inout_channel_ptr),
     m_last_frame_qpc(0)
{
    m_channel_ptr->set_offered_kinds(get_offered_kinds() );
}

Stereo::Transport::FrameChannel* Stereo::ConverterDLL::HostMemoryExporterBase::get_channel_ptr() const
{
    return m_channel_ptr;
}

Stereo::Transport::TransportKindMask Stereo::ConverterDLL::HostMemoryExporterBase::get_offered_kinds() const
{
    return static_cast<Stereo::Transport::TransportKindMask>(Stereo::Transport::TransportKind::HOST_MEMORY);
}

void Stereo::ConverterDLL::HostMemoryExporterBase::hold_to_frame_rate(const int64_t& in_n_frames_per_second)
{
    constexpr LONGLONG N_MILLISECONDS_PER_SECOND = 1000;

    LARGE_INTEGER frequency;
    LARGE_INTEGER now;

    ::QueryPerformanceFrequency(&frequency);
    ::QueryPerformanceCounter  (&now);

    const LONGLONG frame_n_ticks = frequency.QuadPart / in_n_frames_per_second;
    const LONGLONG due_qpc       = m_last_frame_qpc + frame_n_ticks;

    if (m_last_frame_qpc != 0       &&
        now.QuadPart     <  due_qpc)
    {
        ::Sleep(static_cast<DWORD>( (due_qpc - now.QuadPart) * N_MILLISECONDS_PER_SECOND / frequency.QuadPart) );
    }

    ::QueryPerformanceCounter(&now);

    m_last_frame_qpc = now.QuadPart;
}

bool Stereo::ConverterDLL::HostMemoryExporterBase::is_host_memory_negotiated() const
{
    return (m_channel_ptr->get_negotiated_kind() == Stereo::Transport::TransportKind::HOST_MEMORY);
}

void Stereo::ConverterDLL::HostMemoryExporterBase::restart_frame_rate_clock()
{
    LARGE_INTEGER now;

    ::QueryPerformanceCounter(&now);

    m_last_frame_qpc = now.QuadPart;
}

bool Stereo::ConverterDLL::HostMemoryExporterBase::wait_for_screen()
{
    /* The screen asks for a frame about every 14 ms. A wait much longer than a few of those intervals means that the screen is not asking. */
    constexpr uint32_t TIMEOUT_N_MILLISECONDS = 100;

    return m_channel_ptr->wait_for_pace(TIMEOUT_N_MILLISECONDS);
}
