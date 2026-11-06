/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/value_time_series.h"

Analysis::APIDumpAnalysis::ValueTimeSeries::ValueTimeSeries()
    :m_n_updates(0)
{
    /* Stub */
}

bool Analysis::APIDumpAnalysis::ValueTimeSeries::add(const uint64_t&             in_timestamp_qpc,
                                                     const std::vector<uint8_t>& in_value_u8_vec)
{
    bool result = false;

    m_n_updates++;

    if (m_sample_vec.empty()                                 ||
        m_sample_vec.back ().value_u8_vec != in_value_u8_vec)
    {
        Sample sample;

        sample.timestamp_qpc = in_timestamp_qpc;
        sample.value_u8_vec  = in_value_u8_vec;

        m_sample_vec.push_back(std::move(sample) );

        result = true;
    }

    return result;
}

uint64_t Analysis::APIDumpAnalysis::ValueTimeSeries::get_n_updates() const
{
    return m_n_updates;
}

const std::vector<Analysis::APIDumpAnalysis::ValueTimeSeries::Sample>& Analysis::APIDumpAnalysis::ValueTimeSeries::get_sample_vec() const
{
    return m_sample_vec;
}

void Analysis::APIDumpAnalysis::ValueTimeSeries::restore(const uint64_t&     in_n_updates,
                                                         std::vector<Sample> in_sample_vec)
{
    m_n_updates  = in_n_updates;
    m_sample_vec = std::move(in_sample_vec);
}
