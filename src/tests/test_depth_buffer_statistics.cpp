/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for DepthBufferStatistics: how deep a frame is, as the depth buffer that an application of OpenGL rendered says.
 */
#include <gtest/gtest.h>
#include "stereo/core/depth_buffer_statistics.h"
#include <vector>

namespace
{
    /* The elements 10 and 14 of the projection of OpenGL that has the planes at 1 and 100. */
    constexpr float NEAR_PLANE = 1.0f;
    constexpr float FAR_PLANE  = 100.0f;
    constexpr float Z_SCALE    = -(FAR_PLANE + NEAR_PLANE) / (FAR_PLANE - NEAR_PLANE);
    constexpr float Z_SHIFT    = -2.0f * FAR_PLANE * NEAR_PLANE / (FAR_PLANE - NEAR_PLANE);

    /* The depth that the buffer holds for a surface that is this far from the camera. */
    float get_depth(const float& in_distance)
    {
        const float device_z = Z_SHIFT / in_distance - Z_SCALE;

        return (device_z + 1.0f) * 0.5f;
    }
}

TEST(DepthBufferStatistics, ADepthIsTurnedBackIntoTheDistanceItWasMadeFrom)
{
    for (const float distance : {1.0f, 2.5f, 10.0f, 50.0f, 100.0f})
    {
        EXPECT_NEAR(Stereo::Core::DepthBufferStatistics::get_distance(get_depth(distance),
                                                                      Z_SCALE,
                                                                      Z_SHIFT),
                    distance,
                    distance * 0.001f);
    }
}

TEST(DepthBufferStatistics, TheNearestAndTheFarthestSurfacesAreTheEdgesOfWhatWasDrawn)
{
    std::vector<float> depth_vec;

    for (size_t n_pixel = 0;
                n_pixel < 1000;
              ++n_pixel)
    {
        depth_vec.push_back(get_depth( (n_pixel < 400) ? 10.0f
                                     : (n_pixel < 900) ? 20.0f
                                                       : 50.0f) );
    }

    const auto range = Stereo::Core::DepthBufferStatistics::measure(depth_vec.data(),
                                                                    depth_vec.size(),
                                                                    1,
                                                                    Z_SCALE,
                                                                    Z_SHIFT,
                                                                    0.01f);

    ASSERT_TRUE(range.has_depth);
    EXPECT_NEAR(range.near_depth,
                10.0f,
                0.1f);
    EXPECT_NEAR(range.far_depth,
                50.0f,
                0.5f);
    EXPECT_NEAR(range.median_depth,
                20.0f,
                0.2f);
}

TEST(DepthBufferStatistics, WhatWasNotDrawnToIsTheSkyAndAsFarAsTheFarPlane)
{
    std::vector<float> depth_vec;

    for (size_t n_pixel = 0;
                n_pixel < 1000;
              ++n_pixel)
    {
        depth_vec.push_back( (n_pixel < 800) ? get_depth(10.0f)
                                             : 1.0f);
    }

    const auto range = Stereo::Core::DepthBufferStatistics::measure(depth_vec.data(),
                                                                    depth_vec.size(),
                                                                    1,
                                                                    Z_SCALE,
                                                                    Z_SHIFT,
                                                                    0.01f);

    ASSERT_TRUE(range.has_depth);
    EXPECT_NEAR(range.far_depth,
                FAR_PLANE,
                0.5f);
    EXPECT_NEAR(range.near_depth,
                10.0f,
                0.1f);

    /* Less of it than the fraction is not the sky: the farthest surface is the farthest that was drawn. */
    depth_vec.assign(1000,
                     get_depth(10.0f) );
    depth_vec.at(0) = get_depth(30.0f);

    const auto no_sky_range = Stereo::Core::DepthBufferStatistics::measure(depth_vec.data(),
                                                                           depth_vec.size(),
                                                                           1,
                                                                           Z_SCALE,
                                                                           Z_SHIFT,
                                                                           0.0001f);

    EXPECT_NEAR(no_sky_range.far_depth,
                30.0f,
                0.3f);
}

TEST(DepthBufferStatistics, ABufferWithNothingDrawnInItSaysNothing)
{
    const std::vector<float> empty_vec(1000,
                                       1.0f);
    const std::vector<float> hardly_any_vec = [&]()
    {
        std::vector<float> result(10000,
                                  1.0f);

        for (size_t n_pixel = 0;
                    n_pixel < 20;
                  ++n_pixel)
        {
            result.at(n_pixel) = get_depth(10.0f);
        }

        return result;
    }();

    EXPECT_FALSE(Stereo::Core::DepthBufferStatistics::measure(empty_vec.data(),
                                                              empty_vec.size(),
                                                              1,
                                                              Z_SCALE,
                                                              Z_SHIFT,
                                                              0.01f).has_depth);
    EXPECT_FALSE(Stereo::Core::DepthBufferStatistics::measure(hardly_any_vec.data(),
                                                              hardly_any_vec.size(),
                                                              1,
                                                              Z_SCALE,
                                                              Z_SHIFT,
                                                              0.01f).has_depth);
}
