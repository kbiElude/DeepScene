/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for FrameSlicePlanner and SliceSkipper: which frames a replay that goes to a frame has to make, and that it leaves out the content of the others
 * and nothing else.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_dependencies/frame_slice_planner.h"
#include "common/macros.h"
#include "slice_skipper.h"
#include <stdlib.h>

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

namespace
{
    /* A texture that the frames fill one of four strips of, a strip each (frame F writes strip (F - first_strip_frame) % 4 of an extent of 40 x 10), with
     * the version that a frame writes used by the frame after it; the first version, which no frame wrote, is used by the first frame that writes. If the
     * first writer replaces all of the texture (an upload), the strips begin with the frame after it.
     */
    ResourceDependencies make_strip_resource(const uint64_t& in_first_writer_frame,
                                             const uint64_t& in_n_writer_frames,
                                             const bool&     in_first_writer_replaces_all,
                                             const bool&     in_extent_is_known,
                                             const uint32_t& in_extent_width)
    {
        ResourceDependencies result;
        const uint64_t       first_strip_frame = (in_first_writer_replaces_all) ? in_first_writer_frame + 1
                                                                                : in_first_writer_frame;

        result.version_vec.push_back(make_version(false,
                                                  0,
                                                  false,
                                                  {make_range(in_first_writer_frame,
                                                              in_first_writer_frame)}));

        for (uint64_t n_writer = 0;
                      n_writer < in_n_writer_frames;
                    ++n_writer)
        {
            const uint64_t frame_index = in_first_writer_frame + n_writer;
            const bool     is_upload   = (in_first_writer_replaces_all && n_writer == 0);
            ResourceVersion version    = make_version(true,
                                                      frame_index,
                                                      (is_upload == false),
                                                      {make_range(frame_index + 1,
                                                                  frame_index + 1)});

            version.has_inputs = true; /* what flows into a strip is not what the texture had */

            if (is_upload == false)
            {
                Analysis::APIDumpAnalysis::AccessRectangle strip;

                strip.height   = 10;
                strip.is_known = true;
                strip.width    = 10;
                strip.x        = static_cast<uint32_t>((frame_index - first_strip_frame) % 4) * 10;
                strip.y        = 0;

                version.written_rectangle_vec.push_back(strip);
            }

            result.version_vec.push_back(version);
        }

        result.written_extent.height   = 10;
        result.written_extent.is_known = in_extent_is_known;
        result.written_extent.width    = in_extent_width;

        return result;
    }
}

TEST(FrameSlicePlanner, TheWayBackEndsWhenTheLastStripsHaveWrittenAllThatAnyWriteReaches)
{
    /* Frames 1 to 8 write a strip each, and the frame after uses it: a frame that needs frame 6's version needs the strips of frames 3 to 6, which are all
     * four, and not those of frames 1 and 2.
     */
    const auto analysis = make_analysis(12,
                                        {make_strip_resource(1, 8, false, true, 40)});

    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      7),
                              12),
              (std::vector<uint64_t>{3, 4, 5, 6, 7, 8, 9, 10, 11}) );
}

TEST(FrameSlicePlanner, TheFrameThatLastReplacedAllOfTheTextureIsStillNeededForWhatTheStripsDoNotReach)
{
    /* Frame 1 uploads the whole texture and frames 2 to 8 write strips: the strips of frames 4 to 7 are all four, but what is outside the extent is what
     * frame 1 left.
     */
    const auto analysis = make_analysis(12,
                                        {make_strip_resource(1, 8, true, true, 40)});

    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      8),
                              12),
              (std::vector<uint64_t>{1, 4, 5, 6, 7, 8, 9, 10, 11}) );
}

TEST(FrameSlicePlanner, StripsThatDoNotWriteAllOfTheExtentLeaveTheWayBackAsItWas)
{
    /* The extent is wider than the four strips are. */
    const auto analysis = make_analysis(12,
                                        {make_strip_resource(1, 8, false, true, 50)});

    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      7),
                              12),
              (std::vector<uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}) );
}

