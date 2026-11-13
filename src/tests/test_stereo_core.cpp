/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for StereoCore: what a converter which plays an application as it runs decides from a baked recipe, and when it refuses to.
 */
#include <gtest/gtest.h>
#include "analysis/recipe/recipe_loader.h"
#include "stereo/core/stereo_core.h"
#include <math.h>
#include <string>
#include <vector>

class StereoCoreTest : public ::testing::Test
{
protected:
    /* The matrices for the draw, as a vector of their own. */
    static std::vector<Stereo::Core::MatrixChange> bind_matrices(Stereo::Core::StereoCore*      inout_core_ptr,
                                                                 const Analysis::Recipe::DrawClass& in_draw_class,
                                                                 const Analysis::Recipe::Eye&       in_eye,
                                                                 const bool&                              in_is_final_target = false)
    {
        std::vector<Stereo::Core::MatrixChange> result_vec;

        result_vec.resize(inout_core_ptr->bind_matrices(in_draw_class,
                                                        in_eye,
                                                       &result_vec,
                                                        in_is_final_target) );

        return result_vec;
    }

    static constexpr uint32_t STATE_PROJECTION = 3;   /* D3DTS_PROJECTION */
    static constexpr uint32_t STATE_VIEW       = 2;   /* D3DTS_VIEW       */
    static constexpr uint32_t STATE_WORLD      = 256; /* D3DTS_WORLD      */

    static Analysis::Recipe::Recipe make_recipe(const std::string& in_baked_part,
                                                const std::string& in_rule_part)
    {
        Analysis::Recipe::Recipe recipe;
        std::string              error;

        EXPECT_TRUE(Analysis::Recipe::RecipeLoader::parse("{\"recipe_version\": 0, \"key_scheme_version\": 1, " + in_baked_part +
                                                          "\"rules\": [" + in_rule_part + "]}",
                                                          &recipe,
                                                          &error) ) << error;

        return recipe;
    }

    static std::string get_view_rule_text()
    {
        return "{\"call\": \"IDirect3DDevice8::SetTransform\", \"when\": {\"state\": \"D3DTS_VIEW\"},"
               " \"action\": {\"type\": \"eye_offset\", \"order\": \"post\", \"axis\": \"x\"}}";
    }

    static std::string get_baked_text(const std::string& in_draw_rules)
    {
        return "\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [" + in_draw_rules +
               "], \"known_signatures\": [\"0x0000000000000001\", \"0x0000000000000002\"]},";
    }

    static std::vector<APIInterceptor::APIFunctionArgument> make_set_transform_arguments(const uint32_t& in_state)
    {
        std::vector<APIInterceptor::APIFunctionArgument> result_vec;

        result_vec.push_back(APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr) );
        result_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32     (in_state) );
        result_vec.push_back(APIInterceptor::APIFunctionArgument::create_data_chunk_id(1) );

        return result_vec;
    }

    static std::array<float, 16> make_translation(const float& in_x)
    {
        return {1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                in_x, 0.0f, 0.0f, 1.0f};
    }

    static constexpr APIInterceptor::APIFunction SET_TRANSFORM = APIInterceptor::APIFunction::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM;
};

TEST_F(StereoCoreTest, ARecipeThatIsNotBakedIsRefused)
{
    std::vector<std::string> message_vec;

    Stereo::Core::StereoCore core(make_recipe("",
                                              ""),
                                  [&message_vec](const std::string& in_message)
                                  {
                                      message_vec.push_back(in_message);
                                  });

    ASSERT_EQ(message_vec.size(),
              1u);
    EXPECT_NE(message_vec.at(0).find("not baked"),
              std::string::npos);
}

TEST_F(StereoCoreTest, ARuleAboutAPassOrAFrameShapeIsRefusedForItCannotBeFollowedLive)
{
    std::vector<std::string> message_vec;

    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              "{\"name\": \"scene\", \"call\": \"IDirect3DDevice8::SetTransform\","
                                              " \"when\": {\"state\": \"D3DTS_VIEW\", \"pass_keys\": [\"0x0000000000000001\"]},"
                                              " \"action\": {\"type\": \"eye_offset\", \"order\": \"post\", \"axis\": \"x\"}}"),
                                  [&message_vec](const std::string& in_message)
                                  {
                                      message_vec.push_back(in_message);
                                  });

    ASSERT_EQ(message_vec.size(),
              1u);
    EXPECT_NE(message_vec.at(0).find("\"scene\""),
              std::string::npos);
    EXPECT_NE(message_vec.at(0).find("hold the calls of a pass back"),
              std::string::npos);
}

TEST_F(StereoCoreTest, TheMatrixOfARuleIsKeptAndPutInForTheEyeThatDrawsAreMadeFor)
{
    const auto               arguments   = make_set_transform_arguments(STATE_VIEW);
    std::vector<std::string> message_vec;
    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              get_view_rule_text() ),
                                  [&message_vec](const std::string& in_message)
                                  {
                                      message_vec.push_back(in_message);
                                  });

    core.set_eye_separation(0.5f);

    /* The rule is about this call: the converter keeps it, the caller does not execute it. */
    ASSERT_TRUE(core.on_matrix_call(SET_TRANSFORM,
                                    arguments,
                                    make_translation(10.0f) ));

    const auto left  = bind_matrices(&core,
                                     Analysis::Recipe::DrawClass::AFFECTED,
                                     Analysis::Recipe::Eye::LEFT);
    const auto right = bind_matrices(&core,
                                     Analysis::Recipe::DrawClass::AFFECTED,
                                     Analysis::Recipe::Eye::RIGHT);

    ASSERT_EQ(left.size(),
              1u);
    ASSERT_EQ(right.size(),
              1u);

    EXPECT_EQ(left.at(0).function,
              SET_TRANSFORM);
    ASSERT_EQ(left.at(0).argument_vec.size(),
              3u);
    EXPECT_EQ(left.at(0).argument_vec.at(1).get_u32(),
              STATE_VIEW);

    /* Half of the separation to each side of the application's camera. */
    EXPECT_FLOAT_EQ(left.at (0).matrix_fp32vec16.at(12) - 10.0f,
                    -(right.at(0).matrix_fp32vec16.at(12) - 10.0f) );
    EXPECT_FLOAT_EQ(::fabsf(left.at(0).matrix_fp32vec16.at(12) - right.at(0).matrix_fp32vec16.at(12) ),
                    0.5f);
    EXPECT_TRUE(message_vec.empty() );
}

