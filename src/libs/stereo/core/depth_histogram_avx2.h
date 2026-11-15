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
        /* The pixel pass of FastDepthRangeMeasurer for a processor that has AVX2. It is a class of its own because its .cpp file is compiled with AVX2
         * enabled, so no code that the rest of the program may call can live there.
         */
        class DepthHistogramAvx2
        {
        public:
            /* Writes the distance of each pixel to out_distance_ptr (0 for a pixel without a distance). Counts the distances per bin in the
             * histograms described by in_layout. The histograms follow each other in out_histogram_ptr. Neighbouring pixels go to different
             * histograms, so that pixels that fall into the same bin do not wait for each other. Stores the range of used bins and the number
             * of sky pixels in out_result_ptr.
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
