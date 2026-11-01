/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "state_names.h"
#include "Common/macros.h"

const char* Replay::StereoReplayPlayer::StateNames::get_state_name_ptr(const Replay::StereoReplayEngineDLL::PlaybackState& in_state)
{
    const char* result_ptr = "unknown";

    switch (in_state)
    {
        case Replay::StereoReplayEngineDLL::PlaybackState::FAILED:
        {
            result_ptr = "failed";

            break;
        }

        case Replay::StereoReplayEngineDLL::PlaybackState::FINISHED:
        {
            result_ptr = "finished";

            break;
        }

        case Replay::StereoReplayEngineDLL::PlaybackState::IDLE:
        {
            result_ptr = "idle";

            break;
        }

        case Replay::StereoReplayEngineDLL::PlaybackState::PAUSED:
        {
            result_ptr = "paused";

            break;
        }

        case Replay::StereoReplayEngineDLL::PlaybackState::PLAYING:
        {
            result_ptr = "playing";

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_state);

            break;
        }
    }

    return result_ptr;
}
