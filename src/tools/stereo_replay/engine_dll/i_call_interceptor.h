/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_CALL_INTERCEPTOR_H
#define STEREO_REPLAY_ENGINEDLL_I_CALL_INTERCEPTOR_H

#include "replay/replay_engine_dll/i_call_interceptor.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Sees every call of the workload before and after it is replayed (see ::Replay::ReplayEngineDLL::ICallInterceptor). */
            typedef ::Replay::ReplayEngineDLL::ICallInterceptor ICallInterceptor;
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_I_CALL_INTERCEPTOR_H */
