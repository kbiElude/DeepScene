/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_I_REPLAY_OBSERVER_H
#define STEREO_REPLAY_ENGINEDLL_I_REPLAY_OBSERVER_H

#include "Replay/ReplayEngineDLL/i_replay_observer.h"
#include "Stereo/Replay/EngineDLL/types.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Watches a replay (see the one of the replay engine of the interceptor). */
            typedef ::Replay::ReplayEngineDLL::IReplayObserver IReplayObserver;
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_I_REPLAY_OBSERVER_H */
