/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL front end following display lists: the calls made between glNewList and glEndList are kept in the list, with the data
 * that they refer to, and are made each time the list is called.
 */
#include <gtest/gtest.h>
#include "gl_frontend_fixture.h"

namespace
{
    class GLFrontendLists : public Tests::GLFrontendFixture
    {
    protected:
        enum : uint32_t
        {
            COMPILE             = 0x00001300,
            COMPILE_AND_EXECUTE = 0x00001301,
            GL_TRIANGLES        = 0x00000004
        };

        /* A triangle between three vertices, which are given as the data of a pointer (the calls of a list keep it). */
        void add_triangle()
        {
            m_builder.begin    (GL_TRIANGLES);
            m_builder.vertex3fv({0.0f, 0.0f, -1.0f});
            m_builder.vertex3fv({1.0f, 0.0f, -1.0f});
            m_builder.vertex3fv({0.0f, 1.0f, -1.0f});
            m_builder.end      ();
        }

        /* A list of one triangle. */
        void make_triangle_list(const uint32_t& in_list)
        {
            m_builder.new_list(in_list,
                               COMPILE);
            add_triangle      ();
            m_builder.end_list();
        }

        /* What the list does to what an eye has of its own. */
        Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence get_eye_dependence(const uint32_t& in_list)
        {
            Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence result = Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::INDEPENDENT;

            EXPECT_TRUE(m_harness.get_frontend().find_list_eye_dependence(in_list,
                                                                          &result) ) << "list " << in_list;

            return result;
        }

        uint64_t get_n_draws() const
        {
            return m_harness.get_model().frame_record_vec.at(0).n_draws;
        }
    };
}

TEST_F(GLFrontendLists, ACallOfAListMakesItsCalls)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    make_triangle_list    (1);
    m_builder.call_list   (1);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_n_draws(),
              1u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              0u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unsupported_calls(),
              0u);
}

TEST_F(GLFrontendLists, AListIsMadeEachTimeItIsCalled)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    make_triangle_list    (1);
    m_builder.call_list   (1);
    m_builder.call_list   (1);
    m_builder.call_list   (1);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_n_draws(),
              3u);
    EXPECT_EQ(m_harness.get_model().draw_structure_map.size(),
              1u);
}

TEST_F(GLFrontendLists, AListWhichIsNotCalledMakesNothing)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    make_triangle_list    (1);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_n_draws(),
              0u);
}

TEST_F(GLFrontendLists, ACallMadeWhileAListIsMadeIsNotMadeThen)
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

TEST_F(GLFrontendLists, ACallOfAListMakesItsStateChangesEachTime)
{
    start(640,
          480);

    m_builder.new_list (1,
                        COMPILE);
    m_builder.translate(5.0,
                        0.0,
                        0.0);
    m_builder.end_list ();
    m_builder.call_list(1);
    m_builder.call_list(1);

    feed         ();
    read_matrices();

    EXPECT_FLOAT_EQ(m_modelview_f32mat.at(12),
                    10.0f);
}

TEST_F(GLFrontendLists, AListWithAnotherPipelineStateMakesAnotherDraw)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.new_list    (1,
                           COMPILE);
    m_builder.enable      (0x0B71); /* GL_DEPTH_TEST */
    add_triangle          ();
    m_builder.end_list    ();
    add_triangle          ();
    m_builder.call_list   (1);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_n_draws(),
              2u);
    EXPECT_EQ(m_harness.get_model().draw_structure_map.size(),
              2u);
}

TEST_F(GLFrontendLists, AListCanCallAnotherList)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    make_triangle_list    (1);
    m_builder.new_list    (2,
                           COMPILE);
    m_builder.call_list   (1);
    m_builder.call_list   (1);
    m_builder.end_list    ();
    m_builder.call_list   (2);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_n_draws(),
              2u);
}

TEST_F(GLFrontendLists, AListWhichCallsItselfIsNotMadeForEver)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.new_list    (1,
                           COMPILE);
    m_builder.call_list   (1);
    m_builder.end_list    ();
    m_builder.call_list   (1);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_n_draws(),
              0u);
    EXPECT_GE(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST_F(GLFrontendLists, ADeletedListIsNotCalled)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    make_triangle_list    (1);
    make_triangle_list    (2);
    make_triangle_list    (3);
    m_builder.delete_lists(1,
                           2);
    m_builder.call_list   (1);
    m_builder.call_list   (2);
    m_builder.call_list   (3);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_n_draws(),
              1u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              2u);
}

TEST_F(GLFrontendLists, AListWhichWasNeverMadeIsNotFollowed)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.call_list   (7);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_n_draws(),
              0u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST_F(GLFrontendLists, ACompileAndExecuteListIsMadeAtOnceAndWhenCalled)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.new_list    (1,
                           COMPILE_AND_EXECUTE);
    add_triangle          ();
    m_builder.end_list    ();
    m_builder.call_list   (1);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_n_draws(),
              2u);
}

