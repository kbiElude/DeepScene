/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the tracker of the vertices of Direct3D 7 draws: the positions it samples, from memory and from vertex buffers.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/d3d7/d3d7_geometry_tracker.h"
#include "d3d7_call_builder.h"
#include <limits>

TEST(D3D7GeometryTracker, PositionsAreTheFirstThreeFloatsOfEachVertex)
{
    constexpr uint32_t STRIDE = 16;

    const float vertex_array[8] = {1.0f, 2.0f, 3.0f, 9.0f,
                                   4.0f, 5.0f, 6.0f, 9.0f};

    const auto bytes = Tests::D3D7CallBuilder::get_bytes(vertex_array,
                                                         sizeof(vertex_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::DrawRange range;

    range.stride          = STRIDE;
    range.vertex_count    = 2;
    range.vertex_data_ptr = bytes.data();
    range.vertex_n_bytes  = bytes.size();

    const auto sample_ptr = Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    ASSERT_EQ(sample_ptr->position_vec.size(),
              2u);
    EXPECT_EQ(sample_ptr->position_vec.at(0).at(2),
              3.0f);
    EXPECT_EQ(sample_ptr->position_vec.at(1).at(0),
              4.0f);
}

TEST(D3D7GeometryTracker, PreTransformedVerticesHaveTheirXAndYAndTheReciprocalOfTheirDepth)
{
    constexpr uint32_t STRIDE = 32; /* the position, a diffuse and a specular color, and one pair of texture coordinates */

    /* x, y, z (of the depth buffer), the reciprocal of the depth, and what else the vertex has. */
    const float vertex_array[16] = {10.0f, 20.0f, 0.5f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f,
                                    30.0f, 40.0f, 0.6f, 0.125f, 0.0f, 0.0f, 0.0f, 0.0f};

    const auto bytes = Tests::D3D7CallBuilder::get_bytes(vertex_array,
                                                         sizeof(vertex_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::DrawRange range;

    range.is_pre_transformed = true;
    range.stride             = STRIDE;
    range.vertex_count       = 2;
    range.vertex_data_ptr    = bytes.data();
    range.vertex_n_bytes     = bytes.size();

    const auto sample_ptr = Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    EXPECT_TRUE(sample_ptr->is_pre_transformed);
    ASSERT_EQ(sample_ptr->position_vec.size(),
              2u);

    /* The third coordinate is the reciprocal of the depth, which is not the z of the vertex. */
    EXPECT_EQ(sample_ptr->position_vec.at(0).at(0),
              10.0f);
    EXPECT_EQ(sample_ptr->position_vec.at(0).at(1),
              20.0f);
    EXPECT_EQ(sample_ptr->position_vec.at(0).at(2),
              0.25f);
    EXPECT_EQ(sample_ptr->position_vec.at(1).at(2),
              0.125f);

    /* A vertex which has not room for the fourth. */
    range.stride = 12;

    EXPECT_EQ(Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::sample(range),
              nullptr);
}

TEST(D3D7GeometryTracker, IndicesPickTheVerticesAndTheBaseIsAddedToThem)
{
    constexpr uint32_t STRIDE = 12;

    const float    vertex_array[9] = {0.0f, 0.0f, 10.0f,
                                      0.0f, 0.0f, 20.0f,
                                      0.0f, 0.0f, 30.0f};
    const uint16_t index_array[2]  = {0, 1};

    const auto vertex_bytes = Tests::D3D7CallBuilder::get_bytes(vertex_array,
                                                                sizeof(vertex_array) );
    const auto index_bytes  = Tests::D3D7CallBuilder::get_bytes(index_array,
                                                                sizeof(index_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::DrawRange range;

    range.base_vertex_index = 1;
    range.index_data_ptr    = index_bytes.data();
    range.index_n_bytes     = index_bytes.size();
    range.stride            = STRIDE;
    range.vertex_count      = 2;
    range.vertex_data_ptr   = vertex_bytes.data();
    range.vertex_n_bytes    = vertex_bytes.size();

    const auto sample_ptr = Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    ASSERT_EQ(sample_ptr->position_vec.size(),
              2u);
    EXPECT_EQ(sample_ptr->position_vec.at(0).at(2),
              20.0f);
    EXPECT_EQ(sample_ptr->position_vec.at(1).at(2),
              30.0f);
}

TEST(D3D7GeometryTracker, VerticesBeyondTheDataAndNumbersWhichAreNotFiniteAreLeftOut)
{
    constexpr uint32_t STRIDE = 12;

    const float vertex_array[6] = {1.0f, 1.0f, 1.0f,
                                   std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f};

    const auto bytes = Tests::D3D7CallBuilder::get_bytes(vertex_array,
                                                         sizeof(vertex_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::DrawRange range;

    range.stride          = STRIDE;
    range.vertex_count    = 4; /* two more than there are */
    range.vertex_data_ptr = bytes.data();
    range.vertex_n_bytes  = bytes.size();

    const auto sample_ptr = Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    EXPECT_EQ(sample_ptr->position_vec.size(),
              1u);
}

TEST(D3D7GeometryTracker, AStrideWithNoRoomForAPositionIsNotSampled)
{
    constexpr uint32_t STRIDE = 8;

    const std::vector<uint8_t> bytes(64,
                                     0);

    Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::DrawRange range;

    range.stride          = STRIDE;
    range.vertex_count    = 4;
    range.vertex_data_ptr = bytes.data();
    range.vertex_n_bytes  = bytes.size();

    EXPECT_EQ(Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker::sample(range),
              nullptr);
}

TEST(D3D7GeometryTracker, AVertexBufferHoldsWhatWasWrittenToItAndNothingAfterItIsGone)
{
    const auto id = Analysis::APIDumpAnalysis::make_resource_id(1);

    Analysis::APIDumpAnalysis::Frontends::D3D7GeometryTracker tracker;

    EXPECT_EQ(tracker.get_vertex_buffer_bytes_ptr(id),
              nullptr);

    tracker.on_create_vertex_buffer(id,
                                    16);

    ASSERT_NE(tracker.get_vertex_buffer_bytes_ptr(id),
              nullptr);
    EXPECT_EQ(tracker.get_vertex_buffer_bytes_ptr(id)->size(),
              16u);

    const uint8_t data_array[4] = {1, 2, 3, 4};

    tracker.on_unlock(id,
                      data_array,
                      sizeof(data_array) );

    EXPECT_EQ(tracker.get_vertex_buffer_bytes_ptr(id)->at(2),
              3u);
    EXPECT_EQ(tracker.get_vertex_buffer_bytes_ptr(id)->at(4),
              0u);

    tracker.forget(id);

    EXPECT_EQ(tracker.get_vertex_buffer_bytes_ptr(id),
              nullptr);
}
