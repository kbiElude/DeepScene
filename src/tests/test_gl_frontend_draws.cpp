/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end following the draws of immediate mode (glBegin, glVertex and glEnd): one draw for each block, told apart by
 * its mode and by the state of the pipeline, and not made while a display list is made.
 */
#include <gtest/gtest.h>
#include "gl_frontend_fixture.h"
#include <set>

namespace
{
    class GLFrontendDraws : public Tests::GLFrontendFixture
    {
    protected:
        /* The values of OpenGL that the draws use. */
        enum : uint32_t
        {
            BLEND         = 0x00000BE2,
            COMPILE       = 0x00001300,
            DEPTH_TEST    = 0x00000B71,
            GL_LEQUAL     = 0x00000203,
            GL_LESS       = 0x00000201,
            GL_ONE        = 0x00000001,
            GL_SRC_ALPHA  = 0x00000302,
            LINES         = 0x00000001,
            TRIANGLES     = 0x00000004,
            TRIANGLE_FAN  = 0x00000006
        };

        /* A triangle between three vertices, made as the program says. */
        void add_triangle(const uint32_t& in_mode)
        {
            m_builder.begin   (in_mode);
            m_builder.vertex3f(0.0f,
                               0.0f,
                               -1.0f);
            m_builder.vertex3f(1.0f,
                               0.0f,
                               -1.0f);
            m_builder.vertex3f(0.0f,
                               1.0f,
                               -1.0f);
            m_builder.end     ();
        }
    };
}

TEST_F(GLFrontendDraws, ABlockIsOneDraw)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    add_triangle          (TRIANGLES);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    ASSERT_EQ(model.frame_record_vec.size(),
              1u);

    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              1u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unsupported_calls(),
              0u);
}

TEST_F(GLFrontendDraws, ABlockWithNoVerticesIsNoDraw)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.begin       (TRIANGLES);
    m_builder.end         ();
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().frame_record_vec.at(0).n_draws,
              0u);
}

TEST_F(GLFrontendDraws, EveryKindOfVertexCallCountsAsAVertex)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.begin       (LINES);
    m_builder.vertex2f    (0.0f,
                           0.0f);
    m_builder.vertex3fv   ({1.0f, 1.0f, -2.0f});
    m_builder.end         ();
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              1u);
    ASSERT_EQ(model.draw_structure_map.size(),
              1u);
    EXPECT_EQ(model.draw_structure_map.begin()->second.primitive_topology,
              Analysis::APIDumpAnalysis::PrimitiveTopology::LINE_LIST);
}

TEST_F(GLFrontendDraws, QuadsAreTriangles)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    add_triangle          (QUADS);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    ASSERT_EQ(model.draw_structure_map.size(),
              1u);
    EXPECT_EQ(model.draw_structure_map.begin()->second.primitive_topology,
              Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST);
}

TEST_F(GLFrontendDraws, DrawsOfOneKindAreOneStructure)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    add_triangle          (TRIANGLES);
    add_triangle          (TRIANGLES);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              2u);
    EXPECT_EQ(model.draw_structure_map.size(),
              1u);
}

TEST_F(GLFrontendDraws, TheModeOfABlockTellsDrawsApart)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    add_triangle          (TRIANGLES);
    add_triangle          (TRIANGLE_FAN);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().draw_structure_map.size(),
              2u);
}

TEST_F(GLFrontendDraws, TheDepthFunctionTellsDrawsApart)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.enable      (DEPTH_TEST);
    m_builder.depth_func  (GL_LESS);
    add_triangle          (TRIANGLES);
    m_builder.depth_func  (GL_LEQUAL);
    add_triangle          (TRIANGLES);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().draw_structure_map.size(),
              2u);
}

TEST_F(GLFrontendDraws, TheBlendFunctionTellsDrawsApart)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.enable      (BLEND);
    m_builder.blend_func  (GL_SRC_ALPHA,
                           GL_ONE);
    add_triangle          (TRIANGLES);
    m_builder.blend_func  (GL_ONE,
                           GL_ONE);
    add_triangle          (TRIANGLES);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().draw_structure_map.size(),
              2u);
}

TEST_F(GLFrontendDraws, APopPutsBackThePipelineState)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    add_triangle          (TRIANGLES);
    m_builder.push_attrib (ALL_ATTRIB_BITS);
    m_builder.enable      (DEPTH_TEST);
    m_builder.depth_func  (GL_LEQUAL);
    m_builder.pop_attrib  ();
    add_triangle          (TRIANGLES); /* the same as the first one */
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(m_harness.get_model().draw_structure_map.size(),
              1u);
}

TEST_F(GLFrontendDraws, ADrawIsNotMadeWhileAListIsMade)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.new_list    (1,
                           COMPILE);
    add_triangle          (TRIANGLES);
    m_builder.enable      (DEPTH_TEST); /* recorded in the list too: it does not change the state until the list is called */
    m_builder.end_list    ();
    add_triangle          (TRIANGLES);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              1u);

    /* The draw after the list is of the state the program had before it. */
    ASSERT_EQ(model.draw_structure_map.size(),
              1u);
}

