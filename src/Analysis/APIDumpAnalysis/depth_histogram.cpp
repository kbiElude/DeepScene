/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/depth_histogram.h"
#include <math.h>

/* Ranges for each doubling of the depth, from 1 / 128 to 2 ^ 17 (more than 130000), in the units of the camera. */
constexpr uint32_t BINS_PER_OCTAVE = 8;
constexpr int32_t  FIRST_OCTAVE    = -7;
constexpr uint32_t N_OCTAVES       = 24;
constexpr uint32_t N_BINS          = BINS_PER_OCTAVE * N_OCTAVES;


Analysis::APIDumpAnalysis::DepthHistogram::DepthHistogram()
    :m_weight_vec(N_BINS,
                  0)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::DepthHistogram::add(const float&    in_depth,
                                                    const uint64_t& in_weight)
{
    const bool is_finite = ::isfinite(in_depth);

    /* Not a depth that is in front of the camera, or nearer than the first range, is of a draw to converge on. */
    if (is_finite)
    {
        const float first_depth = ::ldexpf(1.0f,
                                           FIRST_OCTAVE);

        if (in_depth > first_depth)
        {
            const double position = (::log2(static_cast<double>(in_depth) ) - FIRST_OCTAVE) * BINS_PER_OCTAVE;
            const auto   bin      = (position >= static_cast<double>(N_BINS) ) ? N_BINS - 1
                                                                               : static_cast<uint32_t>(position);

            m_weight_vec.at(bin) += in_weight;
        }
    }
}

float Analysis::APIDumpAnalysis::DepthHistogram::get_bin_depth(const uint32_t& in_bin)
{
    return static_cast<float>(::exp2(FIRST_OCTAVE + (static_cast<double>(in_bin) + 0.5) / BINS_PER_OCTAVE) );
}

float Analysis::APIDumpAnalysis::DepthHistogram::get_depth_at_fraction(const double& in_fraction) const
{
    float          result = 0.0f;
    const uint64_t total  = get_total_weight();

    if (total != 0)
    {
        const double wanted      = in_fraction * static_cast<double>(total);
        double       accumulated = 0.0;

        for (uint32_t n_bin = 0;
                      n_bin < N_BINS;
                    ++n_bin)
        {
            accumulated += static_cast<double>(m_weight_vec.at(n_bin) );

            if (accumulated            >= wanted &&
                m_weight_vec.at(n_bin) != 0)
            {
                result = get_bin_depth(n_bin);

                break;
            }
        }
    }

    return result;
}

uint32_t Analysis::APIDumpAnalysis::DepthHistogram::get_n_bins()
{
    return N_BINS;
}

uint64_t Analysis::APIDumpAnalysis::DepthHistogram::get_total_weight() const
{
    uint64_t result = 0;

    for (const auto& weight : m_weight_vec)
    {
        result += weight;
    }

    return result;
}

uint64_t Analysis::APIDumpAnalysis::DepthHistogram::get_weight(const uint32_t& in_bin) const
{
    return (in_bin < N_BINS) ? m_weight_vec.at(in_bin)
                             : 0;
}

bool Analysis::APIDumpAnalysis::DepthHistogram::is_empty() const
{
    return (get_total_weight() == 0);
}

void Analysis::APIDumpAnalysis::DepthHistogram::merge(const DepthHistogram& in_other)
{
    for (uint32_t n_bin = 0;
                  n_bin < N_BINS;
                ++n_bin)
    {
        m_weight_vec.at(n_bin) += in_other.m_weight_vec.at(n_bin);
    }
}

bool Analysis::APIDumpAnalysis::DepthHistogram::restore_weight(const uint32_t& in_bin,
                                                               const uint64_t& in_weight)
{
    const bool result = (in_bin < N_BINS);

    if (result)
    {
        m_weight_vec.at(in_bin) = in_weight;
    }

    return result;
}
