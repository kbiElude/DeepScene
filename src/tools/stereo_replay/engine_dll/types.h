/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_TYPES_H
#define STEREO_REPLAY_ENGINEDLL_TYPES_H

#include "analysis/recipe/types.h"
#include "Replay/ReplayEngineDLL/types.h"
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

            typedef ::Replay::ReplayEngineDLL::FrameImage      FrameImage;
            typedef ::Replay::ReplayEngineDLL::PlaybackState   PlaybackState;
            typedef ::Replay::ReplayEngineDLL::PlaybackStatus  PlaybackStatus;
            typedef ::Replay::ReplayEngineDLL::WindowPlacement WindowPlacement;

            /* Applies to the frames first_frame to last_frame, counted from 0; the others are executed as they are read. */
            struct FrameExecutionOptions
            {
                Analysis::Recipe::Eye eye            = Analysis::Recipe::Eye::LEFT; /* BUFFERED_ONCE: the eye to render for; TWICE: the left one, then the right one */
                uint32_t       first_frame    = 0;
                uint32_t       last_frame     = UINT32_MAX;
                FrameExecution mode           = FrameExecution::DIRECT;
                bool           restores_state = false; /* TWICE: the state of the device is restored before the second execution */
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_TYPES_H */
