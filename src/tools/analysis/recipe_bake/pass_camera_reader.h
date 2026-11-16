/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the cameras that a pass of the analysis was drawn with.
 */
#ifndef RECIPEBAKE_PASS_CAMERA_READER_H
#define RECIPEBAKE_PASS_CAMERA_READER_H

#include "analysis/api_dump_analysis/model.h"
#include <array>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace RecipeBake
    {
        class PassCameraReader
        {
        public:
            /* The view matrices that the pass was seen with, in the order of its camera constants and of their samples. A pass that has no camera
             * has one all-zero matrix, which no rule of a recipe matches.
             */
            static std::vector<std::array<float, 16> > get_camera_vec(const Analysis::APIDumpAnalysis::AnalysisResult& in_analysis,
                                                                      const uint32_t&                                  in_pass_id);
        };
    }
}

#endif /* RECIPEBAKE_PASS_CAMERA_READER_H */
