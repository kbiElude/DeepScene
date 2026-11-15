/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/depth_measurer.h"
#include "stereo/core/depth_histogram_avx2.h"
#include "utils/cpu/cpu_features.h"
#include <algorithm>
#include <string.h>

const uint32_t Stereo::Core::DepthMeasurer::FIRST_DISTANCE_KEY = 8;
const uint32_t Stereo::Core::DepthMeasurer::KEY_SHIFT        = 17; /* a bin per 6 bits of the mantissa: about 1.1 percent of the distance */
const uint32_t Stereo::Core::DepthMeasurer::N_BINS           = 1u << (32 - 1 - KEY_SHIFT);
const uint32_t Stereo::Core::DepthMeasurer::N_SUB_HISTOGRAMS = 4;

Stereo::Core::DepthMeasurer::DepthMeasurer(const bool& in_allow_avx2)
    :m_distance_vec  (),
     m_histogram_vec (),
     m_is_using_avx2 (false),
     m_total_vec     ()
{
    const bool is_supported = Utils::Cpu::CpuFeatures::is_avx2_supported();

    m_is_using_avx2 = (in_allow_avx2 && is_supported);

    if (m_is_using_avx2)
    {
        /* The histograms are all zero between the calls: a call clears the bins that it used. */
        m_histogram_vec.assign(static_cast<size_t>(N_SUB_HISTOGRAMS) * N_BINS,
                               0);
        m_total_vec.assign    (N_BINS,
                               0);
    }
}

void Stereo::Core::DepthMeasurer::clear_histogram(const DepthKeyRange& in_key_range)
{
    for (uint32_t n_sub_histogram = 0;
                  n_sub_histogram < N_SUB_HISTOGRAMS;
                  n_sub_histogram++)
    {
        uint32_t* const histogram_ptr = m_histogram_vec.data() + static_cast<size_t>(n_sub_histogram) * N_BINS;

        /* The bins before the first for a distance count what was not one, and are outside the range. */
        std::fill(histogram_ptr,
                  histogram_ptr + FIRST_DISTANCE_KEY,
                  0u);

        if (in_key_range.min_key <= in_key_range.max_key)
        {
            std::fill(histogram_ptr + in_key_range.min_key,
                      histogram_ptr + in_key_range.max_key + 1,
                      0u);
        }
    }
}

uint32_t Stereo::Core::DepthMeasurer::get_key(const float& in_distance)
{
    uint32_t bits = 0;

    ::memcpy(&bits,
             &in_distance,
             sizeof(bits) );

    /* The bits of a positive float grow with it. */
    return (bits >> KEY_SHIFT) + FIRST_DISTANCE_KEY;
}

bool Stereo::Core::DepthMeasurer::is_using_avx2() const
{
    return m_is_using_avx2;
}

uint32_t Stereo::Core::DepthMeasurer::locate_key(const DepthKeyRange& in_key_range,
                                                 const size_t&        in_rank,
                                                 size_t*              out_n_before_ptr) const
{
    size_t   n_before = 0;
    uint32_t result   = in_key_range.min_key;

    for (;
         result < in_key_range.max_key;
         result++)
    {
        if (n_before + m_total_vec.at(result) > in_rank)
        {
            break;
        }

        n_before += m_total_vec.at(result);
    }

    *out_n_before_ptr = n_before;

    return result;
}

Stereo::Core::DepthRange Stereo::Core::DepthMeasurer::measure(const float*  in_depth_ptr,
                                                              const size_t& in_n_depths,
                                                              const size_t& in_step,
                                                              const float&  in_projection_z_scale,
                                                              const float&  in_projection_z_shift,
                                                              const float&  in_fraction)
{
    DepthRange result;

    if (m_is_using_avx2   &&
        in_step <= 1      &&
        in_n_depths > 0)
    {
        result = measure_avx2(in_depth_ptr,
                              in_n_depths,
                              in_projection_z_scale,
                              in_projection_z_shift,
                              in_fraction);
    }
    else
    {
        result = DepthBufferStatistics::measure(in_depth_ptr,
                                                in_n_depths,
                                                in_step,
                                                in_projection_z_scale,
                                                in_projection_z_shift,
                                                in_fraction);
    }

    return result;
}