TEST_F(StereoCoreTest, AMatrixIsPutInOnlyWhenTheDeviceHasAnotherOne)
{
    const auto               arguments = make_set_transform_arguments(STATE_VIEW);
    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              get_view_rule_text() ),
                                  Stereo::Core::FailFunc() );

    core.set_eye_separation(0.5f);
    core.on_matrix_call(SET_TRANSFORM,
                        arguments,
                        make_translation(10.0f) );

    EXPECT_EQ(bind_matrices(&core,
                            Analysis::Recipe::DrawClass::AFFECTED,
                            Analysis::Recipe::Eye::LEFT).size(),
              1u);

    /* The device has it already. */
    EXPECT_TRUE(bind_matrices(&core,
                              Analysis::Recipe::DrawClass::AFFECTED,
                              Analysis::Recipe::Eye::LEFT).empty() );

    EXPECT_EQ(bind_matrices(&core,
                            Analysis::Recipe::DrawClass::AFFECTED,
                            Analysis::Recipe::Eye::RIGHT).size(),
              1u);

    /* A draw which the recipe leaves alone gets the matrix as the application set it. */
    const auto mono = bind_matrices(&core,
                                    Analysis::Recipe::DrawClass::UNAFFECTED,
                                    Analysis::Recipe::Eye::RIGHT);

    ASSERT_EQ(mono.size(),
              1u);
    EXPECT_EQ(mono.at(0).matrix_fp32vec16,
              make_translation(10.0f) );
    EXPECT_TRUE(bind_matrices(&core,
                              Analysis::Recipe::DrawClass::UNAFFECTED,
                              Analysis::Recipe::Eye::LEFT).empty() );
}

TEST_F(StereoCoreTest, AnotherSeparationMakesTheMatrixAgain)
{
    const auto               arguments = make_set_transform_arguments(STATE_VIEW);
    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              get_view_rule_text() ),
                                  Stereo::Core::FailFunc() );

    core.set_eye_separation(0.5f);
    core.on_matrix_call(SET_TRANSFORM,
                        arguments,
                        make_translation(10.0f) );

    const auto first = bind_matrices(&core,
                                     Analysis::Recipe::DrawClass::AFFECTED,
                                     Analysis::Recipe::Eye::LEFT);

    core.set_eye_separation(1.0f);

    const auto second = bind_matrices(&core,
                                      Analysis::Recipe::DrawClass::AFFECTED,
                                      Analysis::Recipe::Eye::LEFT);

    ASSERT_EQ(first.size(),
              1u);
    ASSERT_EQ(second.size(),
              1u);
    EXPECT_NE(first.at(0).matrix_fp32vec16.at(12),
              second.at(0).matrix_fp32vec16.at(12) );
}

TEST_F(StereoCoreTest, ACallThatNoRuleIsAboutIsLeftToTheCaller)
{
    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              get_view_rule_text() ),
                                  Stereo::Core::FailFunc() );

    EXPECT_FALSE(core.on_matrix_call(SET_TRANSFORM,
                                     make_set_transform_arguments(STATE_WORLD),
                                     make_translation(1.0f) ));
    EXPECT_FALSE(core.on_matrix_call(APIInterceptor::APIFunction::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETVIEWPORT,
                                     make_set_transform_arguments(STATE_VIEW),
                                     make_translation(1.0f) ));
    EXPECT_TRUE(bind_matrices(&core,
                              Analysis::Recipe::DrawClass::AFFECTED,
                              Analysis::Recipe::Eye::LEFT).empty() );
}

TEST_F(StereoCoreTest, MatricesThatWereKeptCanBeForgotten)
{
    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              get_view_rule_text() ),
                                  Stereo::Core::FailFunc() );

    core.on_matrix_call(SET_TRANSFORM,
                        make_set_transform_arguments(STATE_VIEW),
                        make_translation(10.0f) );
    core.forget_matrices();

    EXPECT_TRUE(bind_matrices(&core,
                              Analysis::Recipe::DrawClass::AFFECTED,
                              Analysis::Recipe::Eye::LEFT).empty() );
}

TEST_F(StereoCoreTest, ADrawIsToldByItsSignatureAndTheViewMatrixTheApplicationHas)
{
    Stereo::Core::StereoCore core(make_recipe(get_baked_text("{\"signature\": \"0x0000000000000001\", \"class\": \"unaffected\","
                                                             " \"when\": {\"constant\": \"view_matrix\","
                                                             " \"equals_any\": [[1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,2.5,1]]}},"
                                                             "{\"signature\": \"0x0000000000000002\", \"class\": \"skipped\"}"),
                                              get_view_rule_text() ),
                                  Stereo::Core::FailFunc() );

    /* The application has set no camera: the device has the identity. */
    EXPECT_EQ(core.classify_draw(1, 0),
              Analysis::Recipe::DrawClass::AFFECTED);
    EXPECT_EQ(core.classify_draw(2, 0),
              Analysis::Recipe::DrawClass::SKIPPED);

    core.on_view_matrix_set({1.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, 1.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 1.0f, 0.0f,
                             0.0f, 0.0f, 2.5f, 1.0f});

    EXPECT_EQ(core.classify_draw(1, 0),
              Analysis::Recipe::DrawClass::UNAFFECTED);
}

