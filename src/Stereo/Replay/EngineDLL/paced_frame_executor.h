/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_PACED_FRAME_EXECUTOR_H
#define STEREO_REPLAY_ENGINEDLL_PACED_FRAME_EXECUTOR_H

#include "i_frame_executor.h"
#include "frame_pacer.h"
#include <atomic>
#include <memory>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            class PacedFrameHost;

            /* Plays the frames of another frame executor at the pace of the dumped process: the call that ends a frame is not given to it before
             * the time that the frame was presented at.
             */
            class PacedFrameExecutor : public ::Replay::ReplayEngineDLL::IFrameExecutor
            {
            public:
                PacedFrameExecutor(::Replay::ReplayEngineDLL::IFrameExecutor* in_executor_ptr);
                ~PacedFrameExecutor();

                bool execute_frame     (::Replay::ReplayEngineDLL::IFrameHost* in_host_ptr,
                                        bool*                                  out_has_workload_ended_ptr) override;
                void on_frame_presented() override;
                void on_play_started   () override;
                void on_rewind         () override;
                void set_speed         (const double&                          in_speed);

                /* The frames up to this one (counted from 0) are not waited for: the replay hurries to it, as a seek does. Any thread. Without a
                 * frame, or once the replay is past it, the pace is the speed again.
                 */
                void clear_unpaced_frame();
                void set_unpaced_frame  (const uint32_t&                       in_n_frame);

                /* Whether the frame that is about to be presented is one that the replay hurries through. */
                bool is_hurrying        (const uint32_t&                       in_n_frame) const;

            private:
                std::atomic<bool>                          m_has_unpaced_frame;
                ::Replay::ReplayEngineDLL::IFrameExecutor* m_executor_ptr;
                ::Replay::ReplayEngineDLL::IFrameHost*     m_host_ptr; /* the one the paced host is for */
                std::unique_ptr<PacedFrameHost>            m_paced_host_ptr;
                FramePacer                                 m_pacer;
                std::atomic<uint32_t>                      m_unpaced_frame;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_PACED_FRAME_EXECUTOR_H */
