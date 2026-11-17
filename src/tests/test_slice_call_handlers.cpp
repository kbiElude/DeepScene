/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests that what is only for one API is done for that API only: the handlers of SliceSkipper, and the APIs that interceptors ask for.
 */
#include <gtest/gtest.h>
#include "content_slice_call_handler.h"
#include "gl_slice_call_handler.h"
#include "capture/api_dump_loader/api_info.h"
#include "gl_texture_parameter_cache.h"
#include "present_skipper.h"
#include "slice_skipper.h"
#include <algorithm>

namespace
{
    using Capture::APIDumpLoader::APIGroup;

    Capture::APIDumpLoader::DumpedAPICall make_call(const APIInterceptor::APIFunction& in_function)
    {
        Capture::APIDumpLoader::DumpedAPICall result;

        result.func = in_function;

        return result;
    }

    bool has_group(const std::vector<APIGroup>& in_group_vec,
                   const APIGroup&              in_group)
    {
        return (std::find(in_group_vec.begin(),
                          in_group_vec.end  (),
                          in_group) != in_group_vec.end() );
    }
}

TEST(SliceCallHandlers, TheSkipperAsksForTheCallsOfTheApisThatHaveAHandlerOnly)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    const auto group_vec = skipper.get_api_groups();

    EXPECT_EQ(group_vec.size(),
              3u);
    EXPECT_TRUE (has_group(group_vec,
                           APIGroup::D3D7) );
    EXPECT_TRUE (has_group(group_vec,
                           APIGroup::D3D8) );
    EXPECT_TRUE (has_group(group_vec,
                           APIGroup::GL) );
    EXPECT_FALSE(has_group(group_vec,
                           APIGroup::WGL) );
    EXPECT_FALSE(has_group(group_vec,
                           APIGroup::GDI32) );
}

TEST(SliceCallHandlers, TheTextureParameterCacheAsksForOpenGLAndItsContextsOnly)
{
    Stereo::Replay::EngineDLL::GLTextureParameterCache cache;

    const auto group_vec = cache.get_api_groups();

    EXPECT_EQ(group_vec.size(),
              2u);
    EXPECT_TRUE(has_group(group_vec,
                          APIGroup::GL) );
    EXPECT_TRUE(has_group(group_vec,
                          APIGroup::WGL) );
}

TEST(SliceCallHandlers, ThePresentSkipperAsksForEveryApiWhoseCallsCanEndAFrame)
{
    Stereo::Replay::EngineDLL::HurriedFrameSelector hurry;
    Stereo::Replay::EngineDLL::PresentSkipper       skipper(&hurry);

    const auto group_vec = skipper.get_api_groups();

    /* The functions that is_frame_boundary() can say end a frame, by the API they are of. */
    for (const auto& function : {APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS,
                                 APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP,
                                 APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT,
                                 APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLTFAST,
                                 APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT})
    {
        EXPECT_TRUE(has_group(group_vec,
                              Capture::APIDumpLoader::get_api_group(function) ) );
    }

    /* The calls of OpenGL, which are most of the calls of a workload, never end a frame. */
    EXPECT_FALSE(has_group(group_vec,
                           APIGroup::GL) );
}

TEST(SliceCallHandlers, ACallOfAnApiWithNoHandlerIsAlwaysExecuted)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    Analysis::APIDumpDependencies::FrameSlice slice;

    slice.is_live_vec.assign(10,
                             false);

    skipper.set_slice(slice);

    EXPECT_TRUE(skipper.should_execute_call(make_call(APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS),
                                            3) );
}

TEST(SliceCallHandlers, TheContentHandlerLeavesOutContentInADeadFrameAndNothingElse)
{
    Stereo::Replay::EngineDLL::ContentSliceCallHandler handler(APIGroup::D3D8);

    const auto draw_call  = make_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE);
    const auto state_call = make_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETRENDERSTATE);

    EXPECT_EQ(handler.get_group(),
              APIGroup::D3D8);

    EXPECT_FALSE(handler.should_execute_call(draw_call,
                                             true) );
    EXPECT_TRUE (handler.should_execute_call(draw_call,
                                             false) );
    EXPECT_TRUE (handler.should_execute_call(state_call,
                                             true) );

    EXPECT_FALSE(handler.is_chunk_wanted_in_dead_frame(draw_call) );
    EXPECT_TRUE (handler.is_chunk_wanted_in_dead_frame(state_call) );

    /* The reader drops nothing of such an API by itself: the state frames of the index already have no content. */
    const auto always_live_func = [](const uint64_t&)
    {
        return false;
    };

    EXPECT_FALSE(static_cast<bool>(handler.create_call_kept_func(always_live_func) ) );
}

TEST(SliceCallHandlers, TheOpenGLHandlerKeepsTheChunksOfEveryCallAndMakesAFunctionForTheReader)
{
    Stereo::Replay::EngineDLL::GLSliceCallHandler handler;

    const auto always_dead_func = [](const uint64_t&)
    {
        return false;
    };

    EXPECT_EQ(handler.get_group(),
              APIGroup::GL);
    EXPECT_TRUE(handler.is_chunk_wanted_in_dead_frame(make_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV) ) );

    auto func = handler.create_call_kept_func(always_dead_func);

    ASSERT_TRUE(static_cast<bool>(func) );

    EXPECT_FALSE(func(make_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV),
                      0) );
    EXPECT_TRUE (func(make_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),
                      0) );
}
