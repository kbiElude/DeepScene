/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the mapping of the names and locations of GL objects from the dump to the replay.
 */
#include <gtest/gtest.h>
#include "gl_replay_context.h"

namespace
{
    /* A call of a function with the given kinds of parameters. */
    struct ContextFixture
    {
        Capture::APIDumpLoader::DumpedAPICall call;
        Replay::StereoReplayEngineDLL::CallWalker  walker;
        Replay::StereoReplayEngineDLL::GlFunctionInfo info;
        Replay::StereoReplayEngineDLL::ObjectRemapper remapper;
        Replay::StereoReplayEngineDLL::GlReplayContext context;

        ContextFixture()
            :context(&remapper)
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
        }
    };
}

TEST(GlReplayContext, UnknownNamesPassThrough)
{
    ContextFixture fixture;

    EXPECT_EQ(fixture.context.map_name(Replay::StereoReplayEngineDLL::ObjectKind::TEXTURE,
                                                     7),
                            7u);
}

TEST(GlReplayContext, NamesAreMappedPerKind)
{
    ContextFixture fixture;

    fixture.context.register_name(Replay::StereoReplayEngineDLL::ObjectKind::TEXTURE,
                                                1,
                                                5);

    EXPECT_EQ(fixture.context.map_name(Replay::StereoReplayEngineDLL::ObjectKind::TEXTURE,
                                                     1),
                            5u);
    EXPECT_EQ(fixture.context.map_name(Replay::StereoReplayEngineDLL::ObjectKind::BUFFER,
                                                     1),
                            1u);
}

TEST(GlReplayContext, ElementsOfAUniformArrayFollowTheFirstOne)
{
    ContextFixture fixture;

    fixture.context.set_current_program(3);
    fixture.context.register_uniform_location(3,
                                              4,
                                              10);

    EXPECT_EQ(fixture.context.map_uniform_location(4),
              10);
    EXPECT_EQ(fixture.context.map_uniform_location(6),
              12);
}

TEST(GlReplayContext, LocationsOfAnotherProgramAreNotUsed)
{
    ContextFixture fixture;

    fixture.context.register_uniform_location(3,
                                              4,
                                              10);
    fixture.context.set_current_program(8);

    EXPECT_EQ(fixture.context.map_uniform_location(4),
              4);
}

TEST(GlReplayContext, ARangeOfDisplayListsIsMappedOneByOne)
{
    ContextFixture fixture;

    fixture.info.return_kind = Replay::StereoReplayEngineDLL::ObjectKind::DISPLAY_LIST;

    fixture.call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_i32(3) );
    fixture.call.returned_value_ptr.reset(new APIInterceptor::APIFunctionArgument(APIInterceptor::APIFunctionArgument::create_u32(10) ));

    fixture.context.begin_call(&fixture.call,
                               &fixture.walker,
                               1,
                               &fixture.info);
    fixture.context.register_returned_name(100);

    EXPECT_EQ(fixture.context.map_name(Replay::StereoReplayEngineDLL::ObjectKind::DISPLAY_LIST,
                                                     10),
                            100u);
    EXPECT_EQ(fixture.context.map_name(Replay::StereoReplayEngineDLL::ObjectKind::DISPLAY_LIST,
                                                     12),
                            102u);
}
