/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for DepthMeasurer: what it says of a depth buffer is what DepthBufferStatistics::measure() says, to the last bit, with AVX2 and without it.
 */
#include <gtest/gtest.h>
#include "stereo/core/depth_buffer_statistics.h"
#include "stereo/core/depth_measurer.h"
#include <cmath>
#include <limits>
#include <random>
#include <vector>

namespace
{
    /* The elements 10 and 14 of the projection of OpenGL that has the planes at 0.1 and 1000. */
    constexpr float NEAR_PLANE = 0.1f;
    constexpr float FAR_PLANE  = 1000.0f;
    constexpr float Z_SCALE    = -(FAR_PLANE + NEAR_PLANE) / (FAR_PLANE - NEAR_PLANE);
    constexpr float Z_SHIFT    = -2.0f * FAR_PLANE * NEAR_PLANE / (FAR_PLANE - NEAR_PLANE);

    /* The depth that the buffer holds for a surface that is this far from the camera. */
    float get_depth(const float& in_distance)
    {
        const float device_z = Z_SHIFT / in_distance - Z_SCALE;

        return (device_z + 1.0f) * 0.5f;
    }

    /* A buffer of a scene: distances from near to far (most of them near), with a share of the pixels that are the sky. */
    std::vector<float> make_scene(const size_t& in_n_depths,
                                  const float&    in_sky_share,
                                  const uint32_t& in_seed)
    {
        std::mt19937                          rng(in_seed);
        std::uniform_real_distribution<float> unit(0.0f,
                                                   1.0f);
        std::vector<float>                    result;

        for (size_t n_depth = 0;
                    n_depth < in_n_depths;
                  ++n_depth)
        {
            if (unit(rng) < in_sky_share)
            {
                result.push_back(1.0f);
            }
            else
            {
                /* Log-uniform between 0.2 and 500: what a scene has is spread over several powers of two. */
                result.push_back(get_depth(0.2f * std::pow(2500.0f,
                                                           unit(rng) ) ) );
            }
        }

        return result;
    }

    void expect_same_as_scalar(Stereo::Core::DepthMeasurer&  in_measurer,
                               const std::vector<float>&     in_depth_vec,
                               const size_t&                 in_step,
                               const float&                  in_z_scale,
                               const float&                  in_z_shift,
                               const float&                  in_fraction)
    {
        const auto expected = Stereo::Core::DepthBufferStatistics::measure(in_depth_vec.data(),
                                                                           in_depth_vec.size(),
                                                                           in_step,
                                                                           in_z_scale,
                                                                           in_z_shift,
                                                                           in_fraction);
        const auto actual   = in_measurer.measure(in_depth_vec.data(),
                                                  in_depth_vec.size(),
                                                  in_step,
                                                  in_z_scale,
                                                  in_z_shift,
                                                  in_fraction);

        EXPECT_EQ(actual.has_depth,
                  expected.has_depth)    << "n " << in_depth_vec.size() << " fraction " << in_fraction;
        EXPECT_EQ(actual.far_depth,
                  expected.far_depth)    << "n " << in_depth_vec.size() << " fraction " << in_fraction;
        EXPECT_EQ(actual.median_depth,
                  expected.median_depth) << "n " << in_depth_vec.size() << " fraction " << in_fraction;
        EXPECT_EQ(actual.near_depth,
                  expected.near_depth)   << "n " << in_depth_vec.size() << " fraction " << in_fraction;
    }
}

TEST(DepthMeasurer, ItGivesWhatTheScalarMeasureGivesForScenesOfAllSizesAndSkies)
{
    Stereo::Core::DepthMeasurer measurer(true);

    for (const size_t n_depths : {1, 7, 8, 9, 15, 16, 17, 100, 1001, 76800})
    {
        for (const float sky_share : {0.0f, 0.3f, 0.99f, 0.999f, 1.0f})
        {
            const auto depth_vec = make_scene(n_depths,
                                              sky_share,
                                              static_cast<uint32_t>(n_depths) );

            for (const float fraction : {0.0f, 0.001f, 0.02f, 0.5f, 1.0f})
            {
                expect_same_as_scalar(measurer,
                                      depth_vec,
                                      1,
                                      Z_SCALE,
                                      Z_SHIFT,
                                      fraction);
            }
        }
    }
}

TEST(DepthMeasurer, ItUsesAvx2WhenTheProcessorHasIt)
{
    EXPECT_EQ(Stereo::Core::DepthMeasurer(true).is_using_avx2(),
              Stereo::Core::DepthMeasurer::is_avx2_supported() );
}

TEST(DepthMeasurer, ItGivesWhatTheScalarMeasureGivesWithoutAvx2)
{
    Stereo::Core::DepthMeasurer measurer(false);

    EXPECT_FALSE(measurer.is_using_avx2() );

    for (const float sky_share : {0.0f, 0.4f})
    {
        const auto depth_vec = make_scene(5000,
                                          sky_share,
                                          3);

        expect_same_as_scalar(measurer,
                              depth_vec,
                              1,
                              Z_SCALE,
                              Z_SHIFT,
                              0.02f);
    }
}

