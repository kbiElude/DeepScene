/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the mapping of the indices that a GL program hands out (uniform blocks, subroutines, fragment outputs).
 */
#include <gtest/gtest.h>
#include "gl_index_replayer.h"

namespace
{
    constexpr uint64_t PROGRAM = 5;

    struct IndexFixture
    {
        Capture::APIDumpLoader::DumpedAPICall     call;
        Replay::StereoReplayEngineDLL::GlFunctionInfo  info;
        Replay::StereoReplayEngineDLL::ObjectRemapper  remapper;
        Replay::StereoReplayEngineDLL::GlReplayContext context;
        Replay::StereoReplayEngineDLL::GlIndexReplayer replayer;
        Replay::StereoReplayEngineDLL::CallWalker      walker;

        IndexFixture()
            :context (&remapper),
             replayer(&context,
                      [](const char*) -> void*
                      {
                          return nullptr;
                      })
        {
            info.has_explicit_program = false;
            info.ignored_param_mask   = 0;
            info.name_ptr             = "glTest";
            info.return_kind          = Replay::StereoReplayEngineDLL::ObjectKind::NONE;

            call.func = APIInterceptor::APIFUNCTION_GL_GLACCUM;

            context.begin_call(&call,
                               &walker,
                               0,
                               &info);
            context.set_current_program(PROGRAM);
        }
    };
}

TEST(GlIndexMapping, AnIndexIsMappedOnlyForTheProgramThatHandedItOut)
{
    IndexFixture fixture;

    fixture.context.register_index(Replay::StereoReplayEngineDLL::ObjectKind::UNIFORM_BLOCK_INDEX,
                                                 PROGRAM,
                                                 0, /* in_scope */
                                                 2,
                                                 7);

    EXPECT_EQ(fixture.context.map_index(Replay::StereoReplayEngineDLL::ObjectKind::UNIFORM_BLOCK_INDEX,
                                                      PROGRAM,
                                                      0,
                                                      2),
                            7u);
    EXPECT_EQ(fixture.context.map_index(Replay::StereoReplayEngineDLL::ObjectKind::UNIFORM_BLOCK_INDEX,
                                                      PROGRAM + 1,
                                                      0,
                                                      2),
                            2u);
}

TEST(GlIndexMapping, TheIndicesOfSubroutinesAreThoseOfTheShaderStage)
{
    IndexFixture fixture;

    fixture.context.register_index(Replay::StereoReplayEngineDLL::ObjectKind::SUBROUTINE_INDEX,
                                                 PROGRAM,
                                                 0x8B30, /* a fragment shader */
                                                 1,
                                                 3);

    EXPECT_EQ(fixture.context.map_index(Replay::StereoReplayEngineDLL::ObjectKind::SUBROUTINE_INDEX,
                                                      PROGRAM,
                                                      0x8B30,
                                                      1),
                            3u);
    EXPECT_EQ(fixture.context.map_index(Replay::StereoReplayEngineDLL::ObjectKind::SUBROUTINE_INDEX,
                                                      PROGRAM,
                                                      0x8B31, /* a vertex shader */
                                                      1),
                            1u);
}

TEST(GlIndexMapping, ElementsGoToThePositionsOfTheReplay)
{
    IndexFixture fixture;

    /* The dump's outputs 0 and 1 are the replay's 1 and 0. */
    fixture.context.register_index(Replay::StereoReplayEngineDLL::ObjectKind::FRAG_DATA_LOCATION,
                                                 PROGRAM,
                                                 0, /* in_scope */
                                                 0,
                                                 1);
    fixture.context.register_index(Replay::StereoReplayEngineDLL::ObjectKind::FRAG_DATA_LOCATION,
                                                 PROGRAM,
                                                 0, /* in_scope */
                                                 1,
                                                 0);

    const std::vector<uint32_t> dumped_vec = {100, 200};
    const auto                  live_vec   = fixture.replayer.permute(dumped_vec,
                                                                      Replay::StereoReplayEngineDLL::ObjectKind::FRAG_DATA_LOCATION,
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
    IndexFixture fixture;

    const std::vector<uint32_t> dumped_vec = {10, 20, 30};

    EXPECT_EQ(fixture.replayer.permute(dumped_vec,
                                       Replay::StereoReplayEngineDLL::ObjectKind::SUBROUTINE_UNIFORM_LOCATION,
                                       0,
                                       0),
              dumped_vec);
}

TEST(GlIndexMapping, AnElementThatTakesAFreePlaceDoesNotRunOverAMappedOne)
{
    IndexFixture fixture;

    /* Only the dump's output 0 is known: the replay's is 1. The output 1 of the dump has to find another place. */
    fixture.context.register_index(Replay::StereoReplayEngineDLL::ObjectKind::FRAG_DATA_LOCATION,
                                                 PROGRAM,
                                                 0, /* in_scope */
                                                 0,
                                                 1);

    const std::vector<uint32_t> dumped_vec = {100, 200, 300};
    const auto                  live_vec   = fixture.replayer.permute(dumped_vec,
                                                                      Replay::StereoReplayEngineDLL::ObjectKind::FRAG_DATA_LOCATION,
                                                                      0, /* in_scope */
                                                                      0);

    ASSERT_EQ(live_vec.size(),
              3u);
    EXPECT_EQ(live_vec.at(1),
              100u);
    EXPECT_EQ(live_vec.at(2),
              200u);
}