TEST_F(StereoCoreTest, ADrawThatIsNamedBySizeAsPushedIsFlatAndPushed)
{
    Stereo::Core::StereoCore core(make_recipe("\"pushed\": {\"cover_keys\": [\"0x0000000000000005\"]}, " + get_baked_text(""),
                                              get_view_rule_text() ),
                                  Stereo::Core::FailFunc() );

    /* The same signature: the draw of that size is flat, the others are as the recipe says. */
    EXPECT_EQ(core.classify_draw(1, 5),
              Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_TRUE(core.get_is_last_draw_pushed() );

    EXPECT_EQ(core.classify_draw(1, 6),
              Analysis::Recipe::DrawClass::AFFECTED);
    EXPECT_FALSE(core.get_is_last_draw_pushed() );
}

TEST_F(StereoCoreTest, APartThatProtrudesFollowsTheDepthOfItsOriginAndOthersDoNot)
{
    Stereo::Core::StereoCore core(make_recipe("\"pushed\": {\"part_cover_keys\": [\"0x0000000000000005\"], \"cover_keys\": [\"0x0000000000000007\"]}, " + get_baked_text(""),
                                              get_view_rule_text() ),
                                  Stereo::Core::FailFunc() );

    core.set_convergence_distance(100.0f);
    core.set_eye_separation(8.0f);

    core.classify_draw(1, 5);

    EXPECT_TRUE(core.get_is_last_draw_part() );

    /* At the distance of convergence a point has no parallax; nearer, the left eye moves it to the right (a perspective projection of scale 1: x / -z). */
    const float distance      = core.get_effective_convergence_distance();
    const float at_convergence = core.plan_follow_shift(Analysis::Recipe::Eye::LEFT, 640.0f, -distance, 1.0f, true);
    const float nearer         = core.plan_follow_shift(Analysis::Recipe::Eye::LEFT, 640.0f, -distance * 0.5f, 1.0f, true);
    const float right_eye      = core.plan_follow_shift(Analysis::Recipe::Eye::RIGHT, 640.0f, -distance * 0.5f, 1.0f, true);

    EXPECT_FLOAT_EQ(at_convergence, 0.0f);
    EXPECT_GT      (nearer,         0.0f);
    EXPECT_FLOAT_EQ(right_eye,      -nearer);

    /* A draw that is not a part, and a point behind the camera, are not moved. */
    core.classify_draw(1, 7);

    EXPECT_FALSE(core.get_is_last_draw_part() );
    EXPECT_FLOAT_EQ(core.plan_follow_shift(Analysis::Recipe::Eye::LEFT, 640.0f, -distance * 0.5f, 1.0f, true), 0.0f);
}

TEST_F(StereoCoreTest, APartThatIsNearTheCameraIsHeldToTheNearestParallaxOfTheScene)
{
    Stereo::Core::StereoCore core(make_recipe(std::string("\"comfort\": {\"max_near_parallax\": 0.01, \"max_far_parallax\": 0.01, \"overlay_parallax\": 0.015, \"part_parallax\": 0.006}, ") +
                                              "\"pushed\": {\"part_cover_keys\": [\"0x0000000000000005\"]}, " + get_baked_text(""),
                                              get_view_rule_text() ),
                                  Stereo::Core::FailFunc() );

    core.set_convergence_distance(100.0f);
    core.set_eye_separation(8.0f);
    core.classify_draw(1, 5);

    /* The origin is 100 times nearer than the distance of convergence: followed without a limit, the shift is far more than the scene allows. */
    const float left  = core.plan_part_shift(Analysis::Recipe::Eye::LEFT,  640.0f, -1.0f, 1.0f, true);
    const float right = core.plan_part_shift(Analysis::Recipe::Eye::RIGHT, 640.0f, -1.0f, 1.0f, true);

    EXPECT_GT(core.plan_follow_shift(Analysis::Recipe::Eye::LEFT, 640.0f, -1.0f, 1.0f, true), 3.2f);
    EXPECT_FLOAT_EQ(left,   3.2f); /* 0.01 of 640 pixels, half to each eye */
    EXPECT_FLOAT_EQ(right, -3.2f);

    /* At a depth the scene allows, the shift is the follow shift and the push. */
    const float inside = core.plan_part_shift(Analysis::Recipe::Eye::LEFT, 640.0f, -100.0f, 1.0f, true);

    EXPECT_FLOAT_EQ(inside, core.plan_push_shift(Analysis::Recipe::Eye::LEFT, 640.0f) );
}

TEST_F(StereoCoreTest, DrawsOfASignatureTheRecipeDoesNotKnowAreCounted)
{
    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              get_view_rule_text() ),
                                  Stereo::Core::FailFunc() );

    core.classify_draw(1, 0);
    core.classify_draw(2, 0);

    EXPECT_EQ(core.get_n_unknown_draws(),
              0u);

    /* It is affected, like any draw no rule is about. */
    EXPECT_EQ(core.classify_draw(99, 0),
              Analysis::Recipe::DrawClass::AFFECTED);
    EXPECT_EQ(core.get_n_unknown_draws(),
              1u);
}

TEST_F(StereoCoreTest, AnAdapterThatFindsWhatCannotBeConvertedRightFailsTheConverter)
{
    std::vector<std::string> message_vec;

    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              get_view_rule_text() ),
                                  [&message_vec](const std::string& in_message)
                                  {
                                      message_vec.push_back(in_message);
                                  });

    core.require_fast_path(true, /* in_is_ok */
                           "nothing is wrong");

    EXPECT_TRUE(message_vec.empty() );

    core.require_fast_path(false, /* in_is_ok */
                           "A draw reads a texture it also renders to");

    ASSERT_EQ(message_vec.size(),
              1u);
    EXPECT_NE(message_vec.at(0).find("A draw reads a texture it also renders to"),
              std::string::npos);
    EXPECT_NE(message_vec.at(0).find("hold the calls of a pass back"),
              std::string::npos);
}