TEST(DepthMeasurer, ItGivesWhatTheScalarMeasureGivesForDepthsThatAreNotDepths)
{
    Stereo::Core::DepthMeasurer measurer(true);
    auto                        depth_vec = make_scene(2000,
                                                       0.2f,
                                                       11);

    /* What a read of a buffer can hold that is not a depth: nothing at all, infinity, below 0, above 1; and the depth at which the projection has no distance. */
    const float depth_without_distance = (-Z_SCALE + 1.0f) * 0.5f;

    for (size_t n_depth = 0;
                n_depth < depth_vec.size();
                n_depth += 13)
    {
        switch ( (n_depth / 13) % 5)
        {
            case 0:  depth_vec.at(n_depth) = std::numeric_limits<float>::quiet_NaN(); break;
            case 1:  depth_vec.at(n_depth) = std::numeric_limits<float>::infinity();  break;
            case 2:  depth_vec.at(n_depth) = -0.5f;                                   break;
            case 3:  depth_vec.at(n_depth) = 1.5f;                                    break;
            default: depth_vec.at(n_depth) = depth_without_distance;                  break;
        }
    }

    for (const float fraction : {0.0f, 0.02f, 0.5f})
    {
        expect_same_as_scalar(measurer,
                              depth_vec,
                              1,
                              Z_SCALE,
                              Z_SHIFT,
                              fraction);
    }
}

TEST(DepthMeasurer, ItGivesWhatTheScalarMeasureGivesForDistancesThatAreAlmostNothing)
{
    Stereo::Core::DepthMeasurer measurer(true);
    const auto                  depth_vec = make_scene(3000,
                                                       0.0f,
                                                       5);

    /* A shift this small gives distances that are not normal floats, and the bin that they are in is still not the bin of what is not a distance. */
    expect_same_as_scalar(measurer,
                          depth_vec,
                          1,
                          -1.0f,
                          -1.0e-40f,
                          0.02f);

    /* And one that is not the one of the planes at all. */
    expect_same_as_scalar(measurer,
                          depth_vec,
                          1,
                          Z_SCALE,
                          -1.0e30f,
                          0.02f);
}

TEST(DepthMeasurer, WhatItWorksInIsLeftCleanForTheNextCall)
{
    Stereo::Core::DepthMeasurer measurer(true);

    /* A scene that spreads over many bins, then one that is in a few, then a buffer with nothing drawn, then the first scene again: each is what the scalar measure says. */
    const auto wide_vec  = make_scene(76800,
                                      0.1f,
                                      21);
    const auto flat_vec  = std::vector<float>(5000,
                                              get_depth(12.0f) );
    const auto empty_vec = std::vector<float>(2000,
                                              1.0f);

    for (int n_round = 0;
             n_round < 3;
           ++n_round)
    {
        for (const auto* depth_vec_ptr : {&wide_vec, &flat_vec, &empty_vec, &wide_vec})
        {
            expect_same_as_scalar(measurer,
                                  *depth_vec_ptr,
                                  1,
                                  Z_SCALE,
                                  Z_SHIFT,
                                  0.02f);
        }
    }
}

TEST(DepthMeasurer, ABufferThatIsReadEveryFewPixelsIsMeasuredTheWayItIsWithoutAvx2)
{
    Stereo::Core::DepthMeasurer measurer(true);
    const auto                  depth_vec = make_scene(20000,
                                                       0.3f,
                                                       9);

    for (const size_t step : {0, 2, 4})
    {
        expect_same_as_scalar(measurer,
                              depth_vec,
                              step,
                              Z_SCALE,
                              Z_SHIFT,
                              0.02f);
    }
}

TEST(DepthMeasurer, ItGivesWhatTheScalarMeasureGivesWhereverTheBufferStartsInMemory)
{
    Stereo::Core::DepthMeasurer measurer(true);
    const auto                  scene_vec = make_scene(4096 + 64,
                                                       0.2f,
                                                       17);

    /* The copy that the measurer makes is of lines of 64 bytes when the buffer starts at one, and by the pixel when it does not. */
    for (size_t n_offset = 0;
                n_offset < 17;
              ++n_offset)
    {
        std::vector<float> depth_vec(scene_vec.begin() + static_cast<std::ptrdiff_t>(n_offset),
                                     scene_vec.begin() + static_cast<std::ptrdiff_t>(n_offset) + 4000);
        const auto         expected = Stereo::Core::DepthBufferStatistics::measure(depth_vec.data(),
                                                                                   depth_vec.size(),
                                                                                   1,
                                                                                   Z_SCALE,
                                                                                   Z_SHIFT,
                                                                                   0.02f);
        const auto         actual   = measurer.measure(scene_vec.data() + n_offset,
                                                       4000,
                                                       1,
                                                       Z_SCALE,
                                                       Z_SHIFT,
                                                       0.02f);

        EXPECT_EQ(actual.far_depth,
                  expected.far_depth)    << "offset " << n_offset;
        EXPECT_EQ(actual.median_depth,
                  expected.median_depth) << "offset " << n_offset;
        EXPECT_EQ(actual.near_depth,
                  expected.near_depth)   << "offset " << n_offset;
    }
}

TEST(DepthMeasurer, ABufferOfNoDepthsSaysNothing)
{
    Stereo::Core::DepthMeasurer measurer(true);
    const float                 no_depth = 0.5f;

    EXPECT_FALSE(measurer.measure(&no_depth,
                                  0,
                                  1,
                                  Z_SCALE,
                                  Z_SHIFT,
                                  0.02f).has_depth);
}
