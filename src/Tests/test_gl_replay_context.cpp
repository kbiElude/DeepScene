/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the mapping of the names and locations of GL objects from the dump to the replay.
 */
#include <gtest/gtest.h>
#include "call_walker.h"
#include "gl_replay_context.h"
#include "test_doubles.h"

TEST(GlReplayContext, UnknownNamesPassThrough)
{
    Tests::GlContextFixture fixture;

    EXPECT_EQ(fixture.context.map_name(Stereo::Replay::EngineDLL::ObjectKind::TEXTURE,
                                       7),
              7u);
}

TEST(GlReplayContext, NamesAreMappedPerKind)
{
    Tests::GlContextFixture fixture;

    fixture.context.register_name(Stereo::Replay::EngineDLL::ObjectKind::TEXTURE,
                                  1,
                                  5);

    EXPECT_EQ(fixture.context.map_name(Stereo::Replay::EngineDLL::ObjectKind::TEXTURE,
                                       1),
              5u);
    EXPECT_EQ(fixture.context.map_name(Stereo::Replay::EngineDLL::ObjectKind::BUFFER,
                                       1),
              1u);
}

TEST(GlReplayContext, ElementsOfAUniformArrayFollowTheFirstOne)
{
    Tests::GlContextFixture fixture;

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
    Tests::GlContextFixture fixture;

    fixture.context.register_uniform_location(3,
                                              4,
                                              10);
    fixture.context.set_current_program(8);

    EXPECT_EQ(fixture.context.map_uniform_location(4),
              4);
}

TEST(GlReplayContext, ARangeOfDisplayListsIsMappedOneByOne)
{
    Tests::GlContextFixture fixture;

    fixture.info.return_kind = Stereo::Replay::EngineDLL::ObjectKind::DISPLAY_LIST;

    fixture.call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_i32(3) );
    fixture.call.returned_value_ptr.reset(new APIInterceptor::APIFunctionArgument(APIInterceptor::APIFunctionArgument::create_u32(10) ));

    fixture.context.begin_call(&fixture.call,
                               &fixture.walker,
                               1,
                               &fixture.info);
    fixture.context.register_returned_name(100);

    EXPECT_EQ(fixture.context.map_name(Stereo::Replay::EngineDLL::ObjectKind::DISPLAY_LIST,
                                       10),
              100u);
    EXPECT_EQ(fixture.context.map_name(Stereo::Replay::EngineDLL::ObjectKind::DISPLAY_LIST,
                                       12),
              102u);
}
