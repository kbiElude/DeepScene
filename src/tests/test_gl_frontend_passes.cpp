/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end following the contexts, the window, the clears and the frames.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "gl_frontend_fixture.h"

namespace
{
    class GlFrontendPasses : public Tests::GlFrontendFixture
    {
    };
}

TEST_F(GlFrontendPasses, NothingIsMadeBeforeTheFirstClear)
{
    start(640,
          480);
    feed ();

    EXPECT_TRUE(m_harness.get_model().resource_vec.empty() );
}

TEST_F(GlFrontendPasses, TheWindowIsMadeAtTheFirstClearWithTheSizeOfTheViewport)
{
    start(640,
          480);

    m_builder.clear(COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);

    feed();

    const auto& resource_vec = m_harness.get_model().resource_vec;

    ASSERT_EQ(resource_vec.size(),
              2u);

    EXPECT_EQ  (resource_vec.at(0).description.kind,
                Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE);
    EXPECT_TRUE(resource_vec.at(0).description.is_render_target);
    EXPECT_EQ  (resource_vec.at(0).description.width,
                640u);
    EXPECT_EQ  (resource_vec.at(0).description.height,
                480u);

    EXPECT_TRUE(resource_vec.at(1).description.is_depth_stencil);
    EXPECT_EQ  (resource_vec.at(1).description.format.format_class,
                Analysis::APIDumpAnalysis::FormatClass::DEPTH_STENCIL);
    EXPECT_EQ  (resource_vec.at(1).description.width,
                640u);
}

TEST_F(GlFrontendPasses, TheWindowIsMadeOnce)
{
    start(640,
          480);

    m_builder.clear(COLOR_BUFFER_BIT);
    m_builder.clear(DEPTH_BUFFER_BIT);

    feed();

    EXPECT_EQ(m_harness.get_model().resource_vec.size(),
              2u);
}

TEST_F(GlFrontendPasses, ASwapBuffersEndsAFrame)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);
    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().frame_record_vec.size(),
              2u);
}

TEST_F(GlFrontendPasses, AClearIsAPassWithTheViewportOfTheContext)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    ASSERT_EQ(model.pass_structure_interner.get_n_structures(),
              1u);

    const auto& structure = model.pass_structure_interner.get_structure(0);

    EXPECT_TRUE(structure.clears_color);
    EXPECT_TRUE(structure.clears_depth);
    EXPECT_FALSE(structure.clears_stencil);
    EXPECT_EQ  (structure.viewport.width,
                640.0f);
    EXPECT_EQ  (structure.viewport.height,
                480.0f);
}

TEST_F(GlFrontendPasses, TheLastViewportBeforeTheFirstClearIsTheOneOfThePass)
{
    /* The first glViewport of a program can be the size it asks for before it has the window it wanted (626 x 466, then 640 x 480). */
    m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                           CONTEXT_ADDRESS);
    m_builder.viewport    (0,
                           0,
                           626,
                           466);
    m_builder.viewport    (0,
                           0,
                           640,
                           480);
    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    EXPECT_EQ(model.resource_vec.at(0).description.width,
              640u);
    EXPECT_EQ(model.pass_structure_interner.get_structure(0).viewport.height,
              480.0f);
}

TEST_F(GlFrontendPasses, AClearWithNoMaskOfDepthDoesNotClearDepth)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT | STENCIL_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& structure = m_harness.get_model().pass_structure_interner.get_structure(0);

    EXPECT_TRUE (structure.clears_color);
    EXPECT_FALSE(structure.clears_depth);
    EXPECT_TRUE (structure.clears_stencil);
}

TEST_F(GlFrontendPasses, ACallOfAThreadWithNoContextIsNotFollowed)
{
    m_builder.viewport(0,
                       0,
                       640,
                       480);
    m_builder.clear   (COLOR_BUFFER_BIT);

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              2u);
    EXPECT_TRUE(m_harness.get_model().resource_vec.empty() );
}

