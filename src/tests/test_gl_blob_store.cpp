/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the store of the data that draws read from the memory of the application: a piece is one resource whatever the frame, its bytes are
 * kept for the frame that it is seen in, and on if it is seen in more than one.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_blob_store.h"
#include "analysis/api_dump_analysis/model_builder.h"

namespace
{
    typedef Analysis::APIDumpAnalysis::Frontends::GlBlobReuse GlBlobReuse;

    class GlBlobStoreTest : public ::testing::Test
    {
    protected:
        GlBlobStoreTest()
            :m_store(&m_builder)
        {
            /* Stub */
        }

        Analysis::APIDumpAnalysis::ResourceId get_vertex_resource(const std::vector<uint8_t>& in_data_u8vec)
        {
            return m_store.get_resource_id(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX,
                                           in_data_u8vec.data(),
                                           in_data_u8vec.size() );
        }

        Analysis::APIDumpAnalysis::ModelBuilder           m_builder;
        Analysis::APIDumpAnalysis::Frontends::GlBlobStore m_store;
    };
}

TEST_F(GlBlobStoreTest, TheSameBytesAreTheSameResource)
{
    const auto first  = get_vertex_resource({1, 2, 3, 4});
    const auto second = get_vertex_resource({1, 2, 3, 4});
    const auto other  = get_vertex_resource({1, 2, 3, 5});

    EXPECT_EQ(first,
              second);
    EXPECT_NE(first,
              other);
    EXPECT_EQ(m_store.get_n_blobs(),
              2u);
}

TEST_F(GlBlobStoreTest, TheKindOfBufferTellsTheSameBytesApart)
{
    const std::vector<uint8_t> data_u8vec = {1, 2, 3, 4};

    const auto vertices = get_vertex_resource(data_u8vec);
    const auto indices  = m_store.get_resource_id(Analysis::APIDumpAnalysis::ResourceKind::BUFFER_INDEX,
                                                  data_u8vec.data(),
                                                  data_u8vec.size() );

    EXPECT_NE(vertices,
              indices);
}

TEST_F(GlBlobStoreTest, TheBytesAreKeptForTheFrameThatTheyAreSeenIn)
{
    const auto resource = get_vertex_resource({1, 2, 3, 4});
    const auto data_ptr = m_store.get_data_ptr(resource);

    ASSERT_NE(data_ptr,
              nullptr);
    EXPECT_EQ(*data_ptr,
              (std::vector<uint8_t>{1, 2, 3, 4}) );
    EXPECT_EQ(m_store.get_n_cached_bytes(),
              4u);
}

TEST_F(GlBlobStoreTest, WhatWasSeenInOneFrameOnlyIsLetGoOfAtItsEnd)
{
    const auto resource = get_vertex_resource({1, 2, 3, 4});

    m_store.on_frame_ended();

    EXPECT_EQ(m_store.get_data_ptr(resource),
              nullptr);
    EXPECT_EQ(m_store.get_reuse(resource),
              GlBlobReuse::ONE_FRAME);
    EXPECT_EQ(m_store.get_n_cached_bytes(),
              0u);
}

TEST_F(GlBlobStoreTest, WhatIsSeenInTwoFramesIsReuseAndIsKeptOn)
{
    const auto resource = get_vertex_resource({1, 2, 3, 4});

    m_store.on_frame_ended();

    const auto again = get_vertex_resource({1, 2, 3, 4});

    EXPECT_EQ(resource,
              again);
    EXPECT_EQ(m_store.get_reuse(resource),
              GlBlobReuse::ACROSS_FRAMES);

    m_store.on_frame_ended();

    EXPECT_NE(m_store.get_data_ptr(resource),
              nullptr);
    EXPECT_EQ(m_store.get_n_cached_bytes(),
              4u);
}

TEST_F(GlBlobStoreTest, TheSameBytesSeenManyTimesInAFrameAreOneFrame)
{
    const auto resource = get_vertex_resource({1, 2, 3, 4});

    get_vertex_resource({1, 2, 3, 4});
    get_vertex_resource({1, 2, 3, 4});

    EXPECT_EQ(m_store.get_reuse(resource),
              GlBlobReuse::ONE_FRAME);
    EXPECT_EQ(m_store.get_n_cached_bytes(),
              4u);
}

TEST_F(GlBlobStoreTest, WhatWasLetGoOfIsKeptAgainWhenItIsSeenAgain)
{
    const auto resource = get_vertex_resource({1, 2, 3, 4});

    m_store.on_frame_ended();
    m_store.on_frame_ended();

    get_vertex_resource({1, 2, 3, 4});

    EXPECT_NE(m_store.get_data_ptr(resource),
              nullptr);
    EXPECT_EQ(m_store.get_reuse(resource),
              GlBlobReuse::ACROSS_FRAMES);
}

TEST_F(GlBlobStoreTest, AResourceOfNoStoreHasNoBytesAndNoReuse)
{
    const auto unknown = static_cast<Analysis::APIDumpAnalysis::ResourceId>(12345);

    EXPECT_EQ(m_store.get_data_ptr(unknown),
              nullptr);
    EXPECT_EQ(m_store.get_reuse(unknown),
              GlBlobReuse::ONE_FRAME);
}