TEST(FrameSlicePlanner, WithoutAnExtentTheWayBackIsAsItWas)
{
    const auto analysis = make_analysis(12,
                                        {make_strip_resource(1, 8, false, false, 40)});

    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      7),
                              12),
              (std::vector<uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}) );
}

TEST(FrameSlicePlanner, ATargetWithinTheFirstStripsNeedsThemAll)
{
    /* Frame 3 needs what frames 1 and 2 wrote, and they are not four strips yet. */
    const auto analysis = make_analysis(12,
                                        {make_strip_resource(1, 8, false, true, 40)});

    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      3),
                              12),
              (std::vector<uint64_t>{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}) );
}

namespace
{
    /* Three resources and what frame 2 does with them: it uses what frame 0 wrote of resource 3 and what frame 1 wrote of resource 2, and writes resource 1
     * (version 1), which frame 5 uses. What flows into version 1 is resource 2 only, if the version says so (has_inputs).
     */
    DependencyAnalysis make_inputs_analysis(const bool& in_has_inputs)
    {
        ResourceVersion written = make_version(true, 2, false, {make_range(5, 5)});

        written.has_inputs = in_has_inputs;

        if (in_has_inputs)
        {
            written.input_resource_vec.push_back(2);
        }

        return make_analysis(8,
                             {make_resource({make_version(false, 0, false, {}),
                                             written}),
                              make_resource({make_version(false, 0, false, {}),
                                             make_version(true,  1, false, {make_range(2, 2)})}),
                              make_resource({make_version(false, 0, false, {}),
                                             make_version(true,  0, false, {make_range(2, 2)})})});
    }
}

TEST(FrameSlicePlanner, AFrameThatIsMadeForAVersionIsGivenWhatFlowsIntoItAndNotAllThatItUses)
{
    /* Resource 3 is not an input of what frame 2 wrote, so what frame 0 wrote of it is not needed; what frame 1 wrote of resource 2 is. */
    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(make_inputs_analysis(true),
                                                      5),
                              8),
              (std::vector<uint64_t>{1, 2, 5, 6, 7}) );
}

TEST(FrameSlicePlanner, AFrameThatDoesNotSayWhatFlowsIntoAVersionIsGivenAllThatItUses)
{
    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(make_inputs_analysis(false),
                                                      5),
                              8),
              (std::vector<uint64_t>{0, 1, 2, 5, 6, 7}) );
}

TEST(FrameSlicePlanner, AFrameThatIsMadeInPartIsMadeInFullIfAnotherVersionOfItIsNeededInFull)
{
    /* Frame 2 also wrote version 1 of a fourth resource, which says nothing of what flows into it and which frame 6 uses. */
    DependencyAnalysis analysis = make_inputs_analysis(true);

    analysis.result.resource_vec.push_back(make_resource({make_version(false, 0, false, {}),
                                                          make_version(true,  2, false, {make_range(6, 6)})}));

    /* Target 6: it uses the version of the fourth resource, and that needs frame 2 in full, which needs what frames 0 and 1 wrote. */
    EXPECT_EQ(get_live_frames(FrameSlicePlanner::plan(analysis,
                                                      6),
                              8),
              (std::vector<uint64_t>{0, 1, 2, 6, 7}) );
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

TEST(SliceSkipper, TheChunksOfTheContentOfADeadFrameAreNotWantedAndItKeepsTheSliceItWasAskedFor)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;
    Capture::APIDumpLoader::DumpedAPICall   draw;
    Capture::APIDumpLoader::DumpedAPICall   state;
    const auto                              analysis = make_analysis(8,
                                                                     {make_resource({make_version(false, 0, false, {}),
                                                                                     make_version(true,  1, false, {})})});

    draw.func  = APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE;
    state.func = APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE;

    /* With no slice there is nothing to ask. */
    EXPECT_FALSE(skipper.get_chunk_wanted_func() );

    skipper.set_slice(FrameSlicePlanner::plan(analysis,
                                              5) );

    const auto wanted_func = skipper.get_chunk_wanted_func();

    ASSERT_TRUE(wanted_func);

    /* The same decisions as should_execute_call() makes, which is what the replay reads the chunks for. */
    EXPECT_FALSE(wanted_func(draw,
                             2) );
    EXPECT_TRUE (wanted_func(state,
                             2) );
    EXPECT_TRUE (wanted_func(draw,
                             5) );

    /* A slice which replaces it later does not change what it was asked for. */
    skipper.clear_slice();

    EXPECT_FALSE(wanted_func(draw,
                             2) );
}