TEST_F(StereoCoreTest, ASeparationThatIsNotAmountToAnythingIsRefused)
{
    std::vector<std::string> message_vec;

    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              get_view_rule_text() ),
                                  [&message_vec](const std::string& in_message)
                                  {
                                      message_vec.push_back(in_message);
                                  });

    core.set_eye_separation(0.25f);
    core.set_eye_separation(-1.0f);
    core.set_eye_separation(NAN);

    EXPECT_EQ(message_vec.size(),
              2u);
    EXPECT_FLOAT_EQ(core.get_eye_separation(),
                    0.25f);
}

TEST_F(StereoCoreTest, TheConvergenceDistanceShearsTheProjectionForEachEyeAndIsSaidAgainWhenItChanges)
{
    const auto               projection_arguments = make_set_transform_arguments(STATE_PROJECTION);
    std::vector<std::string> message_vec;
    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              "{\"call\": \"IDirect3DDevice8::SetTransform\", \"when\": {\"state\": \"D3DTS_PROJECTION\","
                                              " \"matrix\": \"perspective\"}, \"action\": {\"type\": \"eye_convergence\"}}"),
                                  [&message_vec](const std::string& in_message)
                                  {
                                      message_vec.push_back(in_message);
                                  });
    std::array<float, 16>    projection           = {1.0f, 0.0f, 0.0f, 0.0f,
                                               0.0f, 1.0f, 0.0f, 0.0f,
                                               0.0f, 0.0f, 1.0f, 1.0f,
                                               0.0f, 0.0f, 0.0f, 0.0f};

    core.set_eye_separation(0.2f);

    ASSERT_TRUE(core.on_matrix_call(SET_TRANSFORM,
                                    projection_arguments,
                                    projection) );

    /* None is set: the application's projection, for the eye. */
    auto left = bind_matrices(&core,
                              Analysis::Recipe::DrawClass::AFFECTED,
                              Analysis::Recipe::Eye::LEFT);

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    0.0f);

    core.set_convergence_distance(5.0f);

    EXPECT_FLOAT_EQ(core.get_convergence_distance(),
                    5.0f);

    /* A projection made for another distance is not the one to have. */
    left = bind_matrices(&core,
                         Analysis::Recipe::DrawClass::AFFECTED,
                         Analysis::Recipe::Eye::LEFT);

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    -0.02f);

    const auto right = bind_matrices(&core,
                                     Analysis::Recipe::DrawClass::AFFECTED,
                                     Analysis::Recipe::Eye::RIGHT);

    ASSERT_EQ(right.size(),
              1u);
    EXPECT_FLOAT_EQ(right.at(0).matrix_fp32vec16.at(8),
                    0.02f);

    EXPECT_TRUE(bind_matrices(&core,
                              Analysis::Recipe::DrawClass::AFFECTED,
                              Analysis::Recipe::Eye::RIGHT).empty() );

    core.set_convergence_distance(-1.0f);
    core.set_convergence_distance(NAN);

    EXPECT_EQ(message_vec.size(),
              2u);
    EXPECT_FLOAT_EQ(core.get_convergence_distance(),
                    5.0f);
}

TEST_F(StereoCoreTest, TheDistanceOfTheRecipeIsFollowedForEachDrawUnlessOneIsSet)
{
    const auto               projection_arguments = make_set_transform_arguments(STATE_PROJECTION);
    std::vector<std::string> message_vec;
    auto                     recipe               = make_recipe(get_baked_text(""),
                                                                "{\"call\": \"IDirect3DDevice8::SetTransform\", \"when\": {\"state\": \"D3DTS_PROJECTION\","
                                                                " \"matrix\": \"perspective\"}, \"action\": {\"type\": \"eye_convergence\"}}");
    std::array<float, 16>    projection_f32mat    = {1.0f, 0.0f, 0.0f, 0.0f,
                                                     0.0f, 1.0f, 0.0f, 0.0f,
                                                     0.0f, 0.0f, 1.0f, 1.0f,
                                                     0.0f, 0.0f, 0.0f, 0.0f};

    /* The signatures of get_baked_text() are 1 and 2: the first converges at 5, the second has no distance. */
    recipe.convergence_vec.push_back({5.0f, 1});

    Stereo::Core::StereoCore core(recipe,
                                  [&message_vec](const std::string& in_message)
                                  {
                                      message_vec.push_back(in_message);
                                  });

    core.set_eye_separation(0.2f);

    ASSERT_TRUE(core.on_matrix_call(SET_TRANSFORM,
                                    projection_arguments,
                                    projection_f32mat) );

    core.classify_draw(1, 0);

    auto left = bind_matrices(&core,
                              Analysis::Recipe::DrawClass::AFFECTED,
                              Analysis::Recipe::Eye::LEFT);

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    -0.02f);

    /* A draw with no distance has the projection of the application, for the eye. */
    core.classify_draw(2, 0);

    left = bind_matrices(&core,
                         Analysis::Recipe::DrawClass::AFFECTED,
                         Analysis::Recipe::Eye::LEFT);

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    0.0f);

    /* A distance that is set is for every draw, whatever the recipe says. */
    core.set_convergence_distance(10.0f);
    core.classify_draw(1, 0);

    left = bind_matrices(&core,
                         Analysis::Recipe::DrawClass::AFFECTED,
                         Analysis::Recipe::Eye::LEFT);

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    -0.01f);
    EXPECT_TRUE(message_vec.empty() );
}

