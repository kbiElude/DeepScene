/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Replay/StereoReplayEngineDLL/entrypoints.h"
#include "replay_engine.h"

Replay::StereoReplayEngineDLL::IReplayEngine* Replay::StereoReplayEngineDLL::aiCreateReplayEngine()
{
    return new ReplayEngine();
}

void Replay::StereoReplayEngineDLL::aiDestroyReplayEngine(IReplayEngine* in_engine_ptr)
{
    delete in_engine_ptr;
}
