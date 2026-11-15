/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "capture/api_dump_loader/api_info.h"
#include "paced_frame_executor.h"
#include <Windows.h>
#include <mmsystem.h>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* The host that the executor receives in place of the real host. It forwards everything to the real host, except that
             * reading the call that ends a frame waits until the time the frame was presented at.
             */
            class PacedFrameHost : public ::Replay::ReplayEngineDLL::IFrameHost
            {
            public:
                PacedFrameHost(::Replay::ReplayEngineDLL::IFrameHost* in_host_ptr,
                               FramePacer*                            in_pacer_ptr,
                               const HurriedFrameSelector*            in_hurry_ptr)
                    :m_host_ptr (in_host_ptr),
                     m_hurry_ptr(in_hurry_ptr),
                     m_pacer_ptr(in_pacer_ptr)
                {
                    /* Stub */
                }

                bool execute_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                  const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source) override
                {
                    return m_host_ptr->execute_call(in_call,
                                                    in_chunk_source);
                }

                void fail(const std::string& in_message) override
                {
                    m_host_ptr->fail(in_message);
                }

                ::Replay::ReplayEngineDLL::ICallExecutor* get_call_executor_ptr(const Capture::APIDumpLoader::APIGroup& in_group) const override
                {
                    return m_host_ptr->get_call_executor_ptr(in_group);
                }

                const std::vector< ::Replay::ReplayEngineDLL::ICallExecutor*>& get_call_executor_ptr_vec() const override
                {
                    return m_host_ptr->get_call_executor_ptr_vec();
                }

                const ::Replay::ReplayEngineDLL::IChunkSource& get_chunk_source() const override
                {
                    return m_host_ptr->get_chunk_source();
                }

                uint32_t get_n_frames_presented() const override
                {
                    return m_host_ptr->get_n_frames_presented();
                }

                const Capture::APIDumpLoader::Workload& get_segment() const override
                {
                    return m_host_ptr->get_segment();
                }

                bool read_next_call(const Capture::APIDumpLoader::DumpedAPICall** out_call_ptr_ptr) override
                {
                    const bool result      = m_host_ptr->read_next_call(out_call_ptr_ptr);
                    bool       is_boundary = false;

                    if (result)
                    {
                        is_boundary = Capture::APIDumpLoader::is_frame_boundary(**out_call_ptr_ptr);
                    }

                    if (is_boundary)
                    {
                        if (m_hurry_ptr->is_hurrying(m_host_ptr->get_n_frames_presented() ) )
                        {
                            /* Restart the timeline so that the time of the hurried frames is not made up for when normal pace resumes. */
                            m_pacer_ptr->reset();
                        }
                        else
                        {
                            m_pacer_ptr->wait_for_frame((*out_call_ptr_ptr)->timestamp_qpc,
                                                        m_host_ptr->get_segment().qpc_frequency);
                        }
                    }

                    return result;
                }

            private:
                ::Replay::ReplayEngineDLL::IFrameHost* m_host_ptr;
                const HurriedFrameSelector*            m_hurry_ptr;
                FramePacer*                            m_pacer_ptr;
            };
        }
    }
}

Stereo::Replay::EngineDLL::PacedFrameExecutor::PacedFrameExecutor(::Replay::ReplayEngineDLL::IFrameExecutor* in_executor_ptr,
                                                                  const HurriedFrameSelector*                in_hurry_ptr)
    :m_executor_ptr(in_executor_ptr),
     m_host_ptr    (nullptr),
     m_hurry_ptr   (in_hurry_ptr)
{
    /* Request a timer resolution of 1 ms; without it Sleep() is much less accurate. */
    ::timeBeginPeriod(1);
}

Stereo::Replay::EngineDLL::PacedFrameExecutor::~PacedFrameExecutor()
{
    ::timeEndPeriod(1);
}

bool Stereo::Replay::EngineDLL::PacedFrameExecutor::execute_frame(::Replay::ReplayEngineDLL::IFrameHost* in_host_ptr,
                                                                  bool*                                  out_has_workload_ended_ptr)
{
    /* The paced host is rebuilt only when the host changes. The engine gives the same host for as long as it lives. */
    if (m_host_ptr != in_host_ptr)
    {
        m_host_ptr       = in_host_ptr;
        m_paced_host_ptr.reset(new PacedFrameHost(in_host_ptr,
                                                  &m_pacer,
                                                  m_hurry_ptr) );
    }

    return m_executor_ptr->execute_frame(m_paced_host_ptr.get(),
                                         out_has_workload_ended_ptr);
}

void Stereo::Replay::EngineDLL::PacedFrameExecutor::on_frame_presented()
{
    m_executor_ptr->on_frame_presented();
}

void Stereo::Replay::EngineDLL::PacedFrameExecutor::on_play_started()
{
    /* After a pause or a reset the replay must not hurry to catch up on the time that passed, so restart the timeline. */
    m_pacer.reset();

    m_executor_ptr->on_play_started();
}

void Stereo::Replay::EngineDLL::PacedFrameExecutor::on_rewind()
{
    m_executor_ptr->on_rewind();
}

void Stereo::Replay::EngineDLL::PacedFrameExecutor::set_speed(const double& in_speed)
{
    m_pacer.set_speed(in_speed);
}
