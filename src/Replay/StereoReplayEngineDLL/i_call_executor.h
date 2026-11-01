/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_I_CALL_EXECUTOR_H
#define STEREOREPLAYENGINEDLL_I_CALL_EXECUTOR_H

#include "call_walker.h"

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Replays the calls of one API group (see Capture::APIDumpLoader::APIGroup). */
        class ICallExecutor
        {
        public:
            virtual ~ICallExecutor() = default;

            /* Returns false (and says why) if the call cannot be replayed. The walker provides the call's data chunks. */
            virtual bool execute(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const CallWalker&                   in_walker,
                                          std::string*                        out_error_ptr) = 0;

            /* A frame of the workload has been replayed. */
            virtual void on_frame_presented() = 0;

            /* The replay is not running (paused, or at the end), and called now and then: a window needs its messages
             * answered all the time, or the system takes it for hung.
             */
            virtual void on_idle() { /* Stub */ }

            /* The replay goes back to the start: forget the objects which the replayed calls created. Windows which the
             * viewer sees may stay, to be used again.
             */
            virtual void reset() = 0;

            /* The replay is over: release everything. Unless the executor keeps something for reset(), that is the same. */
            virtual void shutdown() { reset(); }
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_I_CALL_EXECUTOR_H */
