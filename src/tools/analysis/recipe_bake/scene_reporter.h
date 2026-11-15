/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The report that a person needs to tune the comfort of a recipe. For each scene (kind of frame) of the analysis it shows where the
 * frames of the scene are in the run (the cuts), how deep the scene is, where the eyes converge and how far apart they may be with the
 * recipe as it is, and the resulting parallax of its nearest and farthest surfaces, both as a fraction of the width of the picture and
 * as an angle on the screen of the headset.
 */
#ifndef RECIPEBAKE_SCENE_REPORTER_H
#define RECIPEBAKE_SCENE_REPORTER_H

#include "analysis/api_dump_analysis/model.h"
#include "analysis/recipe/types.h"
#include "utils/json/json_writer.h"
#include "types.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace RecipeBake
    {
        /* The frames of a scene that follow one another (a cut): from the first to the last frame index. */
        struct FrameRun
        {
            uint64_t first_frame_index = 0;
            uint64_t last_frame_index  = 0;
        };

        struct SceneReport
        {
            float                 far_parallax      = 0.0f; /* parallax of the farthest surfaces, as a fraction of the picture width */
            SceneLimits           limits;
            SceneMeasure          measure;
            float                 natural_separation = 0.0f; /* separation that the player's strength 1 asks for, before the limit of the scene    */
            float                 near_parallax     = 0.0f; /* parallax of the nearest surfaces, as for far_parallax                                */
            uint64_t              n_frames          = 0;
            std::vector<FrameRun> run_vec;
            float                 separation        = 0.0f; /* separation that the converter uses with the player's strength 1                     */
        };

        class SceneReporter
        {
        public:
            /* The report of each scene that has depth, in the order of the shapes. */
            static std::vector<SceneReport> find(const Analysis::Recipe::Recipe&                  in_recipe,
                                                 const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis);

            /* Writes the reports as JSON, with the keys as hex strings. The angles are for a screen that is this many degrees wide. */
            static void write_json(const std::vector<SceneReport>& in_report_vec,
                                   const float&                    in_screen_degrees,
                                   Utils::Json::JsonWriter*        inout_writer_ptr);
        };
    }
}

#endif /* RECIPEBAKE_SCENE_REPORTER_H */
