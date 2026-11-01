/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_PLAYER_STATE_NAMES_H
#define STEREO_REPLAY_PLAYER_STATE_NAMES_H

#include "Stereo/Replay/EngineDLL/types.h"

namespace Stereo
{
    namespace Replay
    {
        namespace Player
        {
            /* How the front-ends of the player call the states of the playback. */
            class StateNames
            {
            public:
                static const char* get_state_name_ptr(const Stereo::Replay::EngineDLL::PlaybackState& in_state);
            };
        }
    }
}

#endif /* STEREO_REPLAY_PLAYER_STATE_NAMES_H */
