/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Capture/APIDumpLoader/api_info.h"
#include "paced_frame_executor.h"
#include <Windows.h>
#include <mmsystem.h>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* What the executor of the frames is given in place of the host: the same, except that reading the call which ends a frame waits for the
             * time the frame was presented at.
             */
            class PacedFrameHost : public ::Replay::ReplayEngineDLL::IFrameHost
            {
            public:
                PacedFrameHost(::Replay::ReplayEngineDLL::IFrameHost* in_host_ptr,
                               FramePacer*                            in_pacer_ptr)
                    :m_host_ptr (in_host_ptr),
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
                    const bool result = m_host_ptr->read_next_call(out_call_ptr_ptr);

                    if (result                                                          &&
                        Capture::APIDumpLoader::is_frame_boundary(**out_call_ptr_ptr) )
                    {
                        m_pacer_ptr->wait_for_frame((*out_call_ptr_ptr)->timestamp_qpc,
                                                    m_host_ptr->get_segment().qpc_frequency);
                    }

                    return result;
                }

            private:
                ::Replay::ReplayEngineDLL::IFrameHost* m_host_ptr;
                FramePacer*                            m_pacer_ptr;
            };
        }
    }
}

Stereo::Replay::EngineDLL::PacedFrameExecutor::PacedFrameExecutor(::Replay::ReplayEngineDLL::IFrameExecutor* in_executor_ptr)
    :m_executor_ptr(in_executor_ptr),
     m_host_ptr    (nullptr)
{
    /* Sleep() is exact to a millisecond only if asked for. */
    ::timeBeginPeriod(1);
}

Stereo::Replay::EngineDLL::PacedFrameExecutor::~PacedFrameExecutor()
{
    ::timeEndPeriod(1);
}

bool Stereo::Replay::EngineDLL::PacedFrameExecutor::execute_frame(::Replay::ReplayEngineDLL::IFrameHost* in_host_ptr,
                                                                  bool*                                  out_has_workload_ended_ptr)
{
    /* The executor keeps the host it is given until the next frame: the one it is given stays as long as the engine. */
    if (m_host_ptr != in_host_ptr)
    {
        m_host_ptr       = in_host_ptr;
        m_paced_host_ptr.reset(new PacedFrameHost(in_host_ptr,
                                                  &m_pacer) );
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
    /* A pause, or a reset, must not make the replay hurry to catch up. */
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