TEST_F(StereoCoreTest, TheEyesGoTowardASceneByTheTimeThatTheFrameTookAndNotByTheNumberOfFrames)
{
    auto recipe = make_recipe(get_baked_text(""),
                              "");

    recipe.scene_convergence_vec.push_back({10.0f, 100});
    recipe.scene_convergence_vec.push_back({40.0f, 200});

    /* The distance after one frame of the second scene that took this long, from the first scene taken at once. */
    const auto get_distance_func = [&recipe](const float& in_frame_seconds)
    {
        Stereo::Core::StereoCore core(recipe,
                                      [](const std::string&)
                                      {
                                      });

        core.on_frame_completed(100);
        core.set_frame_interval_seconds(in_frame_seconds);
        core.on_frame_completed(200);

        return core.get_effective_convergence_distance();
    };

    const float reference_seconds = Stereo::Core::ConvergenceModel::get_reference_frame_seconds();

    /* A twentieth of the way along the logarithm for a frame of 72 Hz: 10 * 4 ^ 0.05. */
    EXPECT_NEAR(get_distance_func(reference_seconds),
                10.0f * ::powf(4.0f, 0.05f),
                0.001f);

    /* A frame of twice the time goes further, and one of a thousandth of a second hardly moves. */
    EXPECT_GT(get_distance_func(2.0f * reference_seconds),
              get_distance_func(reference_seconds) + 0.5f);
    EXPECT_LT(get_distance_func(0.001f),
              10.1f);

    /* A stall of seconds puts the eyes at the distance of the scene. */
    EXPECT_NEAR(get_distance_func(5.0f),
                40.0f,
                0.1f);
}

TEST_F(StereoCoreTest, TheDistanceOfTheSceneIsTakenAtOnceAtFirstAndThenEasedTowardWhatTheNextSceneHasAndASetOneBeatsIt)
{
    const auto               projection_arguments = make_set_transform_arguments(STATE_PROJECTION);
    std::vector<std::string> message_vec;
    auto                     recipe               = make_recipe(get_baked_text(""),
                                                                "{\"call\": \"IDirect3DDevice8::SetTransform\", \"when\": {\"state\": \"D3DTS_PROJECTION\","
                                                                " \"matrix\": \"perspective\"}, \"action\": {\"type\": \"eye_convergence\"}}");
    std::array<float, 16>    projection_f32mat    = {1.0f, 0.0f, 0.0f, 0.0f,
                                                     0.0f, 1.0f, 0.0f, 0.0f,
                                                     0.0f, 0.0f, 1.0f, 1.0f,
                                                     0.0f, 0.0f, 0.0f, 0.0f};

    /* Two scenes, with the shape keys 100 and 200; the signature 1 converges at 5, and the draw is told of it until a scene is known. */
    recipe.convergence_vec.push_back({5.0f, 1});
    recipe.scene_convergence_vec.push_back({10.0f, 100});
    recipe.scene_convergence_vec.push_back({40.0f, 200});

    Stereo::Core::StereoCore core(recipe,
                                  [&message_vec](const std::string& in_message)
                                  {
                                      message_vec.push_back(in_message);
                                  });

    core.set_eye_separation(0.2f);

    ASSERT_TRUE(core.on_matrix_call(SET_TRANSFORM,
                                    projection_arguments,
                                    projection_f32mat) );

    const auto get_shear_func = [&core]()
                                {
                                    core.classify_draw(1, 0);

                                    const auto left = bind_matrices(&core,
                                                                    Analysis::Recipe::DrawClass::AFFECTED,
                                                                    Analysis::Recipe::Eye::LEFT);

                                    return (left.empty() ) ? 0.0f
                                                           : left.at(0).matrix_fp32vec16.at(8);
                                };

    /* No scene yet: the distance of the signature, 5. */
    EXPECT_FLOAT_EQ(get_shear_func(),
                    -0.1f / 5.0f);

    /* The first scene is taken as it is: 10. */
    core.on_frame_completed(100);

    EXPECT_FLOAT_EQ(get_shear_func(),
                    -0.1f / 10.0f);

    /* The next is approached, not jumped to, and a frame of a shape the recipe does not know changes nothing but the count. */
    core.on_frame_completed(200);

    const float eased = get_shear_func();

    EXPECT_LT(eased,
              -0.1f / 40.0f);
    EXPECT_GT(eased,
              -0.1f / 10.0f);

    core.on_frame_completed(999);

    EXPECT_EQ(core.get_n_unknown_frames(),
              1u);

    for (int n_frame = 0;
             n_frame < 400;
           ++n_frame)
    {
        core.on_frame_completed(200);
    }

    EXPECT_FLOAT_EQ(get_shear_func(),
                    -0.1f / 40.0f);

    /* A distance that is set is for every draw. */
    core.set_convergence_distance(2.0f);

    EXPECT_FLOAT_EQ(get_shear_func(),
                    -0.1f / 2.0f);
    EXPECT_TRUE(message_vec.empty() );
}

TEST_F(StereoCoreTest, TheStrengthOfThe3DMakesTheDistanceBetweenTheEyesAThirtiethOfTheDistanceTheyConvergeAtTimesIt)
{
    const auto               arguments   = make_set_transform_arguments(STATE_VIEW);
    std::vector<std::string> message_vec;
    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              get_view_rule_text() ),
                                  [&message_vec](const std::string& in_message)
                                  {
                                      message_vec.push_back(in_message);
                                  });

    ASSERT_TRUE(core.on_matrix_call(SET_TRANSFORM,
                                    arguments,
                                    make_translation(10.0f) ));

    const auto get_separation_func = [&core]()
                                     {
                                         const auto left  = bind_matrices(&core,
                                                                          Analysis::Recipe::DrawClass::AFFECTED,
                                                                          Analysis::Recipe::Eye::LEFT);
                                         const auto right               = bind_matrices(&core,
                                                                                        Analysis::Recipe::DrawClass::AFFECTED,
                                                                                        Analysis::Recipe::Eye::RIGHT);

                                         return ::fabsf(left.at(0).matrix_fp32vec16.at(12) - right.at(0).matrix_fp32vec16.at(12) );
                                     };

    /* The distance between the eyes that is set is used until there is a strength and a distance to converge at. */
    core.set_eye_separation(0.25f);

    EXPECT_FLOAT_EQ(get_separation_func(),
                    0.25f);

    core.set_depth_strength(1.0f);

    EXPECT_FLOAT_EQ(get_separation_func(),
                    0.25f);

    /* A distance of 30 and the natural strength: 1. Twice the strength, twice that; none, and the distance set is the one again. */
    core.set_convergence_distance(30.0f);

    EXPECT_FLOAT_EQ(get_separation_func(),
                    1.0f);

    core.set_depth_strength(2.0f);

    EXPECT_FLOAT_EQ(get_separation_func(),
                    2.0f);

    core.set_convergence_distance(60.0f);

    EXPECT_FLOAT_EQ(get_separation_func(),
                    4.0f);

    core.set_depth_strength(0.0f);

    EXPECT_FLOAT_EQ(get_separation_func(),
                    0.25f);

    core.set_depth_strength(-1.0f);
    core.set_depth_strength(NAN);

    EXPECT_EQ(message_vec.size(),
              2u);
    EXPECT_FLOAT_EQ(core.get_depth_strength(),
                    0.0f);
}

