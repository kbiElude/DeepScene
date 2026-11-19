/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end following the matrices: the model-view and the projection are kept apart, each with its own stack, and
 * the others (texture and color) do not touch them.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_matrix_math.h"
#include "gl_frontend_fixture.h"
#include <cstring>

namespace
{
    class GLFrontendMatrices : public Tests::GLFrontendFixture
    {
    };
}

/* The product keeps the order of its operations, so the keys that depend on it do not change. */
TEST(GLMatrixMathProduct, MatchesTheTermByTermProductBitForBit)
{
    using Analysis::APIDumpAnalysis::Frontends::GLMatrix;
    using Analysis::APIDumpAnalysis::Frontends::GLMatrixMath;

    uint32_t seed = 12345;

    const auto next_value_func = [&seed]()
    {
        seed = seed * 1664525u + 1013904223u;

        return static_cast<float>(static_cast<int32_t>(seed >> 8) % 20001 - 10000) / 137.0f;
    };

    for (uint32_t n_pair = 0;
                  n_pair < 500;
                ++n_pair)
    {
        GLMatrix left;
        GLMatrix right;
        GLMatrix expected = {};

        for (size_t n_element = 0;
                    n_element < left.size();
                  ++n_element)
        {
            left.at (n_element) = next_value_func();
            right.at(n_element) = next_value_func();
        }

        for (size_t n_column = 0;
                    n_column < 4;
                  ++n_column)
        {
            for (size_t n_row = 0;
                        n_row < 4;
                      ++n_row)
            {
                float sum = 0.0f;

                for (size_t n_term = 0;
                            n_term < 4;
                          ++n_term)
                {
                    sum += left.at(n_term * 4 + n_row) * right.at(n_column * 4 + n_term);
                }

                expected.at(n_column * 4 + n_row) = sum;
            }
        }

        const GLMatrix actual = GLMatrixMath::multiply(left,
                                                       right);

        EXPECT_EQ(::memcmp(actual.data(),
                           expected.data(),
                           sizeof(GLMatrix) ),
                  0);
    }
}

TEST_F(GLFrontendMatrices, TheMatrixModeIsTheOneTheApplicationSelected)
{
    start(640,
          480);

    m_builder.matrix_mode(PROJECTION);

    feed         ();
    read_matrices();

    EXPECT_EQ(m_matrix_mode,
              static_cast<uint32_t>(PROJECTION) );
}

TEST_F(GLFrontendMatrices, TheModelViewAndTheProjectionAreKeptApart)
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

TEST_F(GLFrontendMatrices, AnOrthographicProjectionHasNoPerspective)
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

TEST_F(GLFrontendMatrices, APopPutsBackTheMatrixOfThePush)
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

TEST_F(GLFrontendMatrices, EachKindOfMatrixHasItsOwnStack)
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

TEST_F(GLFrontendMatrices, APopWithNothingPushedIsNotFollowed)
{
    start(640,
          480);

    m_builder.pop_matrix();

    feed();

    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST_F(GLFrontendMatrices, ACallMultipliesTheCurrentMatrixOnTheRight)
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

TEST_F(GLFrontendMatrices, ARotationIsAboutTheAxisByTheAngleInDegrees)
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

TEST_F(GLFrontendMatrices, AMatrixLoadedFromDataReplacesTheCurrentOne)
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

TEST_F(GLFrontendMatrices, AMatrixMultipliedFromDataIsMultipliedOnTheRight)
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

TEST_F(GLFrontendMatrices, ATextureMatrixIsNotTheModelView)
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

TEST_F(GLFrontendMatrices, EachContextHasItsOwnMatrices)
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

TEST_F(GLFrontendMatrices, AMatrixCallWithNoDataIsNotFollowed)
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

TEST_F(GLFrontendMatrices, TheProjectionIsToldToTheModelAsAConstantTheApiDefines)
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
