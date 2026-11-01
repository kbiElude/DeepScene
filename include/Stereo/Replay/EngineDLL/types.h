/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_TYPES_H
#define STEREO_REPLAY_ENGINEDLL_TYPES_H

#include "Analysis/RecipeLoader/types.h"
#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* How the replay executes the calls of a frame. */
            enum class FrameExecution
            {
                BUFFERED_ONCE, /* the calls are read up to the present, then executed once  */
                DIRECT,        /* each call is executed as it is read                       */
                TWICE          /* the calls are read up to the present, then executed twice */
            };

            /* Applies to the frames first_frame to last_frame, counted from 0; the others are executed as they are read. */
            struct FrameExecutionOptions
            {
                Analysis::RecipeLoader::Eye eye            = Analysis::RecipeLoader::Eye::LEFT; /* BUFFERED_ONCE: the eye to render for; TWICE: the left one, then the right one */
                uint32_t       first_frame    = 0;
                uint32_t       last_frame     = UINT32_MAX;
                FrameExecution mode           = FrameExecution::DIRECT;
                bool           restores_state = false; /* TWICE: the state of the device is restored before the second execution */
            };

            /* The back buffer of the device, as B, G, R and A (always 255) of 8 bits for each pixel, rows one after the other. */
            struct FrameImage
            {
                uint32_t       height    = 0;
                const uint8_t* pixel_ptr = nullptr; /* valid only while the sink is called */
                uint32_t       width     = 0;
            };

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
}

#endif /* STEREO_REPLAY_ENGINEDLL_TYPES_H */