namespace
{
    /* A call of GL with a number as its first argument (a list, a mode) and the other arguments left out, in the thread of the capture. */
    Capture::APIDumpLoader::DumpedAPICall make_gl_call(const APIInterceptor::APIFunction& in_function,
                                                       const int&                         in_first_number = 0)
    {
        Capture::APIDumpLoader::DumpedAPICall result;

        result.func = in_function;

        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_i32(in_first_number) );

        return result;
    }

    /* The slice that has frames 0 to 4 dead and the frames from 5 live. */
    void set_dead_until_frame_5(Stereo::Replay::EngineDLL::SliceSkipper* out_skipper_ptr)
    {
        const auto analysis = make_analysis(8,
                                            {make_resource({make_version(false, 0, false, {}),
                                                            make_version(true,  1, false, {})})});

        out_skipper_ptr->set_slice(FrameSlicePlanner::plan(analysis,
                                                           5) );
    }

    constexpr uint32_t DEAD_FRAME = 2;
    constexpr uint32_t LIVE_FRAME = 5;
    constexpr int      LIST       = 7;
}

TEST(SliceSkipper, TheDrawsAndTheClearsOfGLAreContentAndTheStateIsNot)
{
    using Stereo::Replay::EngineDLL::SliceSkipper;

    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLBEGIN) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLEND) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLCLEAR) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLREADPIXELS) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D) );
    EXPECT_TRUE (SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST) );

    /* The state and the objects stay: the frames after go on from them. */
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLENABLE) );
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE) );
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D) );
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY) );
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST) );
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) );

    /* The lists that a call names in an array are not known without reading it. */
    EXPECT_FALSE(SliceSkipper::is_content_call(APIInterceptor::APIFUNCTION_GL_GLCALLLISTS) );
}

TEST(SliceSkipper, TheDrawsOfAGLBlockInADeadFrameAreLeftOutAndTheStateIsMade)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    set_dead_until_frame_5(&skipper);

    const auto begin  = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLBEGIN);
    const auto vertex = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV);
    const auto end    = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLEND);
    const auto enable = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE);

    EXPECT_FALSE(skipper.should_execute_call(begin,  DEAD_FRAME) );
    EXPECT_FALSE(skipper.should_execute_call(vertex, DEAD_FRAME) );
    EXPECT_FALSE(skipper.should_execute_call(end,    DEAD_FRAME) );
    EXPECT_TRUE (skipper.should_execute_call(enable, DEAD_FRAME) );

    EXPECT_TRUE (skipper.should_execute_call(begin,  LIVE_FRAME) );
    EXPECT_TRUE (skipper.should_execute_call(vertex, LIVE_FRAME) );
}

TEST(SliceSkipper, TheChunksOfGLAreKeptSinceTheCallsOfAListThatIsBeingMadeAreNot)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    set_dead_until_frame_5(&skipper);

    const auto wanted_func = skipper.get_chunk_wanted_func();
    const auto vertex      = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV);

    ASSERT_TRUE(wanted_func);

    EXPECT_TRUE(wanted_func(vertex,
                            DEAD_FRAME) );
}

TEST(SliceSkipper, TheCallsOfAListThatIsBeingMadeInADeadFrameAreMadeSoThatTheListHasThem)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    set_dead_until_frame_5(&skipper);

    const auto new_list = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                       LIST);
    const auto vertex   = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV);
    const auto end_list = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST);

    EXPECT_TRUE (skipper.should_execute_call(new_list, DEAD_FRAME) );
    EXPECT_TRUE (skipper.should_execute_call(vertex,   DEAD_FRAME) );
    EXPECT_TRUE (skipper.should_execute_call(end_list, DEAD_FRAME) );

    /* The list is made; what is outside it is left out again. */
    EXPECT_FALSE(skipper.should_execute_call(vertex,   DEAD_FRAME) );
}

