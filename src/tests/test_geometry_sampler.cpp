/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the sampling of the vertices of a draw that the frontends of the APIs share: which vertices are read, and what is left out.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/shared/geometry_sampler.h"
#include <limits>
#include <string.h>
#include <vector>

namespace
{
    template <typename Element>
    std::vector<uint8_t> get_bytes(const std::vector<Element>& in_element_vec)
    {
        std::vector<uint8_t> result(in_element_vec.size() * sizeof(Element) );

        ::memcpy(result.data(),
                 in_element_vec.data(),
                 result.size() );

        return result;
    }

    /* A vertex of 3 floats for each of the positions. */
    std::vector<float> get_positions(const size_t& in_n_vertices)
    {
        std::vector<float> result;

        for (size_t n_vertex = 0;
                    n_vertex < in_n_vertices;
                  ++n_vertex)
        {
            result.push_back(static_cast<float>(n_vertex) );
            result.push_back(static_cast<float>(n_vertex) + 0.25f);
            result.push_back(static_cast<float>(n_vertex) + 0.5f);
        }

        return result;
    }
}

TEST(GeometrySampler, APositionIsThreeFloatsAndFourForAVertexInThePicture)
{
    EXPECT_EQ(Analysis::APIDumpAnalysis::Frontends::GeometrySampler::get_position_n_bytes(false),
              3 * sizeof(float) );
    EXPECT_EQ(Analysis::APIDumpAnalysis::Frontends::GeometrySampler::get_position_n_bytes(true),
              4 * sizeof(float) );
}

TEST(GeometrySampler, IndicesOfTwoBytesAndOfFourBytesPickTheSameVertices)
{
    const auto vertex_byte_vec = get_bytes(get_positions(10) );

    Analysis::APIDumpAnalysis::Frontends::GeometrySamplingRange range;

    range.base_vertex_index = 1;
    range.is_indexed        = true;
    range.stride            = 3 * sizeof(float);
    range.vertex_count      = 3;
    range.vertex_data_ptr   = vertex_byte_vec.data();
    range.vertex_n_bytes    = vertex_byte_vec.size();

    const auto index_16_vec = get_bytes(std::vector<uint16_t>{7, 2, 5});
    const auto index_32_vec = get_bytes(std::vector<uint32_t>{7, 2, 5});

    range.index_data_ptr = index_16_vec.data();
    range.index_n_bytes  = index_16_vec.size();
    range.index_stride   = sizeof(uint16_t);

    const auto sample_16_ptr = Analysis::APIDumpAnalysis::Frontends::GeometrySampler::sample(range);

    range.index_data_ptr = index_32_vec.data();
    range.index_n_bytes  = index_32_vec.size();
    range.index_stride   = sizeof(uint32_t);

    const auto sample_32_ptr = Analysis::APIDumpAnalysis::Frontends::GeometrySampler::sample(range);

    ASSERT_NE(sample_16_ptr,
              nullptr);
    ASSERT_NE(sample_32_ptr,
              nullptr);
    ASSERT_EQ(sample_16_ptr->position_vec.size(),
              3u);

    /* The base is added to each index: vertices 8, 3 and 6. */
    EXPECT_EQ(sample_16_ptr->position_vec.at(0).at(0),
              8.0f);
    EXPECT_EQ(sample_16_ptr->position_vec.at(1).at(0),
              3.0f);
    EXPECT_EQ(sample_16_ptr->position_vec.at(2).at(0),
              6.0f);
    EXPECT_EQ(sample_32_ptr->position_vec,
              sample_16_ptr->position_vec);
}

TEST(GeometrySampler, AtMostAFewDozenVerticesAreReadSpreadOverTheDraw)
{
    constexpr size_t N_VERTICES = 1000;

    const auto vertex_byte_vec = get_bytes(get_positions(N_VERTICES) );

    Analysis::APIDumpAnalysis::Frontends::GeometrySamplingRange range;

    range.stride          = 3 * sizeof(float);
    range.vertex_count    = N_VERTICES;
    range.vertex_data_ptr = vertex_byte_vec.data();
    range.vertex_n_bytes  = vertex_byte_vec.size();

    const auto sample_ptr = Analysis::APIDumpAnalysis::Frontends::GeometrySampler::sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    ASSERT_EQ(sample_ptr->position_vec.size(),
              64u);

    /* From the first vertex to near the last, in order. */
    EXPECT_EQ(sample_ptr->position_vec.front().at(0),
              0.0f);
    EXPECT_GT(sample_ptr->position_vec.back().at(0),
              900.0f);
    EXPECT_LT(sample_ptr->position_vec.front().at(0),
              sample_ptr->position_vec.at(1).at(0) );
}

