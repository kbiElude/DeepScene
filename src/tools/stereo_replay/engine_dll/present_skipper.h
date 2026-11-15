/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_PRESENT_SKIPPER_H
#define STEREO_REPLAY_ENGINEDLL_PRESENT_SKIPPER_H

#include "replay/replay_engine_dll/i_call_interceptor.h"
#include "hurried_frame_selector.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Leaves out the calls that present a frame (a flip or a present) in the frames that HurriedFrameSelector says are hurried through.
             * These calls wait for the screen, and a hurried frame does not need the screen.
             */
            class PresentSkipper : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                explicit PresentSkipper(const HurriedFrameSelector* in_hurry_ptr);

                void on_call_replayed   (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame,
                                         const bool&                                  in_was_executed) override;
                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

            private:
                const HurriedFrameSelector* m_hurry_ptr;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_PRESENT_SKIPPER_H */
