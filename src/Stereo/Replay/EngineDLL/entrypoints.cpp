/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/Replay/EngineDLL/entrypoints.h"
#include "replay_engine.h"

Stereo::Replay::EngineDLL::IReplayEngine* Stereo::Replay::EngineDLL::aiCreateReplayEngine()
{
    return new ReplayEngine();
}

void Stereo::Replay::EngineDLL::aiDestroyReplayEngine(IReplayEngine* in_engine_ptr)
{
    delete in_engine_ptr;
}
