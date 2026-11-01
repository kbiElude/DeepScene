/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "state_names.h"
#include "Common/macros.h"

const char* Stereo::Replay::Player::StateNames::get_state_name_ptr(const Stereo::Replay::EngineDLL::PlaybackState& in_state)
{
    const char* result_ptr = "unknown";

    switch (in_state)
    {
        case Stereo::Replay::EngineDLL::PlaybackState::FAILED:
        {
            result_ptr = "failed";

            break;
        }

        case Stereo::Replay::EngineDLL::PlaybackState::FINISHED:
        {
            result_ptr = "finished";

            break;
        }

        case Stereo::Replay::EngineDLL::PlaybackState::IDLE:
        {
            result_ptr = "idle";

            break;
        }

        case Stereo::Replay::EngineDLL::PlaybackState::PAUSED:
        {
            result_ptr = "paused";

            break;
        }

        case Stereo::Replay::EngineDLL::PlaybackState::PLAYING:
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
