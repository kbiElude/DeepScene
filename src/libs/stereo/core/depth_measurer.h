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
        /* Does what DepthBufferStatistics::measure() does, with the same numbers as the result, in a fraction of the time on a processor that has AVX2 (and as it does on one that
         * has not): the distances of the pixels are put in a histogram, whose bins are the top bits of the distance as a float (which grow with the distance, so that the bins
         * are about one percent wide, whatever the distance is), the bins that hold the nearest, the median and the farthest distance are found from their counts, and the
         * three distances are chosen among those in the three bins only. What a measurer keeps between calls is the memory it works in, which is why a caller owns one.
         */
        class DepthMeasurer
        {
        public:
            /* in_allow_avx2 is false to have the measurer do it the way it is done without AVX2 (for the tests). */
            DepthMeasurer(const bool& in_allow_avx2);

            bool is_using_avx2() const;

            /* As DepthBufferStatistics::measure(); a step that is not 1 is done the way it is done without AVX2. */
            DepthRange measure(const float*  in_depth_ptr,
                               const size_t& in_n_depths,
                               const size_t& in_step,
                               const float&  in_projection_z_scale,
                               const float&  in_projection_z_shift,
                               const float&  in_fraction);

            /* True if the processor and the system have AVX2. */
            static bool is_avx2_supported();

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

            /* The pixel pass of measure_avx2(), in its own file, since that is built for AVX2 (and so are not to be the home of anything that the rest of the program may call).
             * It writes the distance of each pixel (0 for one that is not a distance) to out_distance_ptr, and counts the distances by bin in the histograms of in_layout, which
             * follow each other in out_histogram_ptr (the pixels that are next to each other go to different ones, so that those that are in one bin do not wait for each other).
             */
            static void fill_histogram_avx2(const float*                in_depth_ptr,
                                            const size_t&               in_n_depths,
                                            const float&                in_projection_z_scale,
                                            const float&                in_projection_z_shift,
                                            const DepthHistogramLayout& in_layout,
                                            float*                      out_distance_ptr,
                                            uint32_t*                   out_histogram_ptr,
                                            DepthPassResult*            out_result_ptr);

            static uint32_t get_key   (const float&        in_distance);
            static float    select_nth(std::vector<float>* io_distance_vec_ptr,
                                       const size_t&       in_index);

            std::vector<float>    m_distance_vec;
            std::vector<uint32_t> m_histogram_vec;
            bool                  m_is_using_avx2;
            std::vector<uint32_t> m_total_vec;

            /* The bin of a distance is what is left of its bits when KEY_SHIFT of them are left out, plus FIRST_DISTANCE_KEY: the bins before that are for what is not a distance,
             * one for each of the 8 pixels that the pixel pass takes together.
             */
            static const uint32_t FIRST_DISTANCE_KEY;
            static const uint32_t KEY_SHIFT;
            static const uint32_t N_BINS;
            static const uint32_t N_SUB_HISTOGRAMS;
        };
    }
}

#endif /* STEREO_CORE_DEPTH_MEASURER_H */
