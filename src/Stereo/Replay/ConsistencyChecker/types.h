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
#include <stdint.h>
#include <string>

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
                std::string                    dump_directory;
                float                          eye_separation = 0.0f;
                uint32_t                       first_frame = 0;
                uint32_t                       last_frame  = 999;
                Analysis::RecipeLoader::Recipe recipe;
                std::string                    recipe_filename;
                std::string                    workload_filename;
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