Stereo::Core::DepthRange Stereo::Core::DepthMeasurer::measure_avx2(const float*  in_depth_ptr,
                                                                   const size_t& in_n_depths,
                                                                   const float&  in_projection_z_scale,
                                                                   const float&  in_projection_z_shift,
                                                                   const float&  in_fraction)
{
    DepthHistogramLayout layout;
    DepthPassResult      pass_result;
    DepthRange           result;

    layout.first_distance_key = FIRST_DISTANCE_KEY;
    layout.key_shift        = KEY_SHIFT;
    layout.n_bins           = N_BINS;
    layout.n_sub_histograms = N_SUB_HISTOGRAMS;

    m_distance_vec.resize(in_n_depths);

    DepthHistogramAvx2::fill(in_depth_ptr,
                             in_n_depths,
                             in_projection_z_scale,
                             in_projection_z_shift,
                             layout,
                             m_distance_vec.data(),
                             m_histogram_vec.data(),
                             &pass_result);

    const DepthKeyRange& key_range = pass_result.key_range;
    const size_t         n_drawn   = (key_range.min_key <= key_range.max_key) ? merge_histogram(key_range)
                                                                              : 0;
    const DepthSelection selection = DepthBufferStatistics::get_selection(n_drawn,
                                                                          in_n_depths,
                                                                          pass_result.n_sky,
                                                                          in_projection_z_scale,
                                                                          in_projection_z_shift,
                                                                          in_fraction);

    if (selection.is_enough)
    {
        result = select_depths(selection,
                               n_drawn,
                               key_range);
    }

    clear_histogram(key_range);

    return result;
}

size_t Stereo::Core::DepthMeasurer::merge_histogram(const DepthKeyRange& in_key_range)
{
    size_t result = 0;

    for (uint32_t key = in_key_range.min_key;
                  key <= in_key_range.max_key;
                  key++)
    {
        uint32_t total = 0;

        for (uint32_t n_sub_histogram = 0;
                      n_sub_histogram < N_SUB_HISTOGRAMS;
                      n_sub_histogram++)
        {
            total += m_histogram_vec.at(static_cast<size_t>(n_sub_histogram) * N_BINS + key);
        }

        m_total_vec.at(key) = total;
        result             += total;
    }

    return result;
}

Stereo::Core::DepthRange Stereo::Core::DepthMeasurer::select_depths(const DepthSelection& in_selection,
                                                                    const size_t&         in_n_drawn,
                                                                    const DepthKeyRange&  in_key_range) const
{
    const size_t   rank_far    = in_n_drawn - 1 - in_selection.n_edge;
    const size_t   rank_median = in_n_drawn / 2;
    const size_t   rank_near   = in_selection.n_edge;

    size_t         n_before_far    = 0;
    size_t         n_before_median = 0;
    size_t         n_before_near   = 0;
    DepthRange     result;

    const uint32_t key_far    = locate_key(in_key_range,
                                           rank_far,
                                           &n_before_far);
    const uint32_t key_median = locate_key(in_key_range,
                                           rank_median,
                                           &n_before_median);
    const uint32_t key_near   = locate_key(in_key_range,
                                           rank_near,
                                           &n_before_near);

    /* The distances of the three bins: the one of each rank is among them. */
    std::vector<float> far_vec;
    std::vector<float> median_vec;
    std::vector<float> near_vec;

    for (const float distance : m_distance_vec)
    {
        if (distance > 0.0f)
        {
            const uint32_t key = get_key(distance);

            if (key == key_far)
            {
                far_vec.push_back(distance);
            }

            if (key == key_median)
            {
                median_vec.push_back(distance);
            }

            if (key == key_near)
            {
                near_vec.push_back(distance);
            }
        }
    }

    result.median_depth = select_nth(&median_vec,
                                     rank_median - n_before_median);
    result.near_depth   = select_nth(&near_vec,
                                     rank_near - n_before_near);

    if (in_selection.has_sky)
    {
        result.far_depth = in_selection.far_plane;
    }
    else
    {
        result.far_depth = select_nth(&far_vec,
                                      rank_far - n_before_far);
    }

    result.has_depth = (result.far_depth > result.near_depth);

    return result;
}

float Stereo::Core::DepthMeasurer::select_nth(std::vector<float>* io_distance_vec_ptr,
                                              const size_t&       in_index)
{
    std::nth_element(io_distance_vec_ptr->begin(),
                     io_distance_vec_ptr->begin() + static_cast<std::ptrdiff_t>(in_index),
                     io_distance_vec_ptr->end  () );

    return io_distance_vec_ptr->at(in_index);
}
