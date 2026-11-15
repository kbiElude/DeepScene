/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_DEPTH_MEASURER_H
#define STEREO_CORE_DEPTH_MEASURER_H

#include "stereo/core/depth_buffer_statistics.h"
#include "stereo/core/types.h"
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Core
    {
        /* Does what DepthBufferStatistics::measure() does and returns the same numbers, in a fraction of the time on a processor with AVX2.
         * On a processor without AVX2 it calls that function. The AVX2 path works as follows. It puts the distances of the pixels in a
         * histogram. A bin is selected by the top bits of the distance as a float. These bits grow with the distance, so every bin is about
         * one percent wide, whatever the distance is. The bin counts show which bins hold the nearest, the median and the farthest distance.
         * The three distances are then selected among the values in those three bins only. A measurer keeps its working memory between
         * calls, so each caller owns one.
         */
        class DepthMeasurer
        {
        public:
            /* Pass false as in_allow_avx2 to force the path without AVX2 (for the tests). */
            DepthMeasurer(const bool& in_allow_avx2);

            bool is_using_avx2() const;

            /* Same as DepthBufferStatistics::measure(). A step above 1 uses the path without AVX2. */
            DepthRange measure(const float*  in_depth_ptr,
                               const size_t& in_n_depths,
                               const size_t& in_step,
                               const float&  in_projection_z_scale,
                               const float&  in_projection_z_shift,
                               const float&  in_fraction);

        private:
            void       clear_histogram(const DepthKeyRange&  in_key_range);
            uint32_t   locate_key     (const DepthKeyRange&  in_key_range,
                                       const size_t&         in_rank,
                                       size_t*               out_n_before_ptr) const;
            DepthRange measure_avx2   (const float*          in_depth_ptr,
                                       const size_t&         in_n_depths,
                                       const float&          in_projection_z_scale,
                                       const float&          in_projection_z_shift,
                                       const float&          in_fraction);
            size_t     merge_histogram(const DepthKeyRange&  in_key_range);
            DepthRange select_depths  (const DepthSelection& in_selection,
                                       const size_t&         in_n_drawn,
                                       const DepthKeyRange&  in_key_range) const;

            static uint32_t get_key   (const float&        in_distance);
            static float    select_nth(std::vector<float>* io_distance_vec_ptr,
                                       const size_t&       in_index);

            std::vector<float>    m_distance_vec;
            std::vector<uint32_t> m_histogram_vec;
            bool                  m_is_using_avx2;
            std::vector<uint32_t> m_total_vec;

            /* The bin of a distance is its float bits shifted right by KEY_SHIFT, plus FIRST_DISTANCE_KEY. The bins below FIRST_DISTANCE_KEY
             * count pixels without a distance, one bin for each of the 8 pixels that the pixel pass handles together.
             */
            static const uint32_t FIRST_DISTANCE_KEY;
            static const uint32_t KEY_SHIFT;
            static const uint32_t N_BINS;
            static const uint32_t N_SUB_HISTOGRAMS;
        };
    }
}

#endif /* STEREO_CORE_DEPTH_MEASURER_H */
