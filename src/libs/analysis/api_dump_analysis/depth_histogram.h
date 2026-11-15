/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The depths of the draws of a pass, as seen from the camera. Each range of depth holds a weight: the sum of the vertex counts of the
 * draws that fall into it. The ranges are spaced logarithmically (several per doubling of the depth), because the eyes judge depth in
 * proportion to distance. A converter chooses the distance at which the eyes converge from this histogram (see the draw rules of Recipe).
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

            /* Adds the weight at the depth, which is measured along the viewing direction of the camera. A depth that is not in front of the
             * camera, or is nearer than the first range, is ignored. A depth beyond the last range goes into the last range.
             */
            void add(const float&    in_depth,
                     const uint64_t& in_weight);

            /* Returns the depth below which the given fraction (0 to 1) of the weight lies. The depth is the middle of the range that
             * reaches the fraction. Returns 0 if the histogram holds no weight.
             */
            float get_depth_at_fraction(const double& in_fraction) const;

            uint64_t get_total_weight() const;

            /* The weight of a range. */
            uint64_t get_weight(const uint32_t& in_bin) const;

            bool is_empty() const;

            /* Adds the weights of the other histogram. */
            void merge(const DepthHistogram& in_other);

            /* Sets the weight of a range, to rebuild a stored histogram. Returns false if the range does not exist. */
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