TEST_F(GlFrontendPasses, AMakeCurrentWhichFailedChangesNothing)
{
    /* The first unbind of a program, with no context current, fails and does not matter; a bind which fails does not give a context either. */
    m_builder.make_current(0,
                           0,
                           false); /* in_succeeds */
    m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                           CONTEXT_ADDRESS,
                           false); /* in_succeeds */
    m_builder.clear       (COLOR_BUFFER_BIT);

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST_F(GlFrontendPasses, AnUnbindLeavesTheThreadWithNoContext)
{
    start(640,
          480);

    m_builder.make_current(0,
                           0);
    m_builder.clear       (COLOR_BUFFER_BIT);

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
    EXPECT_TRUE(m_harness.get_model().resource_vec.empty() );
}

TEST_F(GlFrontendPasses, EachContextHasItsOwnViewport)
{
    start(640,
          480);

    m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                           OTHER_CONTEXT_ADDRESS);
    m_builder.viewport    (0,
                           0,
                           100,
                           50);
    m_builder.clear       (COLOR_BUFFER_BIT);

    feed();

    EXPECT_EQ(m_harness.get_model().resource_vec.at(0).description.width,
              100u);

    /* The first context has the viewport it set. */
    m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                           CONTEXT_ADDRESS);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              0u);
}

TEST_F(GlFrontendPasses, TwoThreadsEachHaveTheirOwnCurrentContext)
{
    start(640,
          480);

    m_builder.set_thread_id(2);
    m_builder.clear        (COLOR_BUFFER_BIT); /* the second thread has no context current */

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
    EXPECT_TRUE(m_harness.get_model().resource_vec.empty() );
}

TEST_F(GlFrontendPasses, AClearWithTheScissorTestOnReachesPartOfTheWindow)
{
    Tests::RecordingAccessSink sink;

    m_harness.get_builder().set_access_sink(&sink);

    start(640,
          480);

    m_builder.scissor(0,
                      50,
                      640,
                      380);
    m_builder.clear  (COLOR_BUFFER_BIT); /* the test is off: all of it */
    m_builder.enable (SCISSOR_TEST);
    m_builder.clear  (COLOR_BUFFER_BIT);
    m_builder.scissor(0,
                      0,
                      640,
                      480);
    m_builder.clear  (COLOR_BUFFER_BIT); /* a box which is the window: all of it */
    m_builder.disable(SCISSOR_TEST);

    feed();

    ASSERT_EQ(sink.event_vec.size(),
              3u);

    EXPECT_EQ(sink.event_vec.at(0).coverage,
              Analysis::APIDumpAnalysis::AccessCoverage::FULL);
    EXPECT_EQ(sink.event_vec.at(1).coverage,
              Analysis::APIDumpAnalysis::AccessCoverage::PARTIAL);
    EXPECT_EQ(sink.event_vec.at(2).coverage,
              Analysis::APIDumpAnalysis::AccessCoverage::FULL);
}

TEST_F(GlFrontendPasses, DrawsFromArraysAreNotFollowedYetAndAreCounted)
{
    start(640,
          480);

    m_builder.clear   (COLOR_BUFFER_BIT);
    m_builder.add_call(APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS,
                       {APIInterceptor::APIFunctionArgument::create_u32_glenum(QUADS),
                        APIInterceptor::APIFunctionArgument::create_i32(0),
                        APIInterceptor::APIFunctionArgument::create_i32(4)});

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unsupported_calls(),
              1u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              0u);
}

TEST_F(GlFrontendPasses, TheSessionGivesTheCallsOfOpenGLAndTheSwapToTheFrontEnd)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    bool                                                   did_swap_end_frame = false;

    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    for (const auto& call : *m_builder.get_workload().dumped_api_call_vec_ptr)
    {
        did_swap_end_frame = session.process_call(call,
                                                  m_builder.get_workload() );
    }

    /* The last call is the swap: it ended the frame, and no call was left for lack of a front end. */
    EXPECT_TRUE(did_swap_end_frame);
    EXPECT_EQ  (session.get_model().frame_record_vec.size(),
                1u);
    EXPECT_EQ  (session.get_summary().n_calls_ignored,
                0u);
}
