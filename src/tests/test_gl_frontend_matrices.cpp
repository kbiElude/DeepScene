/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end following the matrices: the model-view and the projection are kept apart, each with its own stack, and
 * the others (texture and color) do not touch them.
 */
#include <gtest/gtest.h>
#include "gl_frontend_fixture.h"

namespace
{
    class GlFrontendMatrices : public Tests::GlFrontendFixture
    {
    };
}

TEST_F(GlFrontendMatrices, TheModelViewAndTheProjectionAreKeptApart)
{
    start(640,
          480);

    m_builder.matrix_mode(PROJECTION);
    m_builder.load_identity();
    m_builder.frustum    (-1.0,
                          1.0,
                          -0.75,
                          0.75,
                          1.0,
                          100.0);
    m_builder.matrix_mode(MODELVIEW);
    m_builder.translate  (1.0,
                          2.0,
                          3.0);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(0),
                    1.0f);
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    1.0f);
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(13),
                    2.0f);
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(14),
                    3.0f);

    /* The matrix of glFrustum: 2n / (r - l), 2n / (t - b), the terms of the depth, and the -1 which makes w the distance. */
    EXPECT_NEAR(m_projection_f32mat.at(0),
                1.0f,
                1e-5f);
    EXPECT_NEAR(m_projection_f32mat.at(5),
                1.0f / 0.75f,
                1e-5f);
    EXPECT_NEAR(m_projection_f32mat.at(10),
                -101.0f / 99.0f,
                1e-5f);
    EXPECT_NEAR(m_projection_f32mat.at(14),
                -200.0f / 99.0f,
                1e-5f);
    EXPECT_FLOAT_EQ(m_projection_f32mat.at(11),
                    -1.0f);
    EXPECT_FLOAT_EQ(m_projection_f32mat.at(12),
                    0.0f);
}

TEST_F(GlFrontendMatrices, AnOrthographicProjectionHasNoPerspective)
{
    start(640,
          480);

    m_builder.matrix_mode(PROJECTION);
    m_builder.ortho      (0.0,
                          640.0,
                          0.0,
                          480.0,
                          -1.0,
                          1.0);

    feed         ();
    read_matrices();

    EXPECT_NEAR    (m_projection_f32mat.at(0),
                    2.0f / 640.0f,
                    1e-7f);
    EXPECT_NEAR    (m_projection_f32mat.at(5),
                    2.0f / 480.0f,
                    1e-7f);
    EXPECT_FLOAT_EQ(m_projection_f32mat.at(12),
                    -1.0f);
    EXPECT_FLOAT_EQ(m_projection_f32mat.at(13),
                    -1.0f);
    EXPECT_FLOAT_EQ(m_projection_f32mat.at(11),
                    0.0f);
    EXPECT_FLOAT_EQ(m_projection_f32mat.at(15),
                    1.0f);
}

TEST_F(GlFrontendMatrices, APopPutsBackTheMatrixOfThePush)
{
    start(640,
          480);

    m_builder.translate  (1.0,
                          0.0,
                          0.0);
    m_builder.push_matrix();
    m_builder.translate  (0.0,
                          5.0,
                          0.0);
    m_builder.pop_matrix ();

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    1.0f);
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(13),
                    0.0f);
}

TEST_F(GlFrontendMatrices, EachKindOfMatrixHasItsOwnStack)
{
    start(640,
          480);

    m_builder.push_matrix();                   /* of the model-view */
    m_builder.matrix_mode(PROJECTION);
    m_builder.translate  (0.0,
                          0.0,
                          1.0);
    m_builder.pop_matrix ();                   /* of the projection, which has nothing pushed */

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST_F(GlFrontendMatrices, APopWithNothingPushedIsNotFollowed)
{
    start(640,
          480);

    m_builder.pop_matrix();

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST_F(GlFrontendMatrices, ACallMultipliesTheCurrentMatrixOnTheRight)
{
    start(640,
          480);

    /* A translation then a scale is a matrix which scales and moves by the amount of the translation, as it was given. */
    m_builder.translate(10.0,
                        0.0,
                        0.0);
    m_builder.scale    (2.0f,
                        2.0f,
                        2.0f);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(0),
                    2.0f);
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    10.0f);

    /* A scale then a translation moves by the amount of the translation times the scale. */
    m_builder.load_identity();
    m_builder.scale        (2.0f,
                            2.0f,
                            2.0f);
    m_builder.translate    (10.0,
                            0.0,
                            0.0);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    20.0f);
}

