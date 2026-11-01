/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_TYPES_H
#define STEREOREPLAYENGINEDLL_TYPES_H

#include <stdint.h>
#include <string>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        enum class PlaybackState
        {
            FAILED,   /* a call could not be replayed; reset() starts over                     */
            FINISHED, /* every call has been replayed; reset() starts over                     */
            IDLE,     /* nothing has been replayed yet (before the first play())               */
            PAUSED,   /* stopped at a frame boundary, also at the first frame after reset()    */
            PLAYING
        };

        struct PlaybackStatus
        {
            std::string   error_message;       /* set in the Failed state */
            uint64_t      n_calls_executed   = 0;
            uint32_t      n_frames_presented = 0;
            PlaybackState state              = PlaybackState::IDLE;
        };

        /* Where the replay put one of its windows: the outer rectangle, in screen coordinates. */
        struct WindowPlacement
        {
            int32_t height = 0;
            int32_t width  = 0;
            int32_t x      = 0;
            int32_t y      = 0;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_TYPES_H */
