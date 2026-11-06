/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What a constant (a camera matrix, say) was over the capture, as the values it took and when it took them. A constant
 * which is set again to what it already was is not stored again.
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

            /* Returns true if the value was stored, i.e. if it is not the one the constant had. */
            bool add(const uint64_t&             in_timestamp_qpc,
                     const std::vector<uint8_t>& in_value_u8_vec);

            /* Updates, including those that did not change the value. */
            uint64_t get_n_updates() const;

            const std::vector<Sample>& get_sample_vec() const;

            /* Makes the series what it was, for a series that has been stored. */
            void restore(const uint64_t&     in_n_updates,
                         std::vector<Sample> in_sample_vec);

        private:
            uint64_t            m_n_updates;
            std::vector<Sample> m_sample_vec;
        };
    }
}

#endif /* APIDUMPANALYSIS_VALUE_TIME_SERIES_H */