TEST(SliceSkipper, ACallOfAListThatOnlyDrawsIsLeftOutOfADeadFrameAndMadeInALiveOne)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    set_dead_until_frame_5(&skipper);

    const auto new_list  = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                        LIST);
    const auto begin     = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLBEGIN);
    const auto vertex    = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV);
    const auto end       = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLEND);
    const auto end_list  = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST);
    const auto call_list = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                                        LIST);

    skipper.should_execute_call(new_list, DEAD_FRAME);
    skipper.should_execute_call(begin,    DEAD_FRAME);
    skipper.should_execute_call(vertex,   DEAD_FRAME);
    skipper.should_execute_call(end,      DEAD_FRAME);
    skipper.should_execute_call(end_list, DEAD_FRAME);

    EXPECT_FALSE(skipper.should_execute_call(call_list, DEAD_FRAME) );
    EXPECT_TRUE (skipper.should_execute_call(call_list, LIVE_FRAME) );
}

TEST(SliceSkipper, ACallOfAListThatSetsAnythingButContentIsMadeInADeadFrameToo)
{
    /* A state, a value for the vertices that follow, an object: what the frames after it go on from, and so what it leaves is not to be taken away. */
    for (const auto function : {APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2F,
                                APIInterceptor::APIFUNCTION_GL_GLMATERIALFV,
                                APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE})
    {
        Stereo::Replay::EngineDLL::SliceSkipper skipper;

        set_dead_until_frame_5(&skipper);

        const auto new_list  = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                            LIST);
        const auto vertex    = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV);
        const auto sets      = make_gl_call(function);
        const auto end_list  = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST);
        const auto call_list = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                                            LIST);

        skipper.should_execute_call(new_list, DEAD_FRAME);
        skipper.should_execute_call(vertex,   DEAD_FRAME);
        skipper.should_execute_call(sets,     DEAD_FRAME);
        skipper.should_execute_call(end_list, DEAD_FRAME);

        EXPECT_TRUE(skipper.should_execute_call(call_list, DEAD_FRAME) );
        EXPECT_TRUE(skipper.should_execute_call(call_list, LIVE_FRAME) );
    }
}

TEST(SliceSkipper, ACallOfAListThatCallsOneThatSetsStateIsMadeInADeadFrameToo)
{
    constexpr int OUTER_LIST = 8;

    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    set_dead_until_frame_5(&skipper);

    /* The inner list sets a state; the outer one only draws and calls it. */
    skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                             LIST),
                                DEAD_FRAME);
    skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),
                                DEAD_FRAME);
    skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST),
                                DEAD_FRAME);
    skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                             OUTER_LIST),
                                DEAD_FRAME);
    skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                                             LIST),
                                DEAD_FRAME);
    skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST),
                                DEAD_FRAME);

    EXPECT_TRUE(skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                                                         OUTER_LIST),
                                            DEAD_FRAME) );
}

namespace
{
    /* A list of the number LIST with one call in it, and a call of the list, both in a frame that the slice leaves out. Ends the process if the skipper does. */
    void call_list_in_dead_frame(const APIInterceptor::APIFunction& in_function_in_list)
    {
        Stereo::Replay::EngineDLL::SliceSkipper skipper;

        set_dead_until_frame_5(&skipper);

        skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                                 LIST),
                                    DEAD_FRAME);
        skipper.should_execute_call(make_gl_call(in_function_in_list),
                                    DEAD_FRAME);
        skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST),
                                    DEAD_FRAME);
        skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                                                 LIST),
                                    DEAD_FRAME);
    }

    void set_no_error_dialog()
    {
        /* The process has no console when the tests run from a tool, and would wait for somebody to close the box. */
        ::_putenv_s("APIINTERCEPTOR_NO_ERROR_DIALOG",
                    "1");
    }
}

