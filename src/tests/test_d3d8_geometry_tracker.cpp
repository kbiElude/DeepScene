/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for D3D8GeometryTracker: what the unlocks of buffers put in them, and the vertices of a draw that are told from it.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/d3d8/d3d8_geometry_tracker.h"
#include <string.h>

namespace
{
    constexpr uint32_t FORMAT_INDEX16  = 101;
    constexpr uint32_t FORMAT_INDEX32  = 102;
    constexpr uint32_t FVF_XYZ         = 0x002;
    constexpr uint32_t FVF_XYZRHW      = 0x004;
    constexpr uint32_t FVF_XYZ_TEX1    = 0x102;
    constexpr uint32_t STRIDE_XYZ      = 12;
    constexpr uint32_t STRIDE_XYZ_TEX1 = 20;

    Analysis::APIDumpAnalysis::ResourceId get_id(const uint32_t& in_value)
    {
        return Analysis::APIDumpAnalysis::make_resource_id(in_value);
    }

    /* A lock and an unlock which write the bytes at the offset. */
    void write(Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker* inout_tracker_ptr,
               const Analysis::APIDumpAnalysis::ResourceId&               in_id,
               const uint32_t&                                            in_offset,
               const void*                                                in_data_ptr,
               const uint32_t&                                            in_n_bytes)
    {
        inout_tracker_ptr->on_lock  (in_id,
                                     in_offset,
                                     in_n_bytes);
        inout_tracker_ptr->on_unlock(in_id,
                                     static_cast<const uint8_t*>(in_data_ptr),
                                     in_n_bytes);
    }
}