TEST_F(StereoCoreTest, WhatTheRecipeLeavesAloneAndIsMadeToWhatIsPresentedIsPushedTowardTheViewer)
{
    const auto                  projection_arguments = make_set_transform_arguments(STATE_PROJECTION);
    Stereo::Core::StereoCore    core(make_recipe("\"comfort\": {\"overlay_parallax\": 0.04}," + get_baked_text(""),
                                                 "{\"call\": \"IDirect3DDevice8::SetTransform\", \"when\": {\"state\": \"D3DTS_PROJECTION\","
                                                 " \"matrix\": \"perspective\"}, \"action\": {\"type\": \"eye_convergence\"}}"),
                                     [](const std::string& in_message)
                                     {
                                         (void) in_message;
                                     });
    const std::array<float, 16> projection           = {1.0f, 0.0f, 0.0f, 0.0f,
                                               0.0f, 1.0f, 0.0f, 0.0f,
                                               0.0f, 0.0f, 1.0f, 1.0f,
                                               0.0f, 0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(core.on_matrix_call(SET_TRANSFORM,
                                    projection_arguments,
                                    projection) );

    core.set_is_overlay_pushed(true);

    /* Pushed: the left picture toward the right, the right one toward the left, by the parallax in all. */
    auto left = bind_matrices(&core,
                              Analysis::Recipe::DrawClass::UNAFFECTED,
                              Analysis::Recipe::Eye::LEFT,
                              true); /* in_is_final_target */

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    0.04f);

    auto right = bind_matrices(&core,
                               Analysis::Recipe::DrawClass::UNAFFECTED,
                               Analysis::Recipe::Eye::RIGHT,
                               true); /* in_is_final_target */

    ASSERT_EQ(right.size(),
              1u);
    EXPECT_FLOAT_EQ(right.at(0).matrix_fp32vec16.at(8),
                    -0.04f);

    /* A draw to a texture is not: what it makes is used elsewhere. The application's matrix is put back. */
    left = bind_matrices(&core,
                         Analysis::Recipe::DrawClass::UNAFFECTED,
                         Analysis::Recipe::Eye::LEFT,
                         false); /* in_is_final_target */

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    0.0f);

    /* Nor is the same draw again, which finds the matrix in the device. */
    EXPECT_TRUE(bind_matrices(&core,
                              Analysis::Recipe::DrawClass::UNAFFECTED,
                              Analysis::Recipe::Eye::LEFT,
                              false).empty() );

    /* An affected draw is for the eyes the way it always was. */
    left = bind_matrices(&core,
                         Analysis::Recipe::DrawClass::AFFECTED,
                         Analysis::Recipe::Eye::LEFT,
                         true); /* in_is_final_target */

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    0.0f);
}

TEST_F(StereoCoreTest, WithoutAParallaxForTheOverlayWhatTheRecipeLeavesAloneIsTheSameForBothEyes)
{
    const auto                  projection_arguments = make_set_transform_arguments(STATE_PROJECTION);
    Stereo::Core::StereoCore    core(make_recipe(get_baked_text(""),
                                                 "{\"call\": \"IDirect3DDevice8::SetTransform\", \"when\": {\"state\": \"D3DTS_PROJECTION\","
                                                 " \"matrix\": \"perspective\"}, \"action\": {\"type\": \"eye_convergence\"}}"),
                                     [](const std::string& in_message)
                                     {
                                         (void) in_message;
                                     });
    const std::array<float, 16> projection           = {1.0f, 0.0f, 0.0f, 0.0f,
                                               0.0f, 1.0f, 0.0f, 0.0f,
                                               0.0f, 0.0f, 1.0f, 1.0f,
                                               0.0f, 0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(core.on_matrix_call(SET_TRANSFORM,
                                    projection_arguments,
                                    projection) );

    const auto left = bind_matrices(&core,
                                    Analysis::Recipe::DrawClass::UNAFFECTED,
                                    Analysis::Recipe::Eye::LEFT,
                                    true); /* in_is_final_target */

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    0.0f);
    EXPECT_TRUE(bind_matrices(&core,
                              Analysis::Recipe::DrawClass::UNAFFECTED,
                              Analysis::Recipe::Eye::RIGHT,
                              true).empty() ); /* in_is_final_target */
}