TEST(SliceSkipper, ACallOfAListThatLoadsAMatrixInADeadFrameEndsTheProcess)
{
    set_no_error_dialog();

    EXPECT_EXIT(call_list_in_dead_frame(APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "related to the location of the eye");
}

TEST(SliceSkipper, ACallOfAListThatMovesTheMatrixInADeadFrameEndsTheProcess)
{
    set_no_error_dialog();

    EXPECT_EXIT(call_list_in_dead_frame(APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "related to the location of the eye");
}

TEST(SliceSkipper, ACallOfAListThatIsNotDefinedInADeadFrameEndsTheProcess)
{
    set_no_error_dialog();

    EXPECT_EXIT({
                    Stereo::Replay::EngineDLL::SliceSkipper skipper;

                    set_dead_until_frame_5(&skipper);

                    skipper.should_execute_call(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                                                             LIST),
                                                DEAD_FRAME);
                },
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "related to the location of the eye");
}

TEST(SliceSkipper, TheReaderIsToldToDropTheGLContentOfDeadFramesAndNothingElse)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    /* With no slice there is nothing to drop. */
    EXPECT_FALSE(skipper.get_call_kept_func_factory() );

    set_dead_until_frame_5(&skipper);

    const auto factory = skipper.get_call_kept_func_factory();

    ASSERT_TRUE(factory);

    const auto kept_func = factory();
    const auto vertex    = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV);
    const auto enable    = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE);
    const auto call_list = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                                        LIST);
    const auto draw_7    = make_gl_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE);

    EXPECT_FALSE(kept_func(vertex,    DEAD_FRAME) );
    EXPECT_TRUE (kept_func(vertex,    LIVE_FRAME) );
    EXPECT_TRUE (kept_func(enable,    DEAD_FRAME) );

    /* A call of a list is for should_execute_call() to judge, which knows the list; Direct3D has its chunks left out and its calls made. */
    EXPECT_TRUE (kept_func(call_list, DEAD_FRAME) );
    EXPECT_TRUE (kept_func(draw_7,    DEAD_FRAME) );
}

TEST(SliceSkipper, TheCallsOfAListThatIsBeingMadeAreKeptByTheReaderToo)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    set_dead_until_frame_5(&skipper);

    const auto kept_func = skipper.get_call_kept_func_factory()();
    const auto new_list  = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                        LIST);
    const auto vertex    = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV);
    const auto end_list  = make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST);

    EXPECT_TRUE (kept_func(new_list, DEAD_FRAME) );
    EXPECT_TRUE (kept_func(vertex,   DEAD_FRAME) );
    EXPECT_TRUE (kept_func(end_list, DEAD_FRAME) );
    EXPECT_FALSE(kept_func(vertex,   DEAD_FRAME) );
}

TEST(SliceSkipper, TheReaderLeavesOutWhatAPopUndoesBetweenThePushAndThePopOfAFrameThatIsLeftOut)
{
    constexpr int ALL_ATTRIBUTE_BITS = 0xFFFFF;

    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    set_dead_until_frame_5(&skipper);

    const auto kept_func = skipper.get_call_kept_func_factory()();

    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),                         DEAD_FRAME) ); /* before the push */
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB, ALL_ATTRIBUTE_BITS), DEAD_FRAME) );

    /* What the pop undoes. */
    EXPECT_FALSE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),                         DEAD_FRAME) );
    EXPECT_FALSE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLDISABLE),                        DEAD_FRAME) );
    EXPECT_FALSE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLMATERIALFV),                     DEAD_FRAME) );
    EXPECT_FALSE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLCOLOR3FV),                       DEAD_FRAME) );
    EXPECT_FALSE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLTEXENVF),                        DEAD_FRAME) );
    EXPECT_FALSE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC),                      DEAD_FRAME) );
    EXPECT_FALSE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2F),                     DEAD_FRAME) );

    /* What the calls after them go by, what changes an object, and the state of the client, which a push of attributes does not save. */
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE),                  DEAD_FRAME) );
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE),                    DEAD_FRAME) );
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI),                  DEAD_FRAME) );
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE),                     DEAD_FRAME) );
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLECLIENTSTATE),              DEAD_FRAME) );
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLVERTEXPOINTER),                  DEAD_FRAME) );
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLCLIENTACTIVETEXTURE),            DEAD_FRAME) );

    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB),                      DEAD_FRAME) );

    /* After the pop what is set is what stays. */
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),                         DEAD_FRAME) );
}

