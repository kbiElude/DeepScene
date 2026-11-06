/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "entrypoints.h"
#include "stereo_replay_engine.h"

Stereo::Replay::EngineDLL::IReplayEngine* Stereo::Replay::EngineDLL::aiCreateReplayEngine()
{
    return new StereoReplayEngine();
}

void Stereo::Replay::EngineDLL::aiDestroyReplayEngine(IReplayEngine* in_engine_ptr)
{
    delete in_engine_ptr;
}
