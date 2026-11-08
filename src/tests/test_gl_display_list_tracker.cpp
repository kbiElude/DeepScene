/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for GlDisplayListTracker: what the lists that a replay makes do to what an eye has of its own, as the calls come.
 */
#include <gtest/gtest.h>
#include "gl_display_list_tracker.h"

namespace
{
    typedef Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence Dependence;

    Capture::APIDumpLoader::DumpedAPICall make_call(const APIInterceptor::APIFunction& in_function,
                                                    const int&                         in_first_number = 0,
                                                    const int&                         in_second_number = 0)
    {
        Capture::APIDumpLoader::DumpedAPICall result;

        result.func = in_function;

        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_i32(in_first_number) );
        result.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_i32(in_second_number) );

        return result;
    }

    /* A list of the number that has the calls in it. */
    void make_list(Stereo::Replay::EngineDLL::GlDisplayListTracker*   inout_tracker_ptr,
                   const int&                                          in_list,
                   const std::vector<APIInterceptor::APIFunction>&     in_function_vec)
    {
        inout_tracker_ptr->on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                                             in_list) );

        for (const auto function : in_function_vec)
        {
            inout_tracker_ptr->on_call(make_call(function) );
        }

        inout_tracker_ptr->on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) );
    }
}

TEST(GlDisplayListTracker, AListThatDoesDrawsAndStateOnlyIsIndependent)
{
    Stereo::Replay::EngineDLL::GlDisplayListTracker tracker;
    Dependence                                      dependence = Dependence::EYE_DEPENDENT;

    make_list(&tracker,
              3,
              {APIInterceptor::APIFUNCTION_GL_GLENABLE,
               APIInterceptor::APIFUNCTION_GL_GLBEGIN,
               APIInterceptor::APIFUNCTION_GL_GLVERTEX3F,
               APIInterceptor::APIFUNCTION_GL_GLEND});

    ASSERT_TRUE(tracker.find_eye_dependence(3,
                                            &dependence) );
    EXPECT_EQ(dependence,
              Dependence::INDEPENDENT);
}

TEST(GlDisplayListTracker, AListThatLoadsAMatrixIsEyeDependentAndOneThatMovesItIsRelative)
{
    Stereo::Replay::EngineDLL::GlDisplayListTracker tracker;
    Dependence                                      dependence = Dependence::INDEPENDENT;

    make_list(&tracker, 1, {APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY});
    make_list(&tracker, 2, {APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF});

    ASSERT_TRUE(tracker.find_eye_dependence(1,
                                            &dependence) );
    EXPECT_EQ(dependence,
              Dependence::EYE_DEPENDENT);

    ASSERT_TRUE(tracker.find_eye_dependence(2,
                                            &dependence) );
    EXPECT_EQ(dependence,
              Dependence::RELATIVE_MATRIX_ONLY);
}

TEST(GlDisplayListTracker, AListTakesTheClassOfTheListsItCalls)
{
    Stereo::Replay::EngineDLL::GlDisplayListTracker tracker;
    Dependence                                      dependence = Dependence::INDEPENDENT;

    make_list(&tracker, 1, {APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF});

    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                              2) );
    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                              1) );
    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) );

    ASSERT_TRUE(tracker.find_eye_dependence(2,
                                            &dependence) );
    EXPECT_EQ(dependence,
              Dependence::RELATIVE_MATRIX_ONLY);
}

TEST(GlDisplayListTracker, AListThatCallsOneThatIsNotDefinedIsEyeDependent)
{
    Stereo::Replay::EngineDLL::GlDisplayListTracker tracker;
    Dependence                                      dependence = Dependence::INDEPENDENT;

    make_list(&tracker, 2, {APIInterceptor::APIFUNCTION_GL_GLCALLLIST});

    ASSERT_TRUE(tracker.find_eye_dependence(2,
                                            &dependence) );
    EXPECT_EQ(dependence,
              Dependence::EYE_DEPENDENT);
}

