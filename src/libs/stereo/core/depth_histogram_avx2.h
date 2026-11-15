/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_DEPTH_HISTOGRAM_AVX2_H
#define STEREO_CORE_DEPTH_HISTOGRAM_AVX2_H

#include "stereo/core/types.h"
#include <stddef.h>
#include <stdint.h>

namespace Stereo
{
    namespace Core
    {
        /* The pixel pass of DepthMeasurer for a processor that has AVX2. It is a class of its own because its file is built for AVX2, and so is not to be the home of anything that
         * the rest of the program may call.
         */
        class DepthHistogramAvx2
        {
        public:
            /* Writes the distance of each pixel (0 for one that is not a distance) to out_distance_ptr, and counts the distances by bin in the histograms of in_layout, which follow
             * each other in out_histogram_ptr (the pixels that are next to each other go to different ones, so that those that are in one bin do not wait for each other).
             */
            static void fill(const float*                in_depth_ptr,
                             const size_t&               in_n_depths,
                             const float&                in_projection_z_scale,
                             const float&                in_projection_z_shift,
                             const DepthHistogramLayout& in_layout,
                             float*                      out_distance_ptr,
                             uint32_t*                   out_histogram_ptr,
                             DepthPassResult*            out_result_ptr);
        };
    }
}

#endif /* STEREO_CORE_DEPTH_HISTOGRAM_AVX2_H */
