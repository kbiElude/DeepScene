/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The values that a constant (for example, a camera matrix) took over the capture, and when it took them. If a constant is set again to the
 * value it already had, the value is not stored again.
 */
#ifndef APIDUMPANALYSIS_VALUE_TIME_SERIES_H
#define APIDUMPANALYSIS_VALUE_TIME_SERIES_H

#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class ValueTimeSeries
        {
        public:
            struct Sample
            {
                uint64_t             timestamp_qpc = 0; /* when the constant took the value */
                std::vector<uint8_t> value_u8_vec;
            };

            ValueTimeSeries();

            /* Returns true if the value was stored, that is, if it differs from the current value of the constant. */
            bool add(const uint64_t&             in_timestamp_qpc,
                     const std::vector<uint8_t>& in_value_u8_vec);

            /* The number of updates, including the updates that did not change the value. */
            uint64_t get_n_updates() const;

            const std::vector<Sample>& get_sample_vec() const;

            /* Restores a series that was stored earlier. */
            void restore(const uint64_t&     in_n_updates,
                         std::vector<Sample> in_sample_vec);

        private:
            uint64_t            m_n_updates;
            std::vector<Sample> m_sample_vec;
        };
    }
}

#endif /* APIDUMPANALYSIS_VALUE_TIME_SERIES_H */