TEST(GlDisplayListTracker, ADeletedListIsNotThereAndTheCallsOutsideAListAreNotItsOwn)
{
    Stereo::Replay::EngineDLL::GlDisplayListTracker tracker;
    Dependence                                      dependence = Dependence::INDEPENDENT;

    make_list(&tracker, 4, {APIInterceptor::APIFUNCTION_GL_GLBEGIN});

    EXPECT_FALSE(tracker.is_recording() );

    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY) );

    ASSERT_TRUE(tracker.find_eye_dependence(4,
                                            &dependence) );
    EXPECT_EQ(dependence,
              Dependence::INDEPENDENT);

    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLDELETELISTS,
                              4,
                              1) );

    EXPECT_FALSE(tracker.find_eye_dependence(4,
                                             &dependence) );
}

TEST(GlDisplayListTracker, ItIsRecordingBetweenTheNewListAndTheEndList)
{
    Stereo::Replay::EngineDLL::GlDisplayListTracker tracker;

    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                              9) );

    EXPECT_TRUE(tracker.is_recording() );

    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) );

    EXPECT_FALSE(tracker.is_recording() );
}

TEST(GlDisplayListTracker, AListThatOnlyDrawsDoesNotSetState)
{
    Stereo::Replay::EngineDLL::GlDisplayListTracker tracker;
    bool                                            sets_state = true;

    make_list(&tracker,
              1,
              {APIInterceptor::APIFUNCTION_GL_GLBEGIN,
               APIInterceptor::APIFUNCTION_GL_GLVERTEX3F,
               APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV,
               APIInterceptor::APIFUNCTION_GL_GLEND});

    ASSERT_TRUE(tracker.find_sets_state(1,
                                        &sets_state) );
    EXPECT_FALSE(sets_state);
}

namespace
{
    /* A list of the one call, which has to be told to set state. */
    void expect_list_of_call_sets_state(const APIInterceptor::APIFunction& in_function)
    {
        Stereo::Replay::EngineDLL::GlDisplayListTracker tracker;
        bool                                            sets_state = false;

        make_list(&tracker,
                  1,
                  {in_function});

        ASSERT_TRUE(tracker.find_sets_state(1,
                                            &sets_state) );
        EXPECT_TRUE(sets_state);
    }
}

TEST(GlDisplayListTracker, AListThatDoesAnythingButDrawSetsState)
{
    /* A capability, a texture coordinate for the vertices that follow, a material, an object that is bound, a light. */
    expect_list_of_call_sets_state(APIInterceptor::APIFUNCTION_GL_GLENABLE);
    expect_list_of_call_sets_state(APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2F);
    expect_list_of_call_sets_state(APIInterceptor::APIFUNCTION_GL_GLMATERIALFV);
    expect_list_of_call_sets_state(APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE);
    expect_list_of_call_sets_state(APIInterceptor::APIFUNCTION_GL_GLLIGHTFV);
}

TEST(GlDisplayListTracker, AListThatCallsOneThatSetsStateOrOneThatIsNotDefinedSetsState)
{
    Stereo::Replay::EngineDLL::GlDisplayListTracker tracker;
    bool                                            sets_state = false;

    make_list(&tracker,
              1,
              {APIInterceptor::APIFUNCTION_GL_GLENABLE});

    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                              2) );
    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                              1) );
    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) );

    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLNEWLIST,
                              3) );
    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLCALLLIST,
                              99) );
    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLENDLIST) );

    ASSERT_TRUE(tracker.find_sets_state(2,
                                        &sets_state) );
    EXPECT_TRUE(sets_state);

    sets_state = false;

    ASSERT_TRUE(tracker.find_sets_state(3,
                                        &sets_state) );
    EXPECT_TRUE(sets_state);
}

TEST(GlDisplayListTracker, AListThatIsDeletedHasNothingToTellOf)
{
    Stereo::Replay::EngineDLL::GlDisplayListTracker tracker;
    bool                                            sets_state = false;

    make_list(&tracker,
              1,
              {APIInterceptor::APIFUNCTION_GL_GLENABLE});

    tracker.on_call(make_call(APIInterceptor::APIFUNCTION_GL_GLDELETELISTS,
                              1,
                              1) );

    EXPECT_FALSE(tracker.find_sets_state(1,
                                         &sets_state) );
}
