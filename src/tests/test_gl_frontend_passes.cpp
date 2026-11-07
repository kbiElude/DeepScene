/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end following the contexts, the window, the clears and the frames.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/opengl_frontend.h"
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "frontend_harness.h"
#include "gl_call_builder.h"
#include <vector>

namespace
{
    constexpr uint64_t CONTEXT_ADDRESS        = 0x10000;
    constexpr uint64_t DEVICE_CONTEXT_ADDRESS = 0x63012365;
    constexpr uint64_t OTHER_CONTEXT_ADDRESS  = 0x10001;

    constexpr uint32_t COLOR_BUFFER_BIT   = 0x4000;
    constexpr uint32_t DEPTH_BUFFER_BIT   = 0x0100;
    constexpr uint32_t QUADS              = 0x0007;
    constexpr uint32_t SCISSOR_TEST       = 0x0C11;
    constexpr uint32_t STENCIL_BUFFER_BIT = 0x0400;

    /* Keeps the accesses that the model builder tells of. */
    class RecordingSink : public Analysis::APIDumpAnalysis::IAccessSink
    {
    public:
        void on_access(const Analysis::APIDumpAnalysis::AccessEvent& in_event) override
        {
            event_vec.push_back(in_event);
        }

        void on_frame_ended(const uint64_t& in_frame_index,
                            const uint64_t& in_first_call_index,
                            const uint64_t& in_last_call_index) override
        {
            (void) in_frame_index;
            (void) in_first_call_index;
            (void) in_last_call_index;
        }

        std::vector<Analysis::APIDumpAnalysis::AccessEvent> event_vec;
    };

    class GlFrontendPasses : public ::testing::Test
    {
    protected:
        typedef Tests::FrontendHarness<Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend,
                                       Tests::GlCallBuilder>                                 Harness;

        void feed()
        {
            m_harness.feed(m_builder);
        }

        /* A context which is current, and the viewport that an application sets first. */
        void start(const int32_t& in_width,
                   const int32_t& in_height)
        {
            m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                                   CONTEXT_ADDRESS);
            m_builder.viewport    (0,
                                   0,
                                   in_width,
                                   in_height);
        }

        Tests::GlCallBuilder m_builder;
        Harness              m_harness;
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
    RecordingSink sink;

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

TEST_F(GlFrontendPasses, DrawsAreNotFollowedYetAndAreCounted)
{
    start(640,
          480);

    m_builder.clear(COLOR_BUFFER_BIT);
    m_builder.begin(QUADS);

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unsupported_calls(),
              1u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              0u);
}

TEST(GlFrontendSession, TheSessionGivesTheCallsOfOpenGLAndTheSwapToTheFrontEnd)
{
    Analysis::APIDumpAnalysis::Frontends::AnalysisSession session;
    Tests::GlCallBuilder                                   builder;
    bool                                                   did_swap_end_frame = false;

    builder.make_current  (DEVICE_CONTEXT_ADDRESS,
                           CONTEXT_ADDRESS);
    builder.viewport      (0,
                           0,
                           640,
                           480);
    builder.clear         (COLOR_BUFFER_BIT);
    builder.swap_buffers  (DEVICE_CONTEXT_ADDRESS);

    for (const auto& call : *builder.get_workload().dumped_api_call_vec_ptr)
    {
        did_swap_end_frame = session.process_call(call,
                                                  builder.get_workload() );
    }

    /* The last call is the swap: it ended the frame, and no call was left for lack of a front end. */
    EXPECT_TRUE(did_swap_end_frame);
    EXPECT_EQ  (session.get_model().frame_record_vec.size(),
                1u);
    EXPECT_EQ  (session.get_summary().n_calls_ignored,
                0u);
}
