/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/statistics/series_stats.h"
#include <stdio.h>
#include <Windows.h>

Utils::Statistics::SeriesStats::SeriesStats(const double& in_late_threshold)
    :m_last_mark     (-1.0),
     m_late_threshold(in_late_threshold),
     m_n_late        (0),
     m_n_values      (0),
     m_sum           (0.0),
     m_worst         (0.0)
{
    /* Stub */
}

void Utils::Statistics::SeriesStats::add(const double& in_value)
{
    ++m_n_values;

    m_sum += in_value;

    if (in_value > m_worst)
    {
        m_worst = in_value;
    }

    if (m_late_threshold > 0.0              &&
        in_value         > m_late_threshold)
    {
        ++m_n_late;
    }
}

void Utils::Statistics::SeriesStats::add_interval(const double& in_now)
{
    if (m_last_mark >= 0.0)
    {
        add(in_now - m_last_mark);
    }

    m_last_mark = in_now;
}

double Utils::Statistics::SeriesStats::get_now()
{
    constexpr double N_MILLISECONDS_PER_SECOND = 1000.0;

    LARGE_INTEGER counter;
    LARGE_INTEGER frequency;

    ::QueryPerformanceCounter  (&counter);
    ::QueryPerformanceFrequency(&frequency);

    return static_cast<double>(counter.QuadPart) * N_MILLISECONDS_PER_SECOND / static_cast<double>(frequency.QuadPart);
}

void Utils::Statistics::SeriesStats::set_late_threshold(const double& in_late_threshold)
{
    m_late_threshold = in_late_threshold;
}

bool Utils::Statistics::SeriesStats::take_period(double*       inout_since_ptr,
                                                 const double& in_now,
                                                 const double& in_period)
{
    const bool result = (in_now - *inout_since_ptr >= in_period);

    if (result)
    {
        *inout_since_ptr = in_now;
    }

    return result;
}

std::string Utils::Statistics::SeriesStats::take_text(const std::string& in_name)
{
    char text_i8vec128[128];

    ::snprintf(text_i8vec128,
               sizeof(text_i8vec128),
               "%s n=%llu mean=%.2f worst=%.2f late=%llu",
               in_name.c_str(),
               static_cast<unsigned long long>(m_n_values),
               (m_n_values > 0) ? m_sum / static_cast<double>(m_n_values)
                                : 0.0,
               m_worst,
               static_cast<unsigned long long>(m_n_late) );

    m_n_late   = 0;
    m_n_values = 0;
    m_sum      = 0.0;
    m_worst    = 0.0;

    return text_i8vec128;
}
