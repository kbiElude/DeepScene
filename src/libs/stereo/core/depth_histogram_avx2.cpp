/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * This file is compiled with AVX2 (see the CMakeLists.txt of the library), so nothing in it may run on a processor without AVX2.
 * FastDepthRangeMeasurer calls it only after it has checked that the processor supports AVX2. The file holds only the pixel pass. It uses no
 * standard library functions and no functions shared with other files: such a function would be compiled here with AVX2, and the linker
 * could pick this copy for callers that run on any processor.
 */
#include "stereo/core/depth_histogram_avx2.h"
#include "stereo/core/depth_range_measurer.h"
#include <float.h>
#include <immintrin.h>
#include <intrin.h>

void Stereo::Core::DepthHistogramAvx2::fill(const float*                in_depth_ptr,
                                            const size_t&               in_n_depths,
                                            const float&                in_projection_z_scale,
                                            const float&                in_projection_z_shift,
                                            const DepthHistogramLayout& in_layout,
                                            float*                      out_distance_ptr,
                                            uint32_t*                   out_histogram_ptr,
                                            DepthPassResult*            out_result_ptr)
{
    constexpr int      ALL_LANES_MASK     = 0xFF;
    constexpr size_t   N_DEPTHS_PER_STEP  = 8;
    constexpr uint32_t QUIET_NAN_BITS     = 0x7FC00000;

    /* The arithmetic matches DepthRangeMeasurer::get_distance(), because the results must be identical: the multiplication and the
     * additions are not fused.
     */
    const __m256i all_ones_v  = _mm256_set1_epi32(-1);
    const __m256i first_key_v = _mm256_set1_epi32(static_cast<int>(in_layout.first_distance_key) );
    const __m128i key_shift_v = _mm_cvtsi32_si128(static_cast<int>(in_layout.key_shift) );

    /* A pixel without a distance goes to the bin with the number of its lane (0 to 7). */
    const __m256i lane_key_v  = _mm256_setr_epi32(0,
                                                  1,
                                                  2,
                                                  3,
                                                  4,
                                                  5,
                                                  6,
                                                  7);
    const __m256  max_float_v = _mm256_set1_ps(FLT_MAX);
    const __m256  minus_one_v = _mm256_set1_ps(-1.0f);
    const __m256  not_drawn_v = _mm256_set1_ps(DepthRangeMeasurer::NOT_DRAWN_DEPTH);
    const __m256  scale_v     = _mm256_set1_ps(in_projection_z_scale);
    const __m256  shift_v     = _mm256_set1_ps(in_projection_z_shift);
    const __m256  two_v       = _mm256_set1_ps(2.0f);
    const __m256  zero_v      = _mm256_setzero_ps();

    __m256i       max_key_v   = _mm256_setzero_si256();
    __m256i       min_key_v   = all_ones_v;
    size_t        n_depth     = 0;
    size_t        n_sky       = 0;

    while (n_depth < in_n_depths)
    {
        const size_t n_left = in_n_depths - n_depth;

        float    tail_depth_array   [N_DEPTHS_PER_STEP];
        float    tail_distance_array[N_DEPTHS_PER_STEP];
        uint32_t key_array          [N_DEPTHS_PER_STEP];

        const float* depth_ptr    = in_depth_ptr    + n_depth;
        float*       distance_ptr = out_distance_ptr + n_depth;

        if (n_left < N_DEPTHS_PER_STEP)
        {
            /* The last few depths are padded with a quiet NaN. It is not sky, and it gives no distance. */
            _mm256_storeu_ps(tail_depth_array,
                             _mm256_castsi256_ps(_mm256_set1_epi32(static_cast<int>(QUIET_NAN_BITS) ) ) );

            for (size_t n_tail = 0;
                        n_tail < n_left;
                        n_tail++)
            {
                tail_depth_array[n_tail] = depth_ptr[n_tail];
            }

            depth_ptr    = tail_depth_array;
            distance_ptr = tail_distance_array;
        }

        const __m256 depth_v    = _mm256_loadu_ps(depth_ptr);
        const __m256 is_sky_v   = _mm256_cmp_ps(depth_v,
                                                not_drawn_v,
                                                _CMP_GE_OQ);
        const int    sky_mask   = _mm256_movemask_ps(is_sky_v);

        if (sky_mask == ALL_LANES_MASK)
        {
            /* All 8 pixels are sky, so there is nothing to divide or count. This is the common case for a pass that drew little. */
            n_sky += N_DEPTHS_PER_STEP;

            _mm256_storeu_ps(distance_ptr,
                             zero_v);
        }
        else
        {
            const __m256 divisor_v  = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(depth_v,
                                                                                two_v),
                                                                  minus_one_v),
                                                    scale_v);
            const __m256 distance_v = _mm256_div_ps(shift_v,
                                                    divisor_v);

            /* A pixel has a distance if the result is above 0 and finite and the pixel is not sky. A division by 0 gives infinity or NaN,
             * and both fail the comparisons.
             */
            const __m256  is_distance_v      = _mm256_andnot_ps   (is_sky_v,
                                                                   _mm256_and_ps   (_mm256_cmp_ps      (distance_v,
                                                                                                        zero_v,
                                                                                                        _CMP_GT_OQ),
                                                                                    _mm256_cmp_ps      (distance_v,
                                                                                                        max_float_v,
                                                                                                        _CMP_LE_OQ) ) );
            const __m256i is_distance_mask_v = _mm256_castps_si256(is_distance_v);
            const __m256i distance_key_v     = _mm256_add_epi32   (_mm256_srl_epi32(_mm256_castps_si256(distance_v),
                                                                                    key_shift_v),
                                                                   first_key_v);
            const __m256i key_v              = _mm256_blendv_epi8 (lane_key_v,
                                                                   distance_key_v,
                                                                   is_distance_mask_v);

            _mm256_storeu_ps   (distance_ptr,
                                _mm256_and_ps(distance_v,
                                              is_distance_v) );
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(key_array),
                                key_v);

            max_key_v = _mm256_max_epu32   (max_key_v,
                                            key_v);
            min_key_v = _mm256_min_epu32   (min_key_v,
                                            _mm256_blendv_epi8(all_ones_v,
                                                               key_v,
                                                               is_distance_mask_v) );
            n_sky    += static_cast<size_t>(__popcnt          (static_cast<unsigned int>(sky_mask) ) );

            for (size_t n_lane = 0;
                        n_lane < N_DEPTHS_PER_STEP;
                        n_lane++)
            {
                out_histogram_ptr[(n_lane & (in_layout.n_sub_histograms - 1)) * in_layout.n_bins + key_array[n_lane] ]++;
            }
        }

        if (n_left < N_DEPTHS_PER_STEP)
        {
            for (size_t n_tail = 0;
                        n_tail < n_left;
                        n_tail++)
            {
                out_distance_ptr[n_depth + n_tail] = distance_ptr[n_tail];
            }
        }

        n_depth += N_DEPTHS_PER_STEP;
    }

    /* Reduces the per-lane lowest and highest bins to the overall range. */
    uint32_t max_key_array[N_DEPTHS_PER_STEP];
    uint32_t min_key_array[N_DEPTHS_PER_STEP];

    _mm256_storeu_si256(reinterpret_cast<__m256i*>(max_key_array),
                        max_key_v);
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(min_key_array),
                        min_key_v);

    out_result_ptr->key_range.max_key = 0;
    out_result_ptr->key_range.min_key = UINT32_MAX;

    for (size_t n_lane = 0;
                n_lane < N_DEPTHS_PER_STEP;
                n_lane++)
    {
        if (max_key_array[n_lane] > out_result_ptr->key_range.max_key)
        {
            out_result_ptr->key_range.max_key = max_key_array[n_lane];
        }

        if (min_key_array[n_lane] < out_result_ptr->key_range.min_key)
        {
            out_result_ptr->key_range.min_key = min_key_array[n_lane];
        }
    }

    out_result_ptr->n_sky = n_sky;
}