TEST_F(GlFrontendMatrices, ARotationIsAboutTheAxisByTheAngleInDegrees)
{
    start(640,
          480);

    m_builder.rotate(90.0f,
                     0.0f,
                     0.0f,
                     1.0f);

    feed         ();
    read_matrices();

    /* A quarter of a turn about z takes x to y and y to minus x (the columns are the images of the axes). */
    EXPECT_NEAR(m_modelview_f32mat.at(0),
                0.0f,
                1e-6f);
    EXPECT_NEAR(m_modelview_f32mat.at(1),
                1.0f,
                1e-6f);
    EXPECT_NEAR(m_modelview_f32mat.at(4),
                -1.0f,
                1e-6f);
    EXPECT_NEAR(m_modelview_f32mat.at(5),
                0.0f,
                1e-6f);
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(10),
                    1.0f);
}

TEST_F(GlFrontendMatrices, AMatrixLoadedFromDataReplacesTheCurrentOne)
{
    start(640,
          480);

    std::array<float, 16>  matrix_f32mat  = {};
    std::array<double, 16> matrix_d64mat  = {};

    matrix_f32mat.at(0)  = 2.0f;
    matrix_f32mat.at(5)  = 2.0f;
    matrix_f32mat.at(10) = 2.0f;
    matrix_f32mat.at(12) = 7.0f;
    matrix_f32mat.at(13) = 8.0f;
    matrix_f32mat.at(14) = 9.0f;
    matrix_f32mat.at(15) = 1.0f;

    matrix_d64mat.at(0)  = 3.0;
    matrix_d64mat.at(5)  = 3.0;
    matrix_d64mat.at(10) = 3.0;
    matrix_d64mat.at(12) = 4.0;
    matrix_d64mat.at(15) = 1.0;

    m_builder.translate  (1.0,
                          1.0,
                          1.0);
    m_builder.load_matrix(matrix_f32mat);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(0),
                    2.0f);
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    7.0f);
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(14),
                    9.0f);

    /* The matrix of doubles is read as well. */
    m_builder.load_matrix_double(matrix_d64mat);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(0),
                    3.0f);
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    4.0f);
}

TEST_F(GlFrontendMatrices, AMatrixMultipliedFromDataIsMultipliedOnTheRight)
{
    start(640,
          480);

    std::array<float, 16> matrix_f32mat = {};

    matrix_f32mat.at(0)  = 3.0f;
    matrix_f32mat.at(5)  = 3.0f;
    matrix_f32mat.at(10) = 3.0f;
    matrix_f32mat.at(15) = 1.0f;

    m_builder.translate  (1.0,
                          0.0,
                          0.0);
    m_builder.mult_matrix(matrix_f32mat);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(0),
                    3.0f);
    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    1.0f);
}

TEST_F(GlFrontendMatrices, ATextureMatrixIsNotTheModelView)
{
    start(640,
          480);

    m_builder.matrix_mode(TEXTURE);
    m_builder.translate  (5.0,
                          5.0,
                          5.0);
    m_builder.matrix_mode(MODELVIEW);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    0.0f);
    EXPECT_FLOAT_EQ(m_projection_f32mat.at(12),
                    0.0f);
}

TEST_F(GlFrontendMatrices, EachContextHasItsOwnMatrices)
{
    start(640,
          480);

    m_builder.translate   (1.0,
                           2.0,
                           3.0);
    m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                           OTHER_CONTEXT_ADDRESS);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    0.0f);

    m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                           CONTEXT_ADDRESS);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    1.0f);
}

TEST_F(GlFrontendMatrices, AMatrixCallWithNoDataIsNotFollowed)
{
    start(640,
          480);

    /* The data of the matrix is a chunk; a call without one has nothing to load. */
    m_builder.add_call(APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF,
                       {});

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST_F(GlFrontendMatrices, TheProjectionIsToldToTheModelAsAConstantTheApiDefines)
{
    start(640,
          480);

    m_builder.matrix_mode (PROJECTION);
    m_builder.frustum     (-1.0,
                           1.0,
                           -0.75,
                           0.75,
                           1.0,
                           100.0);
    m_builder.matrix_mode (MODELVIEW);
    m_builder.clear       (COLOR_BUFFER_BIT | DEPTH_BUFFER_BIT);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    const auto& model = m_harness.get_model();

    ASSERT_EQ(model.pass_structure_interner.get_n_structures(),
              1u);

    const auto& signature_vec = model.pass_structure_interner.get_structure(0).constant_signature_vec;
    bool        has_projection = false;

    for (const auto& signature : signature_vec)
    {
        if (signature.semantic == Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX)
        {
            has_projection = true;

            EXPECT_EQ(signature.provenance,
                      Analysis::APIDumpAnalysis::ConstantProvenance::API_DEFINED);
            EXPECT_EQ(signature.block_kind,
                      Analysis::APIDumpAnalysis::ConstantBlockKind::FIXED_FUNCTION_STATE);
        }
    }

    EXPECT_TRUE(has_projection);
}