TEST_F(StereoCoreTest, ASceneTheRecipeDoesNotKnowKeepsTheLimitOfTheSceneBeforeAndAKnownOneWithoutALimitLiftsIt)
{
    auto recipe = make_recipe(get_baked_text(""),
                              get_view_rule_text() );

    recipe.scene_convergence_vec.push_back({30.0f, 100});
    recipe.scene_convergence_vec.push_back({30.0f, 200});
    recipe.scene_separation_vec.push_back({0.25f, 100});

    Stereo::Core::StereoCore core(recipe,
                                  [](const std::string& in_message)
                                  {
                                      (void) in_message;
                                  });

    core.set_depth_strength(1.0f);

    const auto separation_func = [&core]()
                                 {
                                     const auto left  = bind_matrices(&core,
                                                                      Analysis::Recipe::DrawClass::AFFECTED,
                                                                      Analysis::Recipe::Eye::LEFT);
                                     const auto right           = bind_matrices(&core,
                                                                                Analysis::Recipe::DrawClass::AFFECTED,
                                                                                Analysis::Recipe::Eye::RIGHT);

                                     return (left.empty() || right.empty() ) ? 0.0f
                                                                             : ::fabsf(left.at(0).matrix_fp32vec16.at(12) - right.at(0).matrix_fp32vec16.at(12) );
                                 };

    ASSERT_TRUE(core.on_matrix_call(SET_TRANSFORM,
                                    make_set_transform_arguments(STATE_VIEW),
                                    make_translation(10.0f) ));

    /* A scene with a limit. */
    core.on_frame_completed(100);

    EXPECT_FLOAT_EQ(separation_func(),
                    0.25f);

    /* A scene that is not known: the limit stays. */
    core.on_frame_completed(999);

    EXPECT_FLOAT_EQ(separation_func(),
                    0.25f);

    /* A scene that is known and has none: no limit, a thirtieth of the distance. */
    for (int n_frame = 0;
             n_frame < 200;
           ++n_frame)
    {
        core.on_frame_completed(200);
    }

    EXPECT_FLOAT_EQ(separation_func(),
                    1.0f);
}

TEST_F(StereoCoreTest, WhatTheRecipeSaysOfTheOverlayIsHowFarAndTheOverlayIsNotPushedUnlessThePersonAsks)
{
    const auto                  projection_arguments = make_set_transform_arguments(STATE_PROJECTION);
    Stereo::Core::StereoCore    core(make_recipe("\"comfort\": {\"overlay_parallax\": 0.04}," + get_baked_text(""),
                                                 "{\"call\": \"IDirect3DDevice8::SetTransform\", \"when\": {\"state\": \"D3DTS_PROJECTION\","
                                                 " \"matrix\": \"perspective\"}, \"action\": {\"type\": \"eye_convergence\"}}"),
                                     [](const std::string& in_message)
                                     {
                                         (void) in_message;
                                     });
    const std::array<float, 16> projection           = {1.0f, 0.0f, 0.0f, 0.0f,
                                               0.0f, 1.0f, 0.0f, 0.0f,
                                               0.0f, 0.0f, 1.0f, 1.0f,
                                               0.0f, 0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(core.on_matrix_call(SET_TRANSFORM,
                                    projection_arguments,
                                    projection) );

    EXPECT_FALSE(core.get_is_overlay_pushed() );

    /* The recipe has an amount, and nobody has asked: nothing is moved. */
    const auto left = bind_matrices(&core,
                                    Analysis::Recipe::DrawClass::UNAFFECTED,
                                    Analysis::Recipe::Eye::LEFT,
                                    true); /* in_is_final_target */

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    0.0f);

    /* Asked for, while the program plays: pushed by what the recipe says; and not asked for any longer: put back. */
    core.set_is_overlay_pushed(true);

    const auto pushed = bind_matrices(&core,
                                      Analysis::Recipe::DrawClass::UNAFFECTED,
                                      Analysis::Recipe::Eye::LEFT,
                                      true); /* in_is_final_target */

    ASSERT_EQ(pushed.size(),
              1u);
    EXPECT_FLOAT_EQ(pushed.at(0).matrix_fp32vec16.at(8),
                    0.04f);

    core.set_is_overlay_pushed(false);

    const auto back = bind_matrices(&core,
                                    Analysis::Recipe::DrawClass::UNAFFECTED,
                                    Analysis::Recipe::Eye::LEFT,
                                    true); /* in_is_final_target */

    ASSERT_EQ(back.size(),
              1u);
    EXPECT_FLOAT_EQ(back.at(0).matrix_fp32vec16.at(8),
                    0.0f);
}

TEST_F(StereoCoreTest, ADrawThatCoversTheWholeOfTheViewIsNotPushedWhateverThePersonAsks)
{
    const auto projection_arguments = make_set_transform_arguments(STATE_PROJECTION);
    auto       recipe               = make_recipe("\"comfort\": {\"overlay_parallax\": 0.04}," + get_baked_text(""),
                                                "{\"call\": \"IDirect3DDevice8::SetTransform\", \"when\": {\"state\": \"D3DTS_PROJECTION\","
                                                " \"matrix\": \"perspective\"}, \"action\": {\"type\": \"eye_convergence\"}}");

    recipe.covering_draw_key_vec.push_back(7);

    Stereo::Core::StereoCore    core(recipe,
                                     [](const std::string& in_message)
                                     {
                                         (void) in_message;
                                     });
    const std::array<float, 16> projection = {1.0f, 0.0f, 0.0f, 0.0f,
                                               0.0f, 1.0f, 0.0f, 0.0f,
                                               0.0f, 0.0f, 1.0f, 1.0f,
                                               0.0f, 0.0f, 0.0f, 0.0f};

    ASSERT_TRUE(core.on_matrix_call(SET_TRANSFORM,
                                    projection_arguments,
                                    projection) );

    core.set_is_overlay_pushed(true);

    /* A draw of another size is pushed. */
    core.classify_draw(1, 0);

    auto left = bind_matrices(&core,
                              Analysis::Recipe::DrawClass::UNAFFECTED,
                              Analysis::Recipe::Eye::LEFT,
                              true); /* in_is_final_target */

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    0.04f);

    /* One that covers the view keeps the picture whole: the application's matrix. */
    core.classify_draw(1, 7);

    left = bind_matrices(&core,
                         Analysis::Recipe::DrawClass::UNAFFECTED,
                         Analysis::Recipe::Eye::LEFT,
                         true); /* in_is_final_target */

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    0.0f);

    /* And the next draw, which does not, is pushed again. */
    core.classify_draw(1, 0);

    left = bind_matrices(&core,
                         Analysis::Recipe::DrawClass::UNAFFECTED,
                         Analysis::Recipe::Eye::LEFT,
                         true); /* in_is_final_target */

    ASSERT_EQ(left.size(),
              1u);
    EXPECT_FLOAT_EQ(left.at(0).matrix_fp32vec16.at(8),
                    0.04f);
}

