/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the resources of the data that draws read from the memory of the application: the same bytes are one resource whatever the draw or
 * the frame, other bytes (or the same bytes of another kind of buffer) are another resource, and no bytes are kept.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_client_array_resources.h"
#include "analysis/api_dump_analysis/model_builder.h"

namespace
{
    class GLClientArrayResourcesTest : public ::testing::Test
    {
    protected:
        GLClientArrayResourcesTest()
            :m_resources(&m_builder)
        {
            /* Stub */
        }

        Analysis::APIDumpAnalysis::ResourceId get_vertex_resource(const std::vector<uint8_t>& in_data_u8vec)
        {
            return m_resources.get_resource_id(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX,
                                               in_data_u8vec.data(),
                                               in_data_u8vec.size() );
        }

        Analysis::APIDumpAnalysis::ModelBuilder                      m_builder;
        Analysis::APIDumpAnalysis::Frontends::GLClientArrayResources m_resources;
    };
}

TEST_F(GLClientArrayResourcesTest, TheSameBytesAreTheSameResource)
{
    const auto first  = get_vertex_resource({1, 2, 3, 4});
    const auto second = get_vertex_resource({1, 2, 3, 4});
    const auto other  = get_vertex_resource({1, 2, 3, 5});

    EXPECT_EQ(first,
              second);
    EXPECT_NE(first,
              other);
    EXPECT_EQ(m_resources.get_n_resources(),
              2u);
}

TEST_F(GLClientArrayResourcesTest, TheKindOfBufferTellsTheSameBytesApart)
{
    const std::vector<uint8_t> data_u8vec = {1, 2, 3, 4};

    const auto vertices = get_vertex_resource(data_u8vec);
    const auto indices  = m_resources.get_resource_id(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_INDEX,
                                                      data_u8vec.data(),
                                                      data_u8vec.size() );

    EXPECT_NE(vertices,
              indices);
    EXPECT_EQ(m_resources.get_n_resources(),
              2u);
}

TEST_F(GLClientArrayResourcesTest, TheSameBytesSeenManyTimesAreOneResource)
{
    const auto first = get_vertex_resource({7, 7, 7, 7});

    for (uint32_t n_time = 0;
                  n_time < 1000;
                ++n_time)
    {
        EXPECT_EQ(get_vertex_resource({7, 7, 7, 7}),
                  first);
    }

    EXPECT_EQ(m_resources.get_n_resources(),
              1u);
}

TEST_F(GLClientArrayResourcesTest, TheNumberOfBytesTellsBytesThatBeginTheSameApart)
{
    const auto short_data = get_vertex_resource({1, 2, 3, 4});
    const auto long_data  = get_vertex_resource({1, 2, 3, 4, 0, 0, 0, 0});

    EXPECT_NE(short_data,
              long_data);
}