TEST(D3D8GeometryTracker, TheVerticesOfADrawWithoutIndicesAreReadFromTheFirstOne)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker tracker;
    const float                                               vertex_array[] = {1, 2, 3,
                                                                                4, 5, 6,
                                                                                7, 8, 9};

    tracker.on_create_vertex_buffer(get_id(1),
                                    sizeof(vertex_array) );
    write(&tracker,
          get_id(1),
          0,
          vertex_array,
          sizeof(vertex_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::DrawRange range;

    range.first            = 1;
    range.fvf              = FVF_XYZ;
    range.stride           = STRIDE_XYZ;
    range.vertex_buffer_id = get_id(1);
    range.vertex_count     = 2;

    const auto sample_ptr = tracker.sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    ASSERT_EQ(sample_ptr->position_vec.size(),
              2u);
    EXPECT_FLOAT_EQ(sample_ptr->position_vec.at(0).at(0),
                    4.0f);
    EXPECT_FLOAT_EQ(sample_ptr->position_vec.at(1).at(2),
                    9.0f);
}

TEST(D3D8GeometryTracker, TheIndicesAndTheBaseVertexSayWhichVerticesAreDrawn)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker tracker;
    const float                                               vertex_array[] = {0, 0, 0, 0, 0,
                                                                                1, 1, 1, 0, 0,
                                                                                2, 2, 2, 0, 0,
                                                                                3, 3, 3, 0, 0};
    const uint16_t                                            index_array[]  = {9, 9, 2, 0};

    tracker.on_create_vertex_buffer(get_id(1),
                                    sizeof(vertex_array) );
    tracker.on_create_index_buffer (get_id(2),
                                    sizeof(index_array),
                                    FORMAT_INDEX16);
    write(&tracker,
          get_id(1),
          0,
          vertex_array,
          sizeof(vertex_array) );
    write(&tracker,
          get_id(2),
          0,
          index_array,
          sizeof(index_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::DrawRange range;

    /* The indices 2 and 0 are at the third and fourth place; the base vertex moves them by one. */
    range.base_vertex_index = 1;
    range.first             = 2;
    range.fvf               = FVF_XYZ_TEX1;
    range.index_buffer_id   = get_id(2);
    range.is_indexed        = true;
    range.stride            = STRIDE_XYZ_TEX1;
    range.vertex_buffer_id  = get_id(1);
    range.vertex_count      = 2;

    const auto sample_ptr = tracker.sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    ASSERT_EQ(sample_ptr->position_vec.size(),
              2u);
    EXPECT_FLOAT_EQ(sample_ptr->position_vec.at(0).at(0),
                    3.0f);
    EXPECT_FLOAT_EQ(sample_ptr->position_vec.at(1).at(0),
                    1.0f);
}

TEST(D3D8GeometryTracker, ThirtyTwoBitIndicesAreRead)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker tracker;
    const float                                               vertex_array[] = {5, 5, 5,
                                                                                6, 6, 6};
    const uint32_t                                            index_array[]  = {1};

    tracker.on_create_vertex_buffer(get_id(1),
                                    sizeof(vertex_array) );
    tracker.on_create_index_buffer (get_id(2),
                                    sizeof(index_array),
                                    FORMAT_INDEX32);
    write(&tracker,
          get_id(1),
          0,
          vertex_array,
          sizeof(vertex_array) );
    write(&tracker,
          get_id(2),
          0,
          index_array,
          sizeof(index_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::DrawRange range;

    range.fvf              = FVF_XYZ;
    range.index_buffer_id  = get_id(2);
    range.is_indexed       = true;
    range.stride           = STRIDE_XYZ;
    range.vertex_buffer_id = get_id(1);
    range.vertex_count     = 1;

    const auto sample_ptr = tracker.sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    ASSERT_EQ(sample_ptr->position_vec.size(),
              1u);
    EXPECT_FLOAT_EQ(sample_ptr->position_vec.at(0).at(1),
                    6.0f);
}

TEST(D3D8GeometryTracker, WhatIsWrittenAtAnOffsetOfALockLandsThere)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker tracker;
    const float                                               first_array[]  = {1, 1, 1};
    const float                                               second_array[] = {2, 2, 2};

    tracker.on_create_vertex_buffer(get_id(1),
                                    2 * STRIDE_XYZ);
    write(&tracker,
          get_id(1),
          0,
          first_array,
          sizeof(first_array) );
    write(&tracker,
          get_id(1),
          STRIDE_XYZ,
          second_array,
          sizeof(second_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::DrawRange range;

    range.fvf              = FVF_XYZ;
    range.stride           = STRIDE_XYZ;
    range.vertex_buffer_id = get_id(1);
    range.vertex_count     = 2;

    const auto sample_ptr = tracker.sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    ASSERT_EQ(sample_ptr->position_vec.size(),
              2u);
    EXPECT_FLOAT_EQ(sample_ptr->position_vec.at(0).at(0),
                    1.0f);
    EXPECT_FLOAT_EQ(sample_ptr->position_vec.at(1).at(0),
                    2.0f);
}

TEST(D3D8GeometryTracker, ADrawIsSampledAgainWhenTheBufferIsWrittenAndNotBefore)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker tracker;
    const float                                               old_array[] = {1, 1, 1};
    const float                                               new_array[] = {8, 8, 8};

    tracker.on_create_vertex_buffer(get_id(1),
                                    STRIDE_XYZ);
    write(&tracker,
          get_id(1),
          0,
          old_array,
          sizeof(old_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::DrawRange range;

    range.fvf              = FVF_XYZ;
    range.stride           = STRIDE_XYZ;
    range.vertex_buffer_id = get_id(1);
    range.vertex_count     = 1;

    const auto first_ptr  = tracker.sample(range);
    const auto second_ptr = tracker.sample(range);

    EXPECT_EQ(first_ptr,
              second_ptr);

    write(&tracker,
          get_id(1),
          0,
          new_array,
          sizeof(new_array) );

    const auto third_ptr = tracker.sample(range);

    ASSERT_NE(third_ptr,
              nullptr);
    EXPECT_NE(third_ptr,
              first_ptr);
    EXPECT_FLOAT_EQ(third_ptr->position_vec.at(0).at(0),
                    8.0f);
    EXPECT_FLOAT_EQ(first_ptr->position_vec.at(0).at(0),
                    1.0f);
}

TEST(D3D8GeometryTracker, VerticesThatHaveNoPositionInTheSpaceOfTheModelAreNotSampled)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker tracker;
    const float                                               vertex_array[] = {1, 2, 3, 1};

    tracker.on_create_vertex_buffer(get_id(1),
                                    sizeof(vertex_array) );
    write(&tracker,
          get_id(1),
          0,
          vertex_array,
          sizeof(vertex_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::DrawRange range;

    range.stride           = 16;
    range.vertex_buffer_id = get_id(1);
    range.vertex_count     = 1;

    /* Pre-transformed, and the vertices of a vertex shader. */
    range.fvf = FVF_XYZRHW;

    EXPECT_EQ(tracker.sample(range),
              nullptr);

    range.fvf = 0;

    EXPECT_EQ(tracker.sample(range),
              nullptr);
}

TEST(D3D8GeometryTracker, WhatCannotBeToldGivesNoSample)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker tracker;
    const float                                               vertex_array[] = {1, 2, 3};
    const uint16_t                                            index_array[]  = {40};

    tracker.on_create_vertex_buffer(get_id(1),
                                    sizeof(vertex_array) );
    tracker.on_create_index_buffer (get_id(2),
                                    sizeof(index_array),
                                    FORMAT_INDEX16);
    write(&tracker,
          get_id(1),
          0,
          vertex_array,
          sizeof(vertex_array) );
    write(&tracker,
          get_id(2),
          0,
          index_array,
          sizeof(index_array) );

    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::DrawRange range;

    range.fvf              = FVF_XYZ;
    range.stride           = STRIDE_XYZ;
    range.vertex_buffer_id = get_id(1);
    range.vertex_count     = 1;

    /* A buffer that the tracker does not know, and a stride too small to hold a position. */
    range.vertex_buffer_id = get_id(77);

    EXPECT_EQ(tracker.sample(range),
              nullptr);

    range.vertex_buffer_id = get_id(1);
    range.stride           = 8;

    EXPECT_EQ(tracker.sample(range),
              nullptr);

    /* An index that is past the end of the vertices. */
    range.index_buffer_id = get_id(2);
    range.is_indexed      = true;
    range.stride          = STRIDE_XYZ;

    EXPECT_EQ(tracker.sample(range),
              nullptr);

    /* An index buffer that is gone. */
    tracker.forget(get_id(2) );

    EXPECT_EQ(tracker.sample(range),
              nullptr);
}

TEST(D3D8GeometryTracker, AnUnlockWithNoDataLeavesTheBufferAsItWas)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker tracker;
    const float                                               vertex_array[] = {4, 4, 4};

    tracker.on_create_vertex_buffer(get_id(1),
                                    STRIDE_XYZ);
    write(&tracker,
          get_id(1),
          0,
          vertex_array,
          sizeof(vertex_array) );

    /* A lock for reading has no data with its unlock. */
    tracker.on_lock  (get_id(1),
                      0,
                      STRIDE_XYZ);
    tracker.on_unlock(get_id(1),
                      nullptr,
                      0);

    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::DrawRange range;

    range.fvf              = FVF_XYZ;
    range.stride           = STRIDE_XYZ;
    range.vertex_buffer_id = get_id(1);
    range.vertex_count     = 1;

    const auto sample_ptr = tracker.sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    EXPECT_FLOAT_EQ(sample_ptr->position_vec.at(0).at(0),
                    4.0f);
}

TEST(D3D8GeometryTracker, AtMostAFewDozenVerticesAreLookedAtSpreadOverTheDraw)
{
    constexpr uint32_t N_VERTICES = 1000;

    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker tracker;
    std::vector<float>                                        vertex_vec(3 * N_VERTICES);

    for (uint32_t n_vertex = 0;
                  n_vertex < N_VERTICES;
                ++n_vertex)
    {
        vertex_vec.at(3 * n_vertex) = static_cast<float>(n_vertex);
    }

    tracker.on_create_vertex_buffer(get_id(1),
                                    N_VERTICES * STRIDE_XYZ);
    write(&tracker,
          get_id(1),
          0,
          vertex_vec.data(),
          static_cast<uint32_t>(vertex_vec.size() * sizeof(float) ) );

    Analysis::APIDumpAnalysis::Frontends::D3D8GeometryTracker::DrawRange range;

    range.fvf              = FVF_XYZ;
    range.stride           = STRIDE_XYZ;
    range.vertex_buffer_id = get_id(1);
    range.vertex_count     = N_VERTICES;

    const auto sample_ptr = tracker.sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    EXPECT_LE(sample_ptr->position_vec.size(),
              100u);
    EXPECT_GE(sample_ptr->position_vec.size(),
              16u);
    EXPECT_FLOAT_EQ(sample_ptr->position_vec.front().at(0),
                    0.0f);
    EXPECT_GT(sample_ptr->position_vec.back().at(0),
              static_cast<float>(N_VERTICES) * 0.9f);
}
