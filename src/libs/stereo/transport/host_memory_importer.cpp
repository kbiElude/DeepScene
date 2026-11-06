/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/transport/host_memory_importer.h"

Stereo::Transport::HostMemoryImporter::HostMemoryImporter(std::unique_ptr<FrameChannel> in_channel_ptr)
    :m_channel_ptr(std::move(in_channel_ptr) )
{
    m_channel_ptr->set_accepted_kinds(static_cast<TransportKindMask>(TransportKind::HOST_MEMORY) );
}

bool Stereo::Transport::HostMemoryImporter::acquire(ImportedFrame* out_frame_ptr)
{
    return m_channel_ptr->begin_read(&out_frame_ptr->info,
                                     &out_frame_ptr->pixel_ptr);
}

void Stereo::Transport::HostMemoryImporter::release()
{
    m_channel_ptr->end_read();
}

void Stereo::Transport::HostMemoryImporter::set_ipd_scale(const float& in_ipd_scale)
{
    m_channel_ptr->set_ipd_scale(in_ipd_scale);
}

void Stereo::Transport::HostMemoryImporter::set_pace_delay_us(const uint32_t& in_delay_us)
{
    m_channel_ptr->set_pace_delay_us(in_delay_us);
}

void Stereo::Transport::HostMemoryImporter::signal_pace()
{
    m_channel_ptr->signal_pace();
}
