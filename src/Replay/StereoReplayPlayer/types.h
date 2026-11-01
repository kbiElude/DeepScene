/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the player.
 */
#ifndef STEREOREPLAYPLAYER_TYPES_H
#define STEREOREPLAYPLAYER_TYPES_H

#include <stdint.h>
#include <string>

namespace Replay
{
    namespace StereoReplayPlayer
    {
        /* The eyes the frames are rendered for, when a recipe is played. */
        enum class EyeMode
        {
            BOTH,
            LEFT,
            RIGHT
        };

        struct Options
        {
            EyeMode     eye_mode            = EyeMode::BOTH;
            float       eye_separation      = 0.0f;  /* if has_eye_separation */
            uint32_t    first_frame         = 0;
            bool        has_eye_mode        = false;
            bool        has_eye_separation  = false;
            bool        has_frame_range     = false;
            bool        is_console          = false;
            uint32_t    last_frame          = 0;
            std::string recipe_filename;
            std::string workload_path;
        };
    }
}

#endif /* STEREOREPLAYPLAYER_TYPES_H */