TEST(GeometrySampler, AVertexInThePictureHasTheReciprocalOfItsDepthForItsThirdCoordinate)
{
    /* x, y, the depth of the buffer, and the reciprocal of the depth. */
    const std::vector<float> vertex_vec = {10.0f, 20.0f, 0.5f, 0.25f,
                                           30.0f, 40.0f, 0.6f, 0.125f};

    const auto vertex_byte_vec = get_bytes(vertex_vec);

    Analysis::APIDumpAnalysis::Frontends::GeometrySamplingRange range;

    range.is_pre_transformed = true;
    range.stride             = 4 * sizeof(float);
    range.vertex_count       = 2;
    range.vertex_data_ptr    = vertex_byte_vec.data();
    range.vertex_n_bytes     = vertex_byte_vec.size();

    const auto sample_ptr = Analysis::APIDumpAnalysis::Frontends::GeometrySampler::sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    EXPECT_TRUE(sample_ptr->is_pre_transformed);
    EXPECT_EQ(sample_ptr->position_vec.at(0).at(2),
              0.25f);
    EXPECT_EQ(sample_ptr->position_vec.at(1).at(2),
              0.125f);
}

TEST(GeometrySampler, WhatIsNotANumberOrIsOutsideTheDataIsLeftOut)
{
    constexpr float NOT_A_NUMBER = std::numeric_limits<float>::quiet_NaN();
    constexpr float INFINITY_F   = std::numeric_limits<float>::infinity();

    /* The third and the fifth vertices are not numbers, and the vertex count says one more than there is data for. */
    const std::vector<float> vertex_vec = {1.0f, 1.0f, 1.0f,
                                           2.0f, 2.0f, 2.0f,
                                           NOT_A_NUMBER, 3.0f, 3.0f,
                                           4.0f, 4.0f, 4.0f,
                                           5.0f, INFINITY_F, 5.0f};

    const auto vertex_byte_vec = get_bytes(vertex_vec);

    Analysis::APIDumpAnalysis::Frontends::GeometrySamplingRange range;

    range.stride          = 3 * sizeof(float);
    range.vertex_count    = 6;
    range.vertex_data_ptr = vertex_byte_vec.data();
    range.vertex_n_bytes  = vertex_byte_vec.size();

    const auto sample_ptr = Analysis::APIDumpAnalysis::Frontends::GeometrySampler::sample(range);

    ASSERT_NE(sample_ptr,
              nullptr);
    ASSERT_EQ(sample_ptr->position_vec.size(),
              3u);
    EXPECT_EQ(sample_ptr->position_vec.at(2).at(0),
              4.0f);
}

TEST(GeometrySampler, WhatCannotBeToldGivesNoSample)
{
    const auto vertex_byte_vec = get_bytes(get_positions(4) );

    Analysis::APIDumpAnalysis::Frontends::GeometrySamplingRange range;

    range.stride          = 3 * sizeof(float);
    range.vertex_count    = 4;
    range.vertex_data_ptr = vertex_byte_vec.data();
    range.vertex_n_bytes  = vertex_byte_vec.size();

    /* An indexed draw with no data of indices has no vertices to read, and nothing is read through the pointer that is not there. */
    range.is_indexed   = true;
    range.index_stride = sizeof(uint16_t);

    EXPECT_EQ(Analysis::APIDumpAnalysis::Frontends::GeometrySampler::sample(range),
              nullptr);

    /* No vertices at all, a stride with no room for a position, and no data. */
    range.is_indexed   = false;
    range.vertex_count = 0;

    EXPECT_EQ(Analysis::APIDumpAnalysis::Frontends::GeometrySampler::sample(range),
              nullptr);

    range.vertex_count = 4;
    range.stride       = 2 * sizeof(float);

    EXPECT_EQ(Analysis::APIDumpAnalysis::Frontends::GeometrySampler::sample(range),
              nullptr);

    range.stride          = 3 * sizeof(float);
    range.vertex_data_ptr = nullptr;

    EXPECT_EQ(Analysis::APIDumpAnalysis::Frontends::GeometrySampler::sample(range),
              nullptr);
}
