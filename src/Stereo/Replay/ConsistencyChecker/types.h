/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the replay check tool.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_TYPES_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_TYPES_H

#include "Analysis/RecipeLoader/types.h"
#include "Stereo/Replay/EngineDLL/types.h"
#include "Stereo/Replay/FrameImages/types.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            /* What the tool exits with. */
            enum class ExitCode
            {
                BAD_ARGUMENTS     = 1,
                DIFFERENCES_FOUND = 3, /* the last way of replaying gave other images than the frames as they are */
                REPLAY_FAILED     = 2,
                SUCCESS           = 0
            };

            struct Options
            {
                uint32_t                             contact_sheet_n_frames  = 0;          /* the frames to show in the contact sheet; 0: no sheet */
                std::string                          dump_directory;
                uint32_t                             dump_frame              = UINT32_MAX; /* the images of this frame are written whether or not it differs */
                float                                eye_separation          = 0.0f;
                Stereo::Replay::FrameImages::EyeView eye_view                = Stereo::Replay::FrameImages::EyeView::BOTH;
                uint32_t                             first_frame             = 0;
                bool                                 has_eye_view            = false;      /* the view of the eyes of a screenshot or a sheet is the one that was asked for */
                std::string                          image_filename;                       /* the PNG file of a screenshot or a contact sheet */
                bool                                 is_parallel             = false;      /* the ways are replayed at the same time */
                uint32_t                             last_frame              = 999;
                Analysis::RecipeLoader::Recipe       recipe;
                std::string                          recipe_filename;
                uint32_t                             screenshot_frame        = UINT32_MAX; /* the frame to write the image of: the replay of the workload stops at it */
                std::vector<std::string>             way_name_vec;                         /* the ways to replay besides the frames as they are; none: the usual ones */
                std::string                          workload_filename;
            };

            /* One way of replaying the frames of the check. */
            struct Variant
            {
                Stereo::Replay::EngineDLL::FrameExecution mode;
                const char*                               name_ptr;
                bool                                      restores_state;
                bool                                      uses_recipe;
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_TYPES_H */
