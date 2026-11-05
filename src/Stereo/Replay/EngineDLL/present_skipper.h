/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_PRESENT_SKIPPER_H
#define STEREO_REPLAY_ENGINEDLL_PRESENT_SKIPPER_H

#include "Replay/ReplayEngineDLL/i_call_interceptor.h"
#include "frame_hurry.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Leaves out the calls that present a frame (a flip, a present: they wait for the screen, and the screen is not what a hurry is for)
             * in the frames that FrameHurry says are hurried through.
             */
            class PresentSkipper : public ::Replay::ReplayEngineDLL::ICallInterceptor
            {
            public:
                explicit PresentSkipper(const FrameHurry* in_hurry_ptr);

                void on_call_replayed (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                       const uint32_t&                              in_n_frame,
                                       const bool&                                  in_was_executed) override;
                bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const uint32_t&                              in_n_frame) override;

            private:
                const FrameHurry* m_hurry_ptr;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_PRESENT_SKIPPER_H */
