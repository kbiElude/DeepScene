/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The count, the mean and the worst of a series of times (in milliseconds), for a line of the log each second: what tells whether the
 * frames of the converter and the headset come evenly. Not thread-safe: a series is for the thread that adds to it.
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
            /* A value above the late threshold is counted as late (0: none is). */
            explicit SeriesStats(const double& in_late_threshold = 0.0);

            void add(const double& in_value);

            /* Adds the time since the last time this was called (nothing the first time). */
            void add_interval(const double& in_now);

            /* The text for the values since the last time, with the name; starts the series again. */
            std::string take_text(const std::string& in_name);

            void set_late_threshold(const double& in_late_threshold);

            /* The time in milliseconds, from a clock that only goes forward. */
            static double get_now();

            /* Whether the period has passed since the time in the first argument, which is then the time now. */
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
