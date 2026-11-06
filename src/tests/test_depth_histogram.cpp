/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the depth histogram: what it counts, and the depths it gives back.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/depth_histogram.h"
#include <limits>

TEST(DepthHistogram, ItIsEmptyAndHasNoDepthUntilSomethingIsCounted)
{
    const Analysis::APIDumpAnalysis::DepthHistogram histogram;

    EXPECT_TRUE(histogram.is_empty() );
    EXPECT_EQ  (histogram.get_total_weight(),
                0u);
    EXPECT_FLOAT_EQ(histogram.get_depth_at_fraction(0.5),
                    0.0f);
}

TEST(DepthHistogram, ADepthComesBackWithinTheWidthOfARange)
{
    const float depth_array[] = {0.05f, 1.0f, 3.5f, 10.0f, 250.0f, 9000.0f};

    for (const auto& depth : depth_array)
    {
        Analysis::APIDumpAnalysis::DepthHistogram histogram;

        histogram.add(depth,
                      10);

        /* A range spans 2 ^ (1 / 8), which is 9%; the middle of it is within half of that. */
        EXPECT_NEAR(histogram.get_depth_at_fraction(0.5),
                    depth,
                    depth * 0.05f);
    }
}

TEST(DepthHistogram, WhatIsNotInFrontOfTheCameraOrIsNotANumberIsNotCounted)
{
    Analysis::APIDumpAnalysis::DepthHistogram histogram;

    histogram.add(0.0f,
                  5);
    histogram.add(-3.0f,
                  5);
    histogram.add(0.001f,
                  5);
    histogram.add(std::numeric_limits<float>::quiet_NaN(),
                  5);
    histogram.add(std::numeric_limits<float>::infinity(),
                  5);

    EXPECT_TRUE(histogram.is_empty() );
}

TEST(DepthHistogram, WhatIsBeyondTheLastRangeIsCountedInIt)
{
    Analysis::APIDumpAnalysis::DepthHistogram histogram;

    histogram.add(1.0e9f,
                  7);

    EXPECT_EQ(histogram.get_weight(Analysis::APIDumpAnalysis::DepthHistogram::get_n_bins() - 1),
              7u);
}

TEST(DepthHistogram, TheFractionOfTheWeightIsFoundByTheVerticesNotByTheDraws)
{
    Analysis::APIDumpAnalysis::DepthHistogram histogram;

    /* 100 vertices at 2, 900 at 40: the median is where most of the vertices are. */
    histogram.add(2.0f,
                  100);
    histogram.add(40.0f,
                  900);

    EXPECT_NEAR(histogram.get_depth_at_fraction(0.5),
                40.0f,
                2.0f);
    EXPECT_NEAR(histogram.get_depth_at_fraction(0.05),
                2.0f,
                0.2f);
    EXPECT_NEAR(histogram.get_depth_at_fraction(1.0),
                40.0f,
                2.0f);
}

TEST(DepthHistogram, TwoHistogramsAreMergedAndAWeightIsRestoredOnlyForARangeThatThereIs)
{
    Analysis::APIDumpAnalysis::DepthHistogram first;
    Analysis::APIDumpAnalysis::DepthHistogram second;

    first.add (5.0f,
               3);
    second.add(5.0f,
               4);

    first.merge(second);

    EXPECT_EQ(first.get_total_weight(),
              7u);

    Analysis::APIDumpAnalysis::DepthHistogram restored;

    EXPECT_TRUE (restored.restore_weight(3,
                                         9) );
    EXPECT_EQ   (restored.get_weight(3),
                 9u);
    EXPECT_FALSE(restored.restore_weight(Analysis::APIDumpAnalysis::DepthHistogram::get_n_bins(),
                                         1) );
    EXPECT_EQ   (restored.get_weight(Analysis::APIDumpAnalysis::DepthHistogram::get_n_bins() ),
                 0u);
}