TEST_F(GLFrontendDraws, TheCallsOfAListAreNotMadeByTheMatrixStack)
{
    start(640,
          480);

    m_builder.new_list (1,
                        COMPILE);
    m_builder.translate(5.0,
                        0.0,
                        0.0);
    m_builder.end_list ();

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    0.0f);
}

TEST_F(GLFrontendDraws, TheCallsOfABlockBeginWithItsGLBegin)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    add_triangle          (TRIANGLES); /* the calls 3 (glBegin), 4 to 6 (the vertices) and 7 (glEnd) */
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& draw_vec = m_harness.get_builder().get_completed_frame().draw_instance_vec;

    ASSERT_EQ(draw_vec.size(),
              1u);

    EXPECT_EQ(draw_vec.at(0).call_index,
              7u);
    EXPECT_EQ(draw_vec.at(0).first_call_index,
              3u);
}

TEST_F(GLFrontendDraws, ADrawOfArraysBeginsWithItsOwnCall)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.draw_arrays (TRIANGLES,
                           0,
                           3,
                           {Tests::GLCallBuilder::make_vertex_array({{0.0f, 0.0f, -1.0f},
                                                                     {1.0f, 0.0f, -1.0f},
                                                                     {0.0f, 1.0f, -1.0f} })});
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& draw_vec = m_harness.get_builder().get_completed_frame().draw_instance_vec;

    ASSERT_EQ(draw_vec.size(),
              1u);

    EXPECT_EQ(draw_vec.at(0).first_call_index,
              draw_vec.at(0).call_index);
}

TEST_F(GLFrontendDraws, TheCameraOfADrawBeginsAtTheLastLoadOfTheModelViewMatrix)
{
    start(640,
          480);

    m_builder.load_identity(); /* the call 2 */
    m_builder.translate    (1.0,
                            2.0,
                            3.0);
    add_triangle           (TRIANGLES);
    m_builder.swap_buffers (DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& draw_vec = m_harness.get_builder().get_completed_frame().draw_instance_vec;

    ASSERT_EQ(draw_vec.size(),
              1u);

    /* What the program does to the matrix after it is on top of that: the camera still begins there. */
    EXPECT_EQ(draw_vec.at(0).view_anchor_call_index,
              2u);
}

TEST_F(GLFrontendDraws, ALoadInsideAPushBeginsItsOwnCameraAndAPopGivesBackTheOneOfTheMatrixBelow)
{
    start(640,
          480);

    m_builder.load_identity(); /* the call 2 */
    m_builder.push_matrix  (); /* the call 3 */
    m_builder.load_identity(); /* the call 4: the camera of what is drawn in the push */
    add_triangle           (TRIANGLES);
    m_builder.pop_matrix   ();
    add_triangle           (TRIANGLES);
    m_builder.swap_buffers (DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& draw_vec = m_harness.get_builder().get_completed_frame().draw_instance_vec;

    ASSERT_EQ(draw_vec.size(),
              2u);

    EXPECT_EQ(draw_vec.at(0).view_anchor_call_index,
              4u);
    EXPECT_EQ(draw_vec.at(1).view_anchor_call_index,
              2u);
}

TEST_F(GLFrontendDraws, AMatrixThatIsLoadedBeginsTheCameraToo)
{
    start(640,
          480);

    m_builder.load_identity();
    m_builder.load_matrix   ({1.0f, 0.0f, 0.0f, 0.0f,
                              0.0f, 1.0f, 0.0f, 0.0f,
                              0.0f, 0.0f, 1.0f, 0.0f,
                              5.0f, 0.0f, 0.0f, 1.0f});
    add_triangle            (TRIANGLES);
    m_builder.swap_buffers  (DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& draw_vec = m_harness.get_builder().get_completed_frame().draw_instance_vec;

    ASSERT_EQ(draw_vec.size(),
              1u);

    EXPECT_EQ(draw_vec.at(0).view_anchor_call_index,
              3u);
}

TEST_F(GLFrontendDraws, TheProjectionHasNoPartInTheCamera)
{
    start(640,
          480);

    m_builder.load_identity();            /* the call 2: of the model-view matrix */
    m_builder.matrix_mode  (PROJECTION);
    m_builder.load_identity();
    m_builder.matrix_mode  (MODELVIEW);
    add_triangle           (TRIANGLES);
    m_builder.swap_buffers (DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& draw_vec = m_harness.get_builder().get_completed_frame().draw_instance_vec;

    ASSERT_EQ(draw_vec.size(),
              1u);

    EXPECT_EQ(draw_vec.at(0).view_anchor_call_index,
              2u);
}
