/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end following glPushAttrib and glPopAttrib: what the program set after a push is put back by the pop, as
 * far as the mask of the push says (the matrices are not a part of it).
 */
#include <gtest/gtest.h>
#include "gl_frontend_fixture.h"

namespace
{
    class GlFrontendAttributes : public Tests::GlFrontendFixture
    {
    };
}

TEST_F(GlFrontendAttributes, APopPutsBackTheScissorTest)
{
    Tests::RecordingAccessSink sink;

    m_harness.get_builder().set_access_sink(&sink);

    start(640,
          480);

    m_builder.clear      (COLOR_BUFFER_BIT);
    m_builder.push_attrib(ALL_ATTRIB_BITS);
    m_builder.scissor    (0,
                          50,
                          640,
                          380);
    m_builder.enable     (SCISSOR_TEST);
    m_builder.pop_attrib ();
    m_builder.clear      (COLOR_BUFFER_BIT); /* the test is off again: all of the window */

    feed();

    ASSERT_EQ(sink.event_vec.size(),
              2u);

    EXPECT_EQ(sink.event_vec.at(1).coverage,
              Analysis::APIDumpAnalysis::AccessCoverage::FULL);
}

TEST_F(GlFrontendAttributes, WithoutAPopTheScissorTestStaysOn)
{
    Tests::RecordingAccessSink sink;

    m_harness.get_builder().set_access_sink(&sink);

    start(640,
          480);

    m_builder.clear      (COLOR_BUFFER_BIT);
    m_builder.push_attrib(ALL_ATTRIB_BITS);
    m_builder.scissor    (0,
                          50,
                          640,
                          380);
    m_builder.enable     (SCISSOR_TEST);
    m_builder.clear      (COLOR_BUFFER_BIT);

    feed();

    ASSERT_EQ(sink.event_vec.size(),
              2u);

    EXPECT_EQ(sink.event_vec.at(1).coverage,
              Analysis::APIDumpAnalysis::AccessCoverage::PARTIAL);
}

TEST_F(GlFrontendAttributes, APopPutsBackTheViewport)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.push_attrib (ALL_ATTRIB_BITS);
    m_builder.viewport    (0,
                           0,
                           320,
                           240);
    m_builder.pop_attrib  ();
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    ASSERT_EQ(model.pass_structure_interner.get_n_structures(),
              1u);

    EXPECT_EQ(model.pass_structure_interner.get_structure(0).viewport.width,
              640.0f);
    EXPECT_EQ(model.pass_structure_interner.get_structure(0).viewport.height,
              480.0f);
}

TEST_F(GlFrontendAttributes, APopPutsBackTheMatrixMode)
{
    start(640,
          480);

    m_builder.push_attrib(ALL_ATTRIB_BITS);
    m_builder.matrix_mode(PROJECTION);
    m_builder.pop_attrib ();
    m_builder.translate  (1.0,
                          0.0,
                          0.0);

    feed         ();
    read_matrices();

    /* The mode is the model-view again, so it is the model-view that moved. */
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    1.0f);
    EXPECT_FLOAT_EQ(m_projection_f32mat.at(12),
                    0.0f);
}

TEST_F(GlFrontendAttributes, AMaskOfNothingPutsBackNothing)
{
    start(640,
          480);

    m_builder.push_attrib(0);
    m_builder.matrix_mode(PROJECTION);
    m_builder.pop_attrib ();
    m_builder.translate  (1.0,
                          0.0,
                          0.0);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    0.0f);
    EXPECT_FLOAT_EQ(m_projection_f32mat.at(12),
                    1.0f);
}

TEST_F(GlFrontendAttributes, TheMatricesAreNotAPartOfTheAttributes)
{
    start(640,
          480);

    m_builder.translate  (1.0,
                          0.0,
                          0.0);
    m_builder.push_attrib(ALL_ATTRIB_BITS);
    m_builder.translate  (2.0,
                          0.0,
                          0.0);
    m_builder.pop_attrib ();

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    3.0f);
}

TEST_F(GlFrontendAttributes, APopWithNothingPushedIsNotFollowed)
{
    start(640,
          480);

    m_builder.pop_attrib();

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST_F(GlFrontendAttributes, EachContextHasItsOwnStackOfAttributes)
{
    start(640,
          480);

    m_builder.push_attrib (ALL_ATTRIB_BITS);
    m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                           OTHER_CONTEXT_ADDRESS);
    m_builder.pop_attrib  ();                          /* the other context has pushed nothing */

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}