TEST_F(GLFrontendLists, ADrawOfArraysInAListKeepsItsData)
{
    typedef Tests::GLCallBuilder::ClientArray ClientArray;

    const ClientArray vertices = Tests::GLCallBuilder::make_vertex_array({{0.0f, 0.0f, -1.0f},
                                                                          {1.0f, 0.0f, -1.0f},
                                                                          {0.0f, 1.0f, -1.0f} });

    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    m_builder.new_list    (1,
                           COMPILE);
    m_builder.draw_arrays (GL_TRIANGLES,
                           0,
                           3,
                           {vertices});
    m_builder.end_list    ();
    m_builder.call_list   (1);
    m_builder.call_list   (1);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    size_t n_buffers = 0;

    for (const auto& resource : m_harness.get_model().resource_vec)
    {
        if (resource.description.kind == Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX)
        {
            ++n_buffers;
        }
    }

    EXPECT_EQ(get_n_draws(),
              2u);
    EXPECT_EQ(n_buffers,
              1u);
    EXPECT_EQ(m_harness.get_frontend().get_n_unfollowed_calls(),
              0u);
}

TEST_F(GLFrontendLists, AListMadeInOneContextIsCalledInAnother)
{
    start(640,
          480);

    m_builder.clear       (COLOR_BUFFER_BIT);
    make_triangle_list    (1);
    m_builder.make_current(DEVICE_CONTEXT_ADDRESS,
                           OTHER_CONTEXT_ADDRESS);
    m_builder.viewport    (0,
                           0,
                           640,
                           480);
    m_builder.call_list   (1);
    m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);

    feed();

    EXPECT_EQ(get_n_draws(),
              1u);
}

TEST_F(GLFrontendLists, AListWhichOnlyDrawsAndSetsStateIsEyeIndependent)
{
    start(640,
          480);

    m_builder.new_list (1,
                        COMPILE);
    m_builder.enable   (0x0B71); /* GL_DEPTH_TEST */
    add_triangle       ();
    m_builder.end_list ();

    feed();

    EXPECT_EQ(get_eye_dependence(1),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::INDEPENDENT);
}

TEST_F(GLFrontendLists, AListWhichMovesTheMatricesRelativeToTheCurrentOnesIsToldSo)
{
    start(640,
          480);

    m_builder.new_list (1,
                        COMPILE);
    m_builder.translate(1.0,
                        0.0,
                        0.0);
    m_builder.rotate   (90.0f,
                        0.0f,
                        1.0f,
                        0.0f);
    m_builder.push_matrix();
    m_builder.pop_matrix ();
    add_triangle       ();
    m_builder.end_list ();

    feed();

    EXPECT_EQ(get_eye_dependence(1),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::RELATIVE_MATRIX_ONLY);
}

TEST_F(GLFrontendLists, AListThatMakesSomethingRelativeToTheMatrixOfTheMomentIsRelativeToTheMatrix)
{
    start(640,
          480);

    /* The position of a light, and a clip plane: each is turned into the coordinates of the eye by the matrix that is there when the list is called. */
    m_builder.new_list(1,
                       COMPILE);
    m_builder.add_call(APIInterceptor::APIFUNCTION_GL_GLLIGHTFV,
                       {APIInterceptor::APIFunctionArgument::create_u32_glenum(0x4000),
                        APIInterceptor::APIFunctionArgument::create_u32_glenum(0x1203),
                        APIInterceptor::APIFunctionArgument::create_i32       (0)});
    m_builder.end_list();
    m_builder.new_list(2,
                       COMPILE);
    m_builder.add_call(APIInterceptor::APIFUNCTION_GL_GLCLIPPLANE,
                       {APIInterceptor::APIFunctionArgument::create_u32_glenum(0x3000),
                        APIInterceptor::APIFunctionArgument::create_i32       (0)});
    m_builder.end_list();

    /* A list that calls such a list is too. */
    m_builder.new_list (3,
                        COMPILE);
    m_builder.call_list(1);
    m_builder.end_list ();

    /* A material is not. */
    m_builder.new_list(4,
                       COMPILE);
    m_builder.add_call(APIInterceptor::APIFUNCTION_GL_GLMATERIALFV,
                       {APIInterceptor::APIFunctionArgument::create_u32_glenum(0x0408),
                        APIInterceptor::APIFunctionArgument::create_u32_glenum(0x1200),
                        APIInterceptor::APIFunctionArgument::create_i32       (0)});
    m_builder.end_list();

    feed();

    EXPECT_EQ(get_eye_dependence(1),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::RELATIVE_MATRIX_ONLY);
    EXPECT_EQ(get_eye_dependence(2),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::RELATIVE_MATRIX_ONLY);
    EXPECT_EQ(get_eye_dependence(3),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::RELATIVE_MATRIX_ONLY);
    EXPECT_EQ(get_eye_dependence(4),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::INDEPENDENT);
}

