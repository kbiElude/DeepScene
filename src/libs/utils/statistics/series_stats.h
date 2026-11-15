/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The count, the mean, the worst value and the late count of a series of times (in milliseconds), for one log line each second. The line shows
 * whether the frames of the converter and the headset arrive evenly. Not thread-safe: only the thread that adds to a series may use it.
 */
#ifndef UTILS_STATISTICS_SERIES_STATS_H
#define UTILS_STATISTICS_SERIES_STATS_H

#include <stdint.h>
#include <string>

namespace Utils
{
    namespace Statistics
    {
        class SeriesStats
        {
        public:
            /* A value above the late threshold counts as late. With 0, no value counts as late. */
            explicit SeriesStats(const double& in_late_threshold = 0.0);

            void add(const double& in_value);

            /* Adds the time since the previous call. The first call only records the time. */
            void add_interval(const double& in_now);

            /* Returns the text for the values since the previous call, prefixed with the name, and starts the series again. */
            std::string take_text(const std::string& in_name);

            void set_late_threshold(const double& in_late_threshold);

            /* Returns the current time in milliseconds from a clock that never goes backward. */
            static double get_now();

            /* Returns true if in_period has passed since *inout_since_ptr. In that case *inout_since_ptr is set to in_now. */
            static bool take_period(double*       inout_since_ptr,
                                    const double& in_now,
                                    const double& in_period);

        private:
            double   m_last_mark;
            double   m_late_threshold;
            uint64_t m_n_late;
            uint64_t m_n_values;
            double   m_sum;
            double   m_worst;
        };
    }
}

#endif /* UTILS_STATISTICS_SERIES_STATS_H */
