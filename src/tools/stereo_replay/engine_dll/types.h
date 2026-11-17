/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_TYPES_H
#define STEREO_REPLAY_ENGINEDLL_TYPES_H

#include "analysis/recipe/types.h"
#include "replay/replay_engine_dll/types.h"
#include <functional>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Which variant of a matrix is in the device, as far as the binder knows. */
            enum class EyeMatrixBinding
            {
                LEFT,
                MONO,
                NOTHING, /* the call has not been executed since it was kept */
                RIGHT
            };

            /* How the replay executes the calls of a frame. */
            enum class FrameExecution
            {
                BUFFERED_ONCE, /* the calls are read up to the present, then executed once  */
                DIRECT,        /* each call is executed as it is read                       */
                TWICE,         /* the calls are read up to the present, then executed twice */
                VARIANTS       /* the frames before the last are executed as in TWICE; the last frame is executed once per variant, for the left eye */
            };

            /* One way of playing the last frame in the VARIANTS mode: which draws are left out in addition to those the recipe leaves
             * out, and where the frame is cut.
             */
            struct FrameVariant
            {
                std::vector<Analysis::Recipe::PassDraw> skipped_pass_draw_vec;
                std::vector<uint64_t>                   skipped_pass_key_vec;  /* the draws of these passes are all left out */
                uint64_t                                truncate_draw_key    = 0;     /* truncates_after_draw: the key of the structure of the draw */
                uint64_t                                truncate_pass_key    = 0;     /* truncates_after_draw: the key of the pass that it is in */
                bool                                    truncates_after_draw = false; /* the frame is cut after the last call of the draws with that structure in that pass */
            };

            typedef ::Replay::ReplayEngineDLL::FrameImage      FrameImage;
            typedef std::function<bool(const uint64_t&)>        FrameLiveFunc; /* whether the frame is live: the slice needs its content */
            typedef ::Replay::ReplayEngineDLL::PlaybackState   PlaybackState;
            typedef ::Replay::ReplayEngineDLL::PlaybackStatus  PlaybackStatus;
            typedef ::Replay::ReplayEngineDLL::WindowPlacement WindowPlacement;

            /* Applies to the frames first_frame to last_frame (counted from 0). The other frames are executed as they are read. */
            struct FrameExecutionOptions
            {
                Analysis::Recipe::Eye eye            = Analysis::Recipe::Eye::LEFT; /* BUFFERED_ONCE: the eye to render for; TWICE: the left one, then the right one */
                uint32_t              first_frame    = 0;
                uint32_t              last_frame     = UINT32_MAX;
                FrameExecution        mode           = FrameExecution::DIRECT;
                bool                  restores_state = false;                       /* TWICE: the state of the device is restored before the second execution */
                uint64_t              truncate_draw_key = 0;                        /* truncates_after_draw: the key of the structure of the draw */
                uint64_t              truncate_pass_key = 0;                        /* truncates_after_draw: the key of the pass that it is in */
                bool                  truncates_after_draw = false;                 /* the last frame (last_frame) is cut after the last call of the draws with that structure in that pass. The
                                                                                      * image is what has been drawn up to that point; it shows whether those draws draw anything and whether later draws cover them */
                std::vector<FrameVariant> variant_vec;                              /* VARIANTS: the ways to play the last frame. The Nth execution produces the image of the Nth variant. The state of
                                                                                      * the device is restored between executions if restores_state is set */
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_TYPES_H */