TEST(SliceSkipper, AMaskThatDoesNotSaveWhatACallSetsLeavesItInAndANestedPushIsUndoneByItsOwnPop)
{
    constexpr int ALL_ATTRIBUTE_BITS = 0xFFFFF;
    constexpr int ENABLE_BIT         = 0x2000;

    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    set_dead_until_frame_5(&skipper);

    const auto kept_func = skipper.get_call_kept_func_factory()();

    /* A push that saves only the capabilities does not undo a material. */
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB, ENABLE_BIT), DEAD_FRAME) );
    EXPECT_FALSE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),                 DEAD_FRAME) );
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLMATERIALFV),             DEAD_FRAME) );

    /* One inside it that saves all: what it undoes is left out; what the pop of the one that saves less undoes is left in, as it is not known to be undone. */
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB, ALL_ATTRIBUTE_BITS), DEAD_FRAME) );
    EXPECT_FALSE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLMATERIALFV),                     DEAD_FRAME) );
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB),                      DEAD_FRAME) );
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLMATERIALFV),                     DEAD_FRAME) );
    EXPECT_FALSE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),                         DEAD_FRAME) );
    EXPECT_TRUE (kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB),                      DEAD_FRAME) );
}

TEST(SliceSkipper, NothingIsLeftOutOfAFrameThatIsMade)
{
    constexpr int ALL_ATTRIBUTE_BITS = 0xFFFFF;

    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    set_dead_until_frame_5(&skipper);

    const auto kept_func = skipper.get_call_kept_func_factory()();

    EXPECT_TRUE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB, ALL_ATTRIBUTE_BITS), LIVE_FRAME) );
    EXPECT_TRUE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),                         LIVE_FRAME) );
    EXPECT_TRUE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLMATERIALFV),                     LIVE_FRAME) );
}

TEST(SliceSkipper, AFrameThatIsLeftOutAndEndsBeforeItPopsWhatItPushedEndsTheProcess)
{
    constexpr int ALL_ATTRIBUTE_BITS = 0xFFFFF;

    set_no_error_dialog();

    EXPECT_EXIT({
                    Stereo::Replay::EngineDLL::SliceSkipper skipper;

                    set_dead_until_frame_5(&skipper);

                    const auto kept_func = skipper.get_call_kept_func_factory()();

                    kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB, ALL_ATTRIBUTE_BITS), 1);
                    kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),                         1);

                    /* The next frame begins: the state that the calls of the one before would have left is not made. */
                    kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),                         DEAD_FRAME);
                },
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "before it pops");
}

TEST(SliceSkipper, APopWhosePushWasInTheSegmentBeforeLeavesNothingOut)
{
    Stereo::Replay::EngineDLL::SliceSkipper skipper;

    set_dead_until_frame_5(&skipper);

    const auto kept_func = skipper.get_call_kept_func_factory()();

    /* The segment begins in the middle of a pair: what comes before the pop is not known to be undone by it. */
    EXPECT_TRUE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),    DEAD_FRAME) );
    EXPECT_TRUE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB), DEAD_FRAME) );
    EXPECT_TRUE(kept_func(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENABLE),    DEAD_FRAME) );
}

TEST(SliceSkipper, AListThatIsMadeAcrossTwoSegmentsEndsTheProcess)
{
    set_no_error_dialog();

    EXPECT_EXIT({
                    Stereo::Replay::EngineDLL::SliceSkipper skipper;

                    set_dead_until_frame_5(&skipper);

                    /* The segment is the one in which the list ends: its beginning was in the one before. */
                    skipper.get_call_kept_func_factory()()(make_gl_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST),
                                                           DEAD_FRAME);
                },
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "across two segments");
}

TEST(SliceSkipper, ACallOfAListThatMakesALightRelativeToTheMatrixInADeadFrameEndsTheProcess)
{
    set_no_error_dialog();

    EXPECT_EXIT(call_list_in_dead_frame(APIInterceptor::APIFUNCTION_GL_GLLIGHTFV),
                ::testing::ExitedWithCode(APIInterceptor::FatalError::get_exit_code() ),
                "related to the location of the eye");
}
