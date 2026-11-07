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
    class GlFrontendLists : public Tests::GlFrontendFixture
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
        Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence get_eye_dependence(const uint32_t& in_list)
        {
            Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence result = Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::INDEPENDENT;

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

TEST_F(GlFrontendLists, ACallOfAListMakesItsCalls)
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

TEST_F(GlFrontendLists, AListIsMadeEachTimeItIsCalled)
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

TEST_F(GlFrontendLists, AListWhichIsNotCalledMakesNothing)
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

TEST_F(GlFrontendLists, ACallMadeWhileAListIsMadeIsNotMadeThen)
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

TEST_F(GlFrontendLists, ACallOfAListMakesItsStateChangesEachTime)
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

TEST_F(GlFrontendLists, AListWithAnotherPipelineStateMakesAnotherDraw)
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

TEST_F(GlFrontendLists, AListCanCallAnotherList)
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

TEST_F(GlFrontendLists, AListWhichCallsItselfIsNotMadeForEver)
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

TEST_F(GlFrontendLists, ADeletedListIsNotCalled)
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

TEST_F(GlFrontendLists, AListWhichWasNeverMadeIsNotFollowed)
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

TEST_F(GlFrontendLists, ACompileAndExecuteListIsMadeAtOnceAndWhenCalled)
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

TEST_F(GlFrontendLists, ADrawOfArraysInAListKeepsItsData)
{
    typedef Tests::GlCallBuilder::ClientArray ClientArray;

    const ClientArray vertices = Tests::GlCallBuilder::make_vertex_array({{0.0f, 0.0f, -1.0f},
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

TEST_F(GlFrontendLists, AListMadeInOneContextIsCalledInAnother)
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

TEST_F(GlFrontendLists, AListWhichOnlyDrawsAndSetsStateIsEyeIndependent)
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
              Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::INDEPENDENT);
}

TEST_F(GlFrontendLists, AListWhichMovesTheMatricesRelativeToTheCurrentOnesIsToldSo)
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
              Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::RELATIVE_MATRIX_ONLY);
}

TEST_F(GlFrontendLists, AListWhichSetsAValueOfItsOwnIsEyeDependent)
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
                  Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::EYE_DEPENDENT) << "list " << list;
    }
}

TEST_F(GlFrontendLists, TheStrongestCallOfAListTellsWhatItIs)
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
              Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::EYE_DEPENDENT);
}

TEST_F(GlFrontendLists, AListWhichCallsAnotherIsWhatTheOtherIs)
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
              Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::INDEPENDENT);
    EXPECT_EQ(get_eye_dependence(5),
              Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::RELATIVE_MATRIX_ONLY);
    EXPECT_EQ(get_eye_dependence(6),
              Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::EYE_DEPENDENT);
    EXPECT_EQ(get_eye_dependence(7),
              Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::EYE_DEPENDENT);
}

TEST_F(GlFrontendLists, AListWhichWasNeverMadeHasNoDependence)
{
    start(640,
          480);

    feed();

    Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence dependence = Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence::INDEPENDENT;

    EXPECT_FALSE(m_harness.get_frontend().find_list_eye_dependence(42,
                                                                   &dependence) );
}
