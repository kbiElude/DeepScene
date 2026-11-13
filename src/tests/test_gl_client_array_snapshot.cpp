/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the reading of the snapshot of the arrays that the dumper makes at a draw: the arrays it holds, and the positions of their
 * elements.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_client_array_snapshot.h"
#include "gl_call_builder.h"

namespace
{
    typedef Analysis::APIDumpAnalysis::Frontends::GLClientArraySnapshot Snapshot;

    Capture::APIDumpLoader::DataChunk make_chunk(const std::vector<Tests::GLCallBuilder::ClientArray>& in_array_vec)
    {
        const auto snapshot_u8vec = Tests::GLCallBuilder::make_snapshot(in_array_vec);

        return Capture::APIDumpLoader::DataChunk(snapshot_u8vec.begin(),
                                                 snapshot_u8vec.end() );
    }
}

TEST(GLClientArraySnapshot, AParsedSnapshotHasItsArrays)
{
    auto vertices = Tests::GLCallBuilder::make_vertex_array({{1.0f, 2.0f, 3.0f},
                                                             {4.0f, 5.0f, 6.0f} });

    vertices.first_element = 10;

    const auto                                                         chunk = make_chunk({vertices});
    std::vector<Analysis::APIDumpAnalysis::Frontends::GLClientArray>   array_vec;

    ASSERT_TRUE(Snapshot::parse(chunk,
                                &array_vec) );
    ASSERT_EQ  (array_vec.size(),
                1u);

    EXPECT_EQ(array_vec.at(0).kind,
              Analysis::APIDumpAnalysis::Frontends::GLClientArrayKind::VERTEX);
    EXPECT_EQ(array_vec.at(0).first_element,
              10u);
    EXPECT_EQ(array_vec.at(0).n_elements,
              2u);
    EXPECT_EQ(array_vec.at(0).n_bytes,
              24u);
}

TEST(GLClientArraySnapshot, ASnapshotWhichIsShorterThanItSaysIsNotParsed)
{
    const auto vertices = Tests::GLCallBuilder::make_vertex_array({{1.0f, 2.0f, 3.0f} });
    auto       chunk    = make_chunk({vertices});

    chunk.resize(chunk.size() - 1);

    std::vector<Analysis::APIDumpAnalysis::Frontends::GLClientArray> array_vec;

    EXPECT_FALSE(Snapshot::parse(chunk,
                                 &array_vec) );
}

TEST(GLClientArraySnapshot, ThePositionOfAnElementIsReadFromWhereItsIndexPutsIt)
{
    auto vertices = Tests::GLCallBuilder::make_vertex_array({{1.0f, 2.0f, 3.0f},
                                                             {4.0f, 5.0f, 6.0f} });

    vertices.first_element = 10; /* the data starts at the element 10 */

    const auto                                                       chunk = make_chunk({vertices});
    std::vector<Analysis::APIDumpAnalysis::Frontends::GLClientArray> array_vec;
    std::array<float, 3>                                             position = {};

    ASSERT_TRUE(Snapshot::parse(chunk,
                                &array_vec) );

    EXPECT_TRUE(Snapshot::read_position(array_vec.at(0),
                                        11,
                                        &position) );
    EXPECT_EQ  (position,
                (std::array<float, 3>{4.0f, 5.0f, 6.0f}) );

    /* Elements before and after the copy were not copied. */
    EXPECT_FALSE(Snapshot::read_position(array_vec.at(0),
                                         9,
                                         &position) );
    EXPECT_FALSE(Snapshot::read_position(array_vec.at(0),
                                         12,
                                         &position) );
}

TEST(GLClientArraySnapshot, AStrideSaysHowFarApartTheElementsAre)
{
    auto vertices = Tests::GLCallBuilder::make_vertex_array({{1.0f, 2.0f, 3.0f},
                                                             {4.0f, 5.0f, 6.0f} });

    vertices.stride = 12;

    const auto                                                       chunk = make_chunk({vertices});
    std::vector<Analysis::APIDumpAnalysis::Frontends::GLClientArray> array_vec;
    std::array<float, 3>                                             position = {};

    ASSERT_TRUE(Snapshot::parse(chunk,
                                &array_vec) );

    EXPECT_TRUE(Snapshot::read_position(array_vec.at(0),
                                        1,
                                        &position) );
    EXPECT_EQ  (position,
                (std::array<float, 3>{4.0f, 5.0f, 6.0f}) );
}

TEST(GLClientArraySnapshot, ATwoComponentPositionHasAZeroZ)
{
    auto vertices = Tests::GLCallBuilder::make_vertex_array({{7.0f, 8.0f, 9.0f} });

    vertices.size = 2;

    const auto                                                       chunk = make_chunk({vertices});
    std::vector<Analysis::APIDumpAnalysis::Frontends::GLClientArray> array_vec;
    std::array<float, 3>                                             position = {1.0f, 1.0f, 1.0f};

    ASSERT_TRUE(Snapshot::parse(chunk,
                                &array_vec) );

    EXPECT_TRUE(Snapshot::read_position(array_vec.at(0),
                                        0,
                                        &position) );
    EXPECT_EQ  (position,
                (std::array<float, 3>{7.0f, 8.0f, 0.0f}) );
}
