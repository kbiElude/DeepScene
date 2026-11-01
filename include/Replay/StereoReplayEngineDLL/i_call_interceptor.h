/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_I_CALL_INTERCEPTOR_H
#define STEREOREPLAYENGINEDLL_I_CALL_INTERCEPTOR_H

#include "Capture/APIDumpLoader/APIDumpLoader.h"

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Sees every call of the workload around its replay: the hook for analysis and for tools built on the replayer.
         * Called from the replay thread. The call is valid for the duration of the function only.
         */
        class ICallInterceptor
        {
        public:
            virtual ~ICallInterceptor() = default;

            /* Called after the call was replayed, or skipped. */
            virtual void on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                   const uint32_t&                     in_n_frame,
                                                   const bool&                         in_was_executed) = 0;

            /* Returns false to skip the call. The call is skipped if any interceptor says so. */
            virtual bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                      const uint32_t&                     in_n_frame) = 0;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_I_CALL_INTERCEPTOR_H */
