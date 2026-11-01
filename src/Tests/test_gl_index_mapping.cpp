/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the mapping of the indices that a GL program hands out (uniform blocks, subroutines, fragment outputs).
 */
#include <gtest/gtest.h>
#include "call_walker.h"
#include "gl_index_replayer.h"
#include "test_doubles.h"

TEST(GlIndexMapping, AnIndexIsMappedOnlyForTheProgramThatHandedItOut)
{
    Tests::GlIndexFixture fixture;

    fixture.context.register_index(Stereo::Replay::EngineDLL::ObjectKind::UNIFORM_BLOCK_INDEX,
                                   Tests::GlIndexFixture::PROGRAM,
                                   0, /* in_scope */
                                   2,
                                   7);

    EXPECT_EQ(fixture.context.map_index(Stereo::Replay::EngineDLL::ObjectKind::UNIFORM_BLOCK_INDEX,
                                        Tests::GlIndexFixture::PROGRAM,
                                        0,
                                        2),
              7u);
    EXPECT_EQ(fixture.context.map_index(Stereo::Replay::EngineDLL::ObjectKind::UNIFORM_BLOCK_INDEX,
                                        Tests::GlIndexFixture::PROGRAM + 1,
                                        0,
                                        2),
              2u);
}

TEST(GlIndexMapping, TheIndicesOfSubroutinesAreThoseOfTheShaderStage)
{
    Tests::GlIndexFixture fixture;

    fixture.context.register_index(Stereo::Replay::EngineDLL::ObjectKind::SUBROUTINE_INDEX,
                                   Tests::GlIndexFixture::PROGRAM,
                                   0x8B30, /* a fragment shader */
                                   1,
                                   3);

    EXPECT_EQ(fixture.context.map_index(Stereo::Replay::EngineDLL::ObjectKind::SUBROUTINE_INDEX,
                                        Tests::GlIndexFixture::PROGRAM,
                                        0x8B30,
                                        1),
              3u);
    EXPECT_EQ(fixture.context.map_index(Stereo::Replay::EngineDLL::ObjectKind::SUBROUTINE_INDEX,
                                        Tests::GlIndexFixture::PROGRAM,
                                        0x8B31, /* a vertex shader */
                                        1),
              1u);
}

TEST(GlIndexMapping, ElementsGoToThePositionsOfTheReplay)
{
    Tests::GlIndexFixture fixture;

    /* The dump's outputs 0 and 1 are the replay's 1 and 0. */
    fixture.context.register_index(Stereo::Replay::EngineDLL::ObjectKind::FRAG_DATA_LOCATION,
                                   Tests::GlIndexFixture::PROGRAM,
                                   0, /* in_scope */
                                   0,
                                   1);
    fixture.context.register_index(Stereo::Replay::EngineDLL::ObjectKind::FRAG_DATA_LOCATION,
                                   Tests::GlIndexFixture::PROGRAM,
                                   0, /* in_scope */
                                   1,
                                   0);

    const std::vector<uint32_t> dumped_vec = {100, 200};
    const auto                  live_vec   = fixture.replayer.permute(dumped_vec,
                                                                      Stereo::Replay::EngineDLL::ObjectKind::FRAG_DATA_LOCATION,
                                                                      0, /* in_scope */
                                                                      0);

    ASSERT_EQ(live_vec.size(),
              2u);
    EXPECT_EQ(live_vec.at(0),
              200u);
    EXPECT_EQ(live_vec.at(1),
              100u);
}

TEST(GlIndexMapping, ElementsWithoutAMapStayWhereTheyAre)
{
    Tests::GlIndexFixture fixture;

    const std::vector<uint32_t> dumped_vec = {10, 20, 30};

    EXPECT_EQ(fixture.replayer.permute(dumped_vec,
                                       Stereo::Replay::EngineDLL::ObjectKind::SUBROUTINE_UNIFORM_LOCATION,
                                       0,
                                       0),
              dumped_vec);
}

TEST(GlIndexMapping, AnElementThatTakesAFreePlaceDoesNotRunOverAMappedOne)
{
    Tests::GlIndexFixture fixture;

    /* Only the dump's output 0 is known: the replay's is 1. The output 1 of the dump has to find another place. */
    fixture.context.register_index(Stereo::Replay::EngineDLL::ObjectKind::FRAG_DATA_LOCATION,
                                   Tests::GlIndexFixture::PROGRAM,
                                   0, /* in_scope */
                                   0,
                                   1);

    const std::vector<uint32_t> dumped_vec = {100, 200, 300};
    const auto                  live_vec   = fixture.replayer.permute(dumped_vec,
                                                                      Stereo::Replay::EngineDLL::ObjectKind::FRAG_DATA_LOCATION,
                                                                      0, /* in_scope */
                                                                      0);

    ASSERT_EQ(live_vec.size(),
              3u);
    EXPECT_EQ(live_vec.at(1),
              100u);
    EXPECT_EQ(live_vec.at(2),
              200u);
}
