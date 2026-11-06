/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "host_memory_exporter.h"

Stereo::ConverterDLL::HostMemoryExporter::HostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr)
    :m_channel_ptr(inout_channel_ptr)
{
    m_channel_ptr->set_offered_kinds(get_offered_kinds() );
}

Stereo::Transport::FrameChannel* Stereo::ConverterDLL::HostMemoryExporter::get_channel_ptr() const
{
    return m_channel_ptr;
}

Stereo::Transport::TransportKindMask Stereo::ConverterDLL::HostMemoryExporter::get_offered_kinds() const
{
    return static_cast<Stereo::Transport::TransportKindMask>(Stereo::Transport::TransportKind::HOST_MEMORY);
}

bool Stereo::ConverterDLL::HostMemoryExporter::is_host_memory_negotiated() const
{
    return (m_channel_ptr->get_negotiated_kind() == Stereo::Transport::TransportKind::HOST_MEMORY);
}

bool Stereo::ConverterDLL::HostMemoryExporter::wait_for_screen()
{
    /* The screen asks about every 14 ms; a wait that is much longer than a few of them is not for it. */
    constexpr uint32_t TIMEOUT_N_MILLISECONDS = 100;

    return m_channel_ptr->wait_for_pace(TIMEOUT_N_MILLISECONDS);
}
