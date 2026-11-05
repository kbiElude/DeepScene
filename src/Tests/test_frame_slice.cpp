/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for FrameSlicePlanner and SliceSkipper: which frames a replay that goes to a frame has to make, and that it leaves out the content of the others
 * and nothing else.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpDependencies/frame_slice_planner.h"
#include "slice_skipper.h"

namespace
{
    using Analysis::APIDumpDependencies::DependencyAnalysis;
    using Analysis::APIDumpDependencies::FrameRange;
    using Analysis::APIDumpDependencies::FrameSlicePlanner;
    using Analysis::APIDumpDependencies::ResourceDependencies;
    using Analysis::APIDumpDependencies::ResourceVersion;

    /* A version of a resource: the frame that wrote it (none if has_writer is false), whether it left some of the version before, and the frames
     * which use it.
     */
    ResourceVersion make_version(const bool&                  in_has_writer,
                                 const uint64_t&              in_writer_frame_index,
                                 const bool&                  in_keeps_earlier_content,
                                 const std::vector<FrameRange>& in_consumer_range_vec)
    {
        ResourceVersion result;

        result.consumer_range_vec    = in_consumer_range_vec;
        result.has_writer            = in_has_writer;
        result.keeps_earlier_content = in_keeps_earlier_content;
        result.writer_frame_index    = in_writer_frame_index;

        return result;
    }

    FrameRange make_range(const uint64_t& in_first_frame_index,
                          const uint64_t& in_last_frame_index)
    {
        FrameRange result;

        result.first_frame_index = in_first_frame_index;
        result.last_frame_index  = in_last_frame_index;

        return result;
    }

    DependencyAnalysis make_analysis(const uint64_t&                   in_n_frames,
                                     const std::vector<ResourceDependencies>& in_resource_vec)
    {
        DependencyAnalysis result;

        result.result.n_frames      = in_n_frames;
        result.result.resource_vec = in_resource_vec;

        return result;
    }

    ResourceDependencies make_resource(const std::vector<ResourceVersion>& in_version_vec)
    {
        ResourceDependencies result;

        result.version_vec = in_version_vec;

        return result;
    }

    std::vector<uint64_t> get_live_frames(const Analysis::APIDumpDependencies::FrameSlice& in_slice,
                                          const uint64_t&                                  in_n_frames)
    {
        std::vector<uint64_t> result_vec;

        for (uint64_t n_frame = 0;
                      n_frame < in_n_frames;
                    ++n_frame)
        {
            if (FrameSlicePlanner::is_live(in_slice,
                                           n_frame) )
            {
                result_vec.push_back(n_frame);
            }
        }

        return result_vec;
    }
}

TEST(FrameSlicePlanner, ARewriteOfTheWholeResourceEndsTheWayBack)
{
    /* Written whole in frames 1, 3 and 5, and each version used by the frame after it. */
    const auto analysis = make_analysis(8,
                                        {make_resource({make_version(false, 0, false, {}),
                                                        make_version(true,  1, false, {make_range(2, 2)}),
                                                        make_version(true,  3, false, {make_range(4, 4)}),
                                                        make_version(true,  5, false, {})})});

    /* Frame 4 uses what frame 3 wrote, and frame 3 replaces it all: frames 1 and 2 are not needed for 4 or any frame after it. */
    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      4),
                              8),
              (std::vector<uint64_t>{3, 4, 5, 6, 7}) );

    /* Nothing after frame 6 uses what the frames before wrote. */
    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      6),
                              8),
              (std::vector<uint64_t>{6, 7}) );
}

TEST(FrameSlicePlanner, APartialWriteKeepsTheVersionBeforeItNeeded)
{
    /* The write of frame 3 leaves some of what frame 1 wrote. */
    const auto analysis = make_analysis(8,
                                        {make_resource({make_version(false, 0, false, {}),
                                                        make_version(true,  1, false, {}),
                                                        make_version(true,  3, true,  {make_range(4, 4)})})});

    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      4),
                              8),
              (std::vector<uint64_t>{1, 3, 4, 5, 6, 7}) );
}

TEST(FrameSlicePlanner, WhatTheFramesAfterTheTargetReadIsMadeToo)
{
    /* A resource written in frame 0 and read by frames 5 to 7 (a texture which is loaded and then used): a replay that goes to frame 3, and goes on, needs it. */
    const auto analysis = make_analysis(8,
                                        {make_resource({make_version(false, 0, false, {}),
                                                        make_version(true,  0, false, {make_range(5, 7)})})});

    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      3),
                              8),
              (std::vector<uint64_t>{0, 3, 4, 5, 6, 7}) );
}

TEST(FrameSlicePlanner, WhatAFrameWhichIsMadeUsesIsMadeToo)
{
    /* Frame 4 uses what frame 3 wrote of the first resource, and frame 3 uses what frame 1 wrote of the second. */
    const auto analysis = make_analysis(6,
                                        {make_resource({make_version(false, 0, false, {}),
                                                        make_version(true,  3, false, {make_range(4, 4)})}),
                                         make_resource({make_version(false, 0, false, {}),
                                                        make_version(true,  1, false, {make_range(3, 3)})})});

    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      4),
                              6),
              (std::vector<uint64_t>{1, 3, 4, 5}) );
}

TEST(FrameSlicePlanner, AFrameThatTheAnalysisHasNotSeenIsLive)
{
    const auto analysis = make_analysis(4,
                                        {});
    const auto slice    = FrameSlicePlanner::plan(analysis,
                                                  2);

    EXPECT_FALSE(FrameSlicePlanner::is_live(slice,
                                            1) );
    EXPECT_TRUE (FrameSlicePlanner::is_live(slice,
                                            2) );
    EXPECT_TRUE (FrameSlicePlanner::is_live(slice,
                                            100) );
}

TEST(SliceSkipper, TheContentOfADeadFrameIsLeftOutAndTheStateIsNot)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;
    Capture::APIDumpLoader::DumpedAPICall   draw;
    Capture::APIDumpLoader::DumpedAPICall   state;
    const auto                              analysis = make_analysis(8,
                                                                     {make_resource({make_version(false, 0, false, {}),
                                                                                     make_version(true,  1, false, {})})});

    draw.func  = APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE;
    state.func = APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE;

    /* Without a slice nothing is left out. */
    EXPECT_TRUE(skipper.should_execute_call(draw,
                                            2) );
    EXPECT_FALSE(skipper.is_dead_frame(2) );

    skipper.set_slice(FrameSlicePlanner::plan(analysis,
                                              5) );

    EXPECT_TRUE (skipper.is_dead_frame(2) );
    EXPECT_FALSE(skipper.should_execute_call(draw,
                                             2) );
    EXPECT_TRUE (skipper.should_execute_call(state,
                                             2) );

    /* The frame which the replay goes to, and the frames after it, are made. */
    EXPECT_TRUE(skipper.should_execute_call(draw,
                                            5) );
    EXPECT_TRUE(skipper.should_execute_call(draw,
                                            7) );

    skipper.clear_slice();

    EXPECT_TRUE(skipper.should_execute_call(draw,
                                            2) );
}

TEST(SliceSkipper, ADrawAClearACopyAndAnUploadAreContentAndAStateObjectOrASceneCallIsNot)
{
    using Stereo::Replay::EngineDLL::SliceSkipper;

    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_LOCK) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DTEXTURE8_UNLOCKRECT) );

    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE) );
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTEXTURE) );
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_BEGINSCENE) );
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE) );
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP) );
}