TEST_F(StereoCoreTest, TheEyeShiftMatrixMovesTheCameraHalfTheSeparationToEachSide)
{
    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              ""),
                                  Stereo::Core::FailFunc() );

    core.set_eye_separation(0.5f);

    const auto left  = core.make_eye_shift_matrix(Analysis::Recipe::Eye::LEFT);
    const auto right = core.make_eye_shift_matrix(Analysis::Recipe::Eye::RIGHT);

    /* The scene goes toward the right for the left eye: the camera is to its left. */
    EXPECT_FLOAT_EQ(left.at(12),
                    0.25f);
    EXPECT_FLOAT_EQ(right.at(12),
                    -0.25f);

    /* With no distance of convergence there is no shear, and nothing else is changed. */
    EXPECT_FLOAT_EQ(left.at(8),
                    0.0f);
    EXPECT_FLOAT_EQ(left.at(0),
                    1.0f);
    EXPECT_FLOAT_EQ(left.at(5),
                    1.0f);
    EXPECT_FLOAT_EQ(left.at(10),
                    1.0f);
    EXPECT_FLOAT_EQ(left.at(15),
                    1.0f);
}

TEST_F(StereoCoreTest, TheEyeShiftMatrixShearsTheSceneByTheDepthSoWhatIsAtTheDistanceStaysPut)
{
    Stereo::Core::StereoCore core(make_recipe(get_baked_text(""),
                                              ""),
                                  Stereo::Core::FailFunc() );

    core.set_convergence_distance(10.0f);
    core.set_eye_separation      (0.5f);

    const auto left  = core.make_eye_shift_matrix(Analysis::Recipe::Eye::LEFT);
    const auto right = core.make_eye_shift_matrix(Analysis::Recipe::Eye::RIGHT);

    /* A point in front of the camera is at z = -d: x + h + (h / d) * z is x there, for both eyes. */
    EXPECT_FLOAT_EQ(left.at(12) + left.at(8) * -10.0f,
                    0.0f);
    EXPECT_FLOAT_EQ(right.at(12) + right.at(8) * -10.0f,
                    0.0f);
    EXPECT_FLOAT_EQ(left.at(8),
                    0.025f);
    EXPECT_FLOAT_EQ(right.at(8),
                    -0.025f);
}

TEST_F(StereoCoreTest, TheCoreSaysWhetherTheRecipeHasThePixelReadsCached)
{
    Stereo::Core::StereoCore cached_core(make_recipe("\"cache_pixel_reads\": true," + get_baked_text(""),
                                                     ""),
                                         Stereo::Core::FailFunc() );
    Stereo::Core::StereoCore plain_core (make_recipe(get_baked_text(""),
                                                     ""),
                                         Stereo::Core::FailFunc() );

    EXPECT_TRUE (cached_core.get_caches_pixel_reads() );
    EXPECT_FALSE(plain_core.get_caches_pixel_reads() );
}

TEST_F(StereoCoreTest, TheDepthThatWasMeasuredSetsWhereTheEyesConvergeAndHowFarApartTheyMayBe)
{
    Stereo::Core::StereoCore core(make_recipe("\"comfort\": {\"balance_convergence\": true, \"max_far_parallax\": 0.01, \"max_near_parallax\": 0.015, \"measure_depth\": true},"
                                              + get_baked_text(""),
                                              ""),
                                  Stereo::Core::FailFunc() );
    Stereo::Core::DepthRange range;

    EXPECT_TRUE(core.get_measures_depth() );

    core.set_depth_strength(1.0f);

    /* Nothing in the range: nothing is taken from it. */
    EXPECT_FALSE(core.on_frame_depth(1,
                                     range,
                                     1.0f) );
    EXPECT_FLOAT_EQ(core.get_effective_convergence_distance(),
                    0.0f);

    range.far_depth    = 1000.0f;
    range.has_depth    = true;
    range.median_depth = 400.0f;
    range.near_depth   = 100.0f;

    ASSERT_TRUE(core.on_frame_depth(1,
                                    range,
                                    1.0f) );

    /* Between the nearest surface and the farthest, and with the nearest the one that has the near limit: 1 / d = 1 / 100 - (1 / 100 - 1 / 1000) * 0.015 / 0.025. */
    const float distance = core.get_effective_convergence_distance();

    EXPECT_GT(distance,
              100.0f);
    EXPECT_LT(distance,
              1000.0f);
    EXPECT_NEAR(distance,
                1.0f / (0.01f - 0.009f * 0.6f),
                1.0f);

    /* The separation is within the limits that the two ends of the depth give: the nearest surface has no more than 0.015 of the width of the picture. */
    const float separation  = core.get_effective_eye_separation();
    const float near_parallax = separation * (1.0f / range.near_depth - 1.0f / distance) * 0.5f;

    EXPECT_GT(separation,
              0.0f);
    EXPECT_LE(near_parallax,
              0.015f * 1.01f);
}

TEST_F(StereoCoreTest, WhatTheRecipeSaysOfAKindOfFrameWinsOverTheDepthThatWasMeasured)
{
    Stereo::Core::StereoCore core(make_recipe("\"comfort\": {\"max_near_parallax\": 0.015, \"measure_depth\": true, \"scene_overrides\": [{\"shape\": \"0x0000000000000007\", \"convergence_distance\": 250}]},"
                                              + get_baked_text(""),
                                              ""),
                                  Stereo::Core::FailFunc() );
    Stereo::Core::DepthRange range;

    range.far_depth    = 1000.0f;
    range.has_depth    = true;
    range.median_depth = 400.0f;
    range.near_depth   = 100.0f;

    core.set_depth_strength(1.0f);

    ASSERT_TRUE(core.on_frame_depth(7,
                                    range,
                                    1.0f) );
    EXPECT_FLOAT_EQ(core.get_effective_convergence_distance(),
                    250.0f);
}
