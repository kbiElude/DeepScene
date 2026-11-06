/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of keeping what a constant was over time.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/value_time_series.h"

TEST(ValueTimeSeries, TheFirstValueIsStored)
{
    Analysis::APIDumpAnalysis::ValueTimeSeries series;

    EXPECT_TRUE(series.add(100,
                           {1, 2, 3}) );
    EXPECT_EQ  (series.get_sample_vec().size(),
                1u);
    EXPECT_EQ  (series.get_sample_vec().at(0).timestamp_qpc,
                100u);
}

TEST(ValueTimeSeries, ARepeatedValueIsNotStoredAgain)
{
    Analysis::APIDumpAnalysis::ValueTimeSeries series;

    series.add(100,
               {1, 2, 3});

    EXPECT_FALSE(series.add(200,
                            {1, 2, 3}) );
    EXPECT_FALSE(series.add(300,
                            {1, 2, 3}) );
    EXPECT_EQ   (series.get_sample_vec().size(),
                 1u);
    EXPECT_EQ   (series.get_n_updates(),
                 3u);
}

TEST(ValueTimeSeries, AChangeIsStoredWhenItHappens)
{
    Analysis::APIDumpAnalysis::ValueTimeSeries series;

    series.add(100,
               {1});
    series.add(200,
               {2});
    series.add(300,
               {2});

    EXPECT_TRUE(series.add(400,
                           {1}) );

    ASSERT_EQ(series.get_sample_vec().size(),
              3u);

    EXPECT_EQ(series.get_sample_vec().at(1).timestamp_qpc,
              200u);
    EXPECT_EQ(series.get_sample_vec().at(2).timestamp_qpc,
              400u);
    EXPECT_EQ(series.get_sample_vec().at(2).value_u8_vec,
              std::vector<uint8_t>({1}) );
    EXPECT_EQ(series.get_n_updates(),
              4u);
}
