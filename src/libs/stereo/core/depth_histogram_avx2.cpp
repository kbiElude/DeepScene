/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * This file is built for AVX2 (see the CMakeLists.txt of the library), so that nothing in it may be used on a processor that does not have it: DepthMeasurer calls it only after it
 * found out that the processor has AVX2. It is kept to what has to be here: the pixel pass, with no function of the standard library and none that is shared with other files
 * (such a function is built here with AVX2, and the linker may pick this copy of it for the callers that are not).
 */
#include "stereo/core/depth_measurer.h"
#include <float.h>
#include <immintrin.h>
#include <intrin.h>

void Stereo::Core::DepthMeasurer::fill_histogram_avx2(const float*                in_depth_ptr,
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

    /* The same arithmetic as DepthBufferStatistics::get_distance(), which is what the results must be the same as: the multiplication and the additions are not fused. */
    const __m256i all_ones_v        = _mm256_set1_epi32(-1);
    const __m256i first_key_v       = _mm256_set1_epi32(static_cast<int>(in_layout.first_distance_key) );
    const __m128i key_shift_v       = _mm_cvtsi32_si128(static_cast<int>(in_layout.key_shift) );
    /* The bin of what is not a distance, for each lane. */
    const __m256i lane_key_v        = _mm256_setr_epi32(0,
                                                        1,
                                                        2,
                                                        3,
                                                        4,
                                                        5,
                                                        6,
                                                        7);
    const __m256  max_float_v       = _mm256_set1_ps(FLT_MAX);
    const __m256  minus_one_v       = _mm256_set1_ps(-1.0f);
    const __m256  not_drawn_v       = _mm256_set1_ps(DepthBufferStatistics::NOT_DRAWN_DEPTH);
    const __m256  scale_v           = _mm256_set1_ps(in_projection_z_scale);
    const __m256  shift_v           = _mm256_set1_ps(in_projection_z_shift);
    const __m256  two_v             = _mm256_set1_ps(2.0f);
    const __m256  zero_v            = _mm256_setzero_ps();

    __m256i       max_key_v         = _mm256_setzero_si256();
    __m256i       min_key_v         = all_ones_v;
    size_t        n_depth           = 0;
    size_t        n_sky             = 0;

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
            /* The last few depths are padded with what is not a depth (it is not the sky either, and it gives no distance). */
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
            /* Nothing to divide or to count: all of it is the sky, which is what most of a buffer of a pass that drew little is. */
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

            /* A distance is above 0 and finite (a division by 0 gives infinity or nothing at all, which both fail this), and is not of the sky. */
            const __m256  is_distance_v      = _mm256_andnot_ps(is_sky_v,
                                                                _mm256_and_ps(_mm256_cmp_ps(distance_v,
                                                                                            zero_v,
                                                                                            _CMP_GT_OQ),
                                                                              _mm256_cmp_ps(distance_v,
                                                                                            max_float_v,
                                                                                            _CMP_LE_OQ) ) );
            const __m256i is_distance_mask_v = _mm256_castps_si256(is_distance_v);
            const __m256i distance_key_v     = _mm256_add_epi32(_mm256_srl_epi32(_mm256_castps_si256(distance_v),
                                                                                 key_shift_v),
                                                                first_key_v);
            const __m256i key_v              = _mm256_blendv_epi8(lane_key_v,
                                                                  distance_key_v,
                                                                  is_distance_mask_v);

            _mm256_storeu_ps(distance_ptr,
                             _mm256_and_ps(distance_v,
                                           is_distance_v) );
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(key_array),
                                key_v);

            max_key_v = _mm256_max_epu32(max_key_v,
                                         key_v);
            min_key_v = _mm256_min_epu32(min_key_v,
                                         _mm256_blendv_epi8(all_ones_v,
                                                            key_v,
                                                            is_distance_mask_v) );
            n_sky    += static_cast<size_t>(__popcnt(static_cast<unsigned int>(sky_mask) ) );

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

    /* The lowest and the highest bin that were used, of the lanes. */
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