TEST_F(GLFrontendLists, AListWhichSetsAValueOfItsOwnIsEyeDependent)
{
    start(640,
          480);

    m_builder.new_list     (1,
                            COMPILE);
    m_builder.load_identity();
    m_builder.end_list     ();
    m_builder.new_list     (2,
                            COMPILE);
    m_builder.frustum      (-1.0, 1.0, -1.0, 1.0, 1.0, 100.0);
    m_builder.end_list     ();
    m_builder.new_list     (3,
                            COMPILE);
    m_builder.ortho        (0.0, 640.0, 0.0, 480.0, -1.0, 1.0);
    m_builder.end_list     ();
    m_builder.new_list     (4,
                            COMPILE);
    m_builder.viewport     (0,
                            0,
                            320,
                            480);
    m_builder.end_list     ();
    m_builder.new_list     (5,
                            COMPILE);
    m_builder.scissor      (0,
                            0,
                            320,
                            480);
    m_builder.end_list     ();
    m_builder.new_list     (6,
                            COMPILE);
    m_builder.load_matrix  ({1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1});
    m_builder.end_list     ();

    feed();

    for (uint32_t list = 1;
                  list <= 6;
                ++list)
    {
        EXPECT_EQ(get_eye_dependence(list),
                  Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::EYE_DEPENDENT) << "list " << list;
    }
}

TEST_F(GLFrontendLists, TheStrongestCallOfAListTellsWhatItIs)
{
    start(640,
          480);

    m_builder.new_list     (1,
                            COMPILE);
    m_builder.translate    (1.0,
                            0.0,
                            0.0);
    m_builder.load_identity();
    m_builder.translate    (1.0,
                            0.0,
                            0.0);
    m_builder.end_list     ();

    feed();

    EXPECT_EQ(get_eye_dependence(1),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::EYE_DEPENDENT);
}

TEST_F(GLFrontendLists, AListWhichCallsAnotherIsWhatTheOtherIs)
{
    start(640,
          480);

    make_triangle_list (1);
    m_builder.new_list (2,
                        COMPILE);
    m_builder.translate(1.0,
                        0.0,
                        0.0);
    m_builder.end_list ();
    m_builder.new_list     (3,
                            COMPILE);
    m_builder.load_identity();
    m_builder.end_list     ();
    m_builder.new_list (4,
                        COMPILE);
    m_builder.call_list(1);
    m_builder.end_list ();
    m_builder.new_list (5,
                        COMPILE);
    m_builder.call_list(2);
    m_builder.end_list ();
    m_builder.new_list (6,
                        COMPILE);
    m_builder.call_list(3);
    m_builder.end_list ();
    m_builder.new_list (7,
                        COMPILE);
    m_builder.call_list(99); /* not made (yet): it is taken for the worst */
    m_builder.end_list ();

    feed();

    EXPECT_EQ(get_eye_dependence(4),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::INDEPENDENT);
    EXPECT_EQ(get_eye_dependence(5),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::RELATIVE_MATRIX_ONLY);
    EXPECT_EQ(get_eye_dependence(6),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::EYE_DEPENDENT);
    EXPECT_EQ(get_eye_dependence(7),
              Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::EYE_DEPENDENT);
}

TEST_F(GLFrontendLists, AListWhichWasNeverMadeHasNoDependence)
{
    start(640,
          480);

    feed();

    Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence dependence = Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence::INDEPENDENT;

    EXPECT_FALSE(m_harness.get_frontend().find_list_eye_dependence(42,
                                                                   &dependence) );
}

TEST_F(GLFrontendLists, ACallOfAListGivesTheKeysOfEachDrawItMakes)
{
    start(640,
          480);

    make_triangle_list(1);
    m_builder.call_list(1);
    feed();

    EXPECT_EQ(m_harness.get_builder().get_call_draw_key_vec().size(),
              1u);

    m_builder.call_list(1);
    m_builder.call_list(1);
    feed();

    EXPECT_EQ(m_harness.get_builder().get_call_draw_key_vec().size(),
              1u);
}

TEST_F(GLFrontendLists, ACallThatDrawsNothingGivesNoKeys)
{
    start(640,
          480);

    make_triangle_list(1);
    m_builder.call_list(1);
    m_builder.clear    (COLOR_BUFFER_BIT);
    feed();

    EXPECT_TRUE(m_harness.get_builder().get_call_draw_key_vec().empty() );
}
