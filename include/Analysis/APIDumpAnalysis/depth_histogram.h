/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * How deep the draws of a pass are, as seen from the camera: the sum of a weight (the vertices of a draw) for each range of depth.
 * The ranges follow the logarithm of the depth, since what the eyes make of a depth is in proportion to it, and there are many
 * for each doubling. A converter chooses the distance at which the eyes converge from it (see Recipe's draw rules).
 */
#ifndef APIDUMPANALYSIS_DEPTH_HISTOGRAM_H
#define APIDUMPANALYSIS_DEPTH_HISTOGRAM_H

#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class DepthHistogram
        {
        public:
            DepthHistogram();

            /* Counts the weight at the depth, which is the distance along the way the camera looks. Nothing that is not in front of the
             * camera, or is nearer than the first range, is counted; what is beyond the last range is counted in it.
             */
            void add(const float&    in_depth,
                     const uint64_t& in_weight);

            /* The depth below which the fraction (from 0 to 1) of the weight is, by the middle of the range it falls in; 0 if there is no
             * weight.
             */
            float get_depth_at_fraction(const double& in_fraction) const;

            uint64_t get_total_weight() const;

            /* The weight of a range. */
            uint64_t get_weight(const uint32_t& in_bin) const;

            bool is_empty() const;

            /* Adds what the other has counted. */
            void merge(const DepthHistogram& in_other);

            /* Makes the weight of a range what it was, for a histogram that has been stored. Returns false if there is no such range. */
            bool restore_weight(const uint32_t& in_bin,
                                const uint64_t& in_weight);


            /* The depth in the middle of a range, on the logarithmic scale. */
            static float get_bin_depth(const uint32_t& in_bin);

            static uint32_t get_n_bins();

        private:
            std::vector<uint64_t> m_weight_vec; /* by range */
        };
    }
}

#endif /* APIDUMPANALYSIS_DEPTH_HISTOGRAM_H */
