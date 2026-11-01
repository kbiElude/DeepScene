/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYPLAYER_STATE_NAMES_H
#define STEREOREPLAYPLAYER_STATE_NAMES_H

#include "Replay/StereoReplayEngineDLL/types.h"

namespace Replay
{
    namespace StereoReplayPlayer
    {
        /* How the front-ends of the player call the states of the playback. */
        class StateNames
        {
        public:
            static const char* get_state_name_ptr(const Replay::StereoReplayEngineDLL::PlaybackState& in_state);
        };
    }
}

#endif /* STEREOREPLAYPLAYER_STATE_NAMES_H */
