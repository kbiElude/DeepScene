/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for RecipeApplier: which rules are about a call, and what a rule does to a matrix.
 */
#include <gtest/gtest.h>
#include "analysis/recipe/recipe_applier.h"
#include "interception/hooks/D3D8/types.h"

class RecipeApplierTest : public ::testing::Test
{
protected:
    static constexpr APIInterceptor::APIFunction SET_TRANSFORM = APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM;

    static std::array<float, 16> make_identity()
    {
        return {1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1};
    }

    static std::array<float, 16> make_scale(const float& in_factor)
    {
        return {in_factor, 0,         0,         0,
                0,         in_factor, 0,         0,
                0,         0,         in_factor, 0,
                0,         0,         0,         1};
    }

    static std::array<float, 16> make_translation(const float& in_x)
    {
        return {1,    0, 0, 0,
                0,    1, 0, 0,
                0,    0, 1, 0,
                in_x, 0, 0, 1};
    }

    /* A rule for SetTransform with the state D3DTS_VIEW. */
    static Analysis::Recipe::MatrixRule make_view_rule()
    {
        Analysis::Recipe::MatrixRule        rule;
        Analysis::Recipe::ArgumentCondition condition;

        condition.argument_index = 1;
        condition.value          = Interception::Hooks::D3D8::D3DTS_VIEW;

        rule.argument_condition_vec.push_back(condition);

        rule.function              = SET_TRANSFORM;
        rule.left_matrix_fp32vec16 = make_translation(1.0f);
        rule.matrix_argument_index = 2;
        rule.right_matrix_fp32vec16 = make_translation(-1.0f);

        return rule;
    }

    static Capture::APIDumpLoader::DumpedAPICall make_set_transform(const uint32_t& in_state)
    {
        Capture::APIDumpLoader::DumpedAPICall call;

        call.func = SET_TRANSFORM;

        call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr) );
        call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(in_state) );
        call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_data_chunk_id(1) );

        return call;
    }

    static std::vector<const Analysis::Recipe::MatrixRule*> match(const Analysis::Recipe::Recipe&      in_recipe,
                                                                  const Capture::APIDumpLoader::DumpedAPICall&     in_call,
                                                                  const Analysis::Recipe::CallPlace&  in_place)
    {
        const Analysis::Recipe::RecipeApplier applier(in_recipe);

        return applier.get_matching_rule_ptr_vec(in_call,
                                                 in_place);
    }
};

TEST_F(RecipeApplierTest, ARuleIsAboutTheFunctionAndTheArgumentsItNames)
{
    Analysis::Recipe::Recipe recipe;

    recipe.rule_vec.push_back(make_view_rule() );

    EXPECT_EQ(match(recipe,
                    make_set_transform(Interception::Hooks::D3D8::D3DTS_VIEW),
                    {}).size(),
              1u);
    EXPECT_EQ(match(recipe,
                    make_set_transform(Interception::Hooks::D3D8::D3DTS_PROJECTION),
                    {}).size(),
              0u);

    auto other_call = make_set_transform(Interception::Hooks::D3D8::D3DTS_VIEW);

    other_call.func = APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETMATERIAL;

    EXPECT_EQ(match(recipe,
                    other_call,
                    {}).size(),
              0u);
}

TEST_F(RecipeApplierTest, AConditionOnAnArgumentThatIsNotThereOrOfAnotherTypeIsNotMet)
{
    Analysis::Recipe::Recipe recipe;
    auto                     rule   = make_view_rule();

    rule.argument_condition_vec.at(0).argument_index = 7;

    recipe.rule_vec.push_back(rule);

    EXPECT_TRUE(match(recipe,
                      make_set_transform(Interception::Hooks::D3D8::D3DTS_VIEW),
                      {}).empty() );

    recipe.rule_vec.at(0).argument_condition_vec.at(0).argument_index = 2; /* a chunk, not a number */

    EXPECT_TRUE(match(recipe,
                      make_set_transform(Interception::Hooks::D3D8::D3DTS_VIEW),
                      {}).empty() );
}

TEST_F(RecipeApplierTest, OnlyTheFunctionsWithARuleHaveARule)
{
    Analysis::Recipe::Recipe recipe;

    recipe.rule_vec.push_back(make_view_rule() );

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_TRUE (applier.has_rule_for(SET_TRANSFORM) );
    EXPECT_FALSE(applier.has_rule_for(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETMATERIAL) );
}

TEST_F(RecipeApplierTest, ARuleWithNoConditionsIsAboutEveryCallOfItsFunction)
{
    Analysis::Recipe::Recipe recipe;
    auto                     rule   = make_view_rule();

    rule.argument_condition_vec.clear();

    recipe.rule_vec.push_back(rule);

    EXPECT_EQ(match(recipe,
                    make_set_transform(Interception::Hooks::D3D8::D3DTS_PROJECTION),
                    {}).size(),
              1u);
}

TEST_F(RecipeApplierTest, ARuleWithPassKeysIsAboutTheCallsOfThosePasses)
{
    Analysis::Recipe::Recipe recipe;
    auto                     rule   = make_view_rule();

    rule.pass_key_vec = {0x11, 0x22};

    recipe.rule_vec.push_back(rule);

    Analysis::Recipe::CallPlace in_pass;
    Analysis::Recipe::CallPlace in_other_pass;
    Analysis::Recipe::CallPlace in_no_pass;

    in_pass.has_pass       = true;
    in_pass.pass_key       = 0x22;
    in_other_pass.has_pass = true;
    in_other_pass.pass_key = 0x33;

    const auto call = make_set_transform(Interception::Hooks::D3D8::D3DTS_VIEW);

    EXPECT_EQ(match(recipe, call, in_pass).size(),
              1u);
    EXPECT_EQ(match(recipe, call, in_other_pass).size(),
              0u);
    EXPECT_EQ(match(recipe, call, in_no_pass).size(),
              0u);
}

TEST_F(RecipeApplierTest, ARuleWithShapeKeysIsAboutTheCallsOfThoseFrames)
{
    Analysis::Recipe::Recipe recipe;
    auto                     rule   = make_view_rule();

    rule.shape_key_vec = {0xaa};

    recipe.rule_vec.push_back(rule);

    Analysis::Recipe::CallPlace known_frame;
    Analysis::Recipe::CallPlace other_frame;

    known_frame.shape_key = 0xaa;
    other_frame.shape_key = 0xbb;

    const auto call = make_set_transform(Interception::Hooks::D3D8::D3DTS_VIEW);

    EXPECT_EQ(match(recipe, call, known_frame).size(),
              1u);
    EXPECT_EQ(match(recipe, call, other_frame).size(),
              0u);
}

TEST_F(RecipeApplierTest, BothKindsOfKeysMustMatchWhenThereAreBoth)
{
    Analysis::Recipe::Recipe recipe;
    auto                     rule   = make_view_rule();

    rule.pass_key_vec  = {0x11};
    rule.shape_key_vec = {0xaa};

    recipe.rule_vec.push_back(rule);

    Analysis::Recipe::CallPlace place;

    place.has_pass  = true;
    place.pass_key  = 0x11;
    place.shape_key = 0xaa;

    const auto call = make_set_transform(Interception::Hooks::D3D8::D3DTS_VIEW);

    EXPECT_EQ(match(recipe, call, place).size(),
              1u);

    place.shape_key = 0xbb;

    EXPECT_EQ(match(recipe, call, place).size(),
              0u);
}

TEST_F(RecipeApplierTest, TheRulesThatMatchComeInTheOrderOfTheRecipe)
{
    Analysis::Recipe::Recipe recipe;
    auto                     first  = make_view_rule();
    auto                     second = make_view_rule();

    first.name  = "first";
    second.name = "second";

    recipe.rule_vec.push_back(first);
    recipe.rule_vec.push_back(second);

    const Analysis::Recipe::RecipeApplier applier(recipe);
    const auto                                rule_ptr_vec = applier.get_matching_rule_ptr_vec(make_set_transform(Interception::Hooks::D3D8::D3DTS_VIEW),
                                                                                               {});

    ASSERT_EQ(rule_ptr_vec.size(),
              2u);
    EXPECT_EQ(rule_ptr_vec.at(0)->name,
              "first");
    EXPECT_EQ(rule_ptr_vec.at(1)->name,
              "second");
}

TEST_F(RecipeApplierTest, AProductWithTheRecipesMatrixAfterOrBeforeTheCallsIsNotTheSame)
{
    Analysis::Recipe::MatrixRule rule = make_view_rule();

    rule.action_kind              = Analysis::Recipe::ActionKind::MULTIPLY;
    rule.left_matrix_fp32vec16    = make_scale(2.0f);
    rule.right_matrix_fp32vec16   = make_scale(2.0f);

    auto matrix = make_translation(1.0f);

    /* Row vectors: M * X scales the translation too, X * M leaves it. */
    rule.multiply_order = Analysis::Recipe::MultiplyOrder::POST;

    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            0.0f,
                                                            &matrix) );
    EXPECT_FLOAT_EQ(matrix.at(12),
                    2.0f);
    EXPECT_FLOAT_EQ(matrix.at(0),
                    2.0f);

    matrix = make_translation(1.0f);

    rule.multiply_order = Analysis::Recipe::MultiplyOrder::PRE;

    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            0.0f,
                                                            &matrix) );
    EXPECT_FLOAT_EQ(matrix.at(12),
                    1.0f);
    EXPECT_FLOAT_EQ(matrix.at(0),
                    2.0f);
}

TEST_F(RecipeApplierTest, EachEyeGetsItsOwnMatrix)
{
    Analysis::Recipe::MatrixRule rule = make_view_rule();

    rule.action_kind    = Analysis::Recipe::ActionKind::MULTIPLY;
    rule.multiply_order = Analysis::Recipe::MultiplyOrder::POST;

    auto left  = make_identity();
    auto right = make_identity();

    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            0.0f,
                                                            &left) );
    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::RIGHT,
                                                            0.0f,
                                                            &right) );

    EXPECT_FLOAT_EQ(left.at(12),
                    1.0f);
    EXPECT_FLOAT_EQ(right.at(12),
                    -1.0f);
}

TEST_F(RecipeApplierTest, ReplacingPutsTheRecipesMatrixInPlaceOfTheCalls)
{
    Analysis::Recipe::MatrixRule rule = make_view_rule();

    rule.action_kind = Analysis::Recipe::ActionKind::REPLACE;

    auto matrix = make_scale(5.0f);

    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::RIGHT,
                                                            0.0f,
                                                            &matrix) );
    EXPECT_EQ(matrix,
              make_translation(-1.0f) );
}

TEST_F(RecipeApplierTest, ARuleAboutPerspectiveOrOrthographicMatricesLeavesTheOthersAlone)
{
    Analysis::Recipe::MatrixRule rule = make_view_rule();

    rule.action_kind = Analysis::Recipe::ActionKind::REPLACE;

    /* D3D perspective: the last column is (0, 0, 1, 0); orthographic: (0, 0, 0, 1). */
    std::array<float, 16> perspective  = make_identity();
    std::array<float, 16> orthographic = make_identity();

    perspective.at(11)  = 1.0f;
    perspective.at(15)  = 0.0f;

    rule.matrix_kind = Analysis::Recipe::MatrixKind::PERSPECTIVE;

    auto matrix = perspective;

    EXPECT_TRUE (Analysis::Recipe::RecipeApplier::apply_rule(rule, Analysis::Recipe::Eye::LEFT, 0.0f, &matrix) );

    matrix = orthographic;

    EXPECT_FALSE(Analysis::Recipe::RecipeApplier::apply_rule(rule, Analysis::Recipe::Eye::LEFT, 0.0f, &matrix) );
    EXPECT_EQ   (matrix,
                 orthographic);

    rule.matrix_kind = Analysis::Recipe::MatrixKind::ORTHOGRAPHIC;

    EXPECT_TRUE (Analysis::Recipe::RecipeApplier::apply_rule(rule, Analysis::Recipe::Eye::LEFT, 0.0f, &matrix) );

    matrix = perspective;

    EXPECT_FALSE(Analysis::Recipe::RecipeApplier::apply_rule(rule, Analysis::Recipe::Eye::LEFT, 0.0f, &matrix) );
    EXPECT_EQ   (matrix,
                 perspective);

    rule.matrix_kind = Analysis::Recipe::MatrixKind::ANY;

    EXPECT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule, Analysis::Recipe::Eye::LEFT, 0.0f, &matrix) );
}

TEST_F(RecipeApplierTest, AFrameIsKnownIfItsShapeIsListedOrNoShapeIs)
{
    Analysis::Recipe::Recipe recipe;

    EXPECT_TRUE(Analysis::Recipe::RecipeApplier(recipe).is_frame_known(0x1234) );

    recipe.known_shape_key_vec = {0xaa, 0xbb};

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_TRUE (applier.is_frame_known(0xbb) );
    EXPECT_FALSE(applier.is_frame_known(0xcc) );
}

TEST_F(RecipeApplierTest, ThePolicyForUnknownFramesIsThatOfTheRecipe)
{
    Analysis::Recipe::Recipe recipe;

    recipe.unknown_frame_policy = Analysis::Recipe::UnknownFramePolicy::SKIP;

    EXPECT_EQ(Analysis::Recipe::RecipeApplier(recipe).get_unknown_frame_policy(),
              Analysis::Recipe::UnknownFramePolicy::SKIP);
}

TEST_F(RecipeApplierTest, EveryDrawIsAffectedUnlessARecipeNamesItOrItsPass)
{
    Analysis::Recipe::Recipe recipe;

    {
        const Analysis::Recipe::RecipeApplier applier(recipe);

        EXPECT_TRUE(applier.is_draw_affected(1,
                                             2) );
    }

    recipe.unaffected.pass_key_vec = {10};
    recipe.unaffected.draw_key_vec = {20};

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_FALSE(applier.is_draw_affected(10,
                                          1) );
    EXPECT_FALSE(applier.is_draw_affected(1,
                                          20) );
    EXPECT_TRUE (applier.is_draw_affected(1,
                                          1) );
}

TEST_F(RecipeApplierTest, ADrawKeyBeatsAPassKeyAndAnAffectedKeyBeatsNothing)
{
    Analysis::Recipe::Recipe recipe;

    recipe.unaffected.pass_key_vec = {10, 11};
    recipe.unaffected.draw_key_vec = {20};
    recipe.affected.draw_key_vec   = {30};
    recipe.affected.pass_key_vec   = {11};

    const Analysis::Recipe::RecipeApplier applier(recipe);

    /* A draw named affected, inside an unaffected pass. */
    EXPECT_TRUE (applier.is_draw_affected(10,
                                          30) );
    /* A draw named unaffected, inside a pass which is affected. */
    EXPECT_FALSE(applier.is_draw_affected(11,
                                          20) );
    /* Neither named: the pass says. */
    EXPECT_FALSE(applier.is_draw_affected(10,
                                          1) );
    EXPECT_TRUE (applier.is_draw_affected(11,
                                          1) );
}

TEST_F(RecipeApplierTest, ADrawIsSkippedIfItOrItsPassIsNamed)
{
    Analysis::Recipe::Recipe recipe;

    {
        const Analysis::Recipe::RecipeApplier applier(recipe);

        EXPECT_FALSE(applier.is_draw_skipped(1,
                                             2) );
    }

    recipe.skipped.pass_key_vec = {10};
    recipe.skipped.draw_key_vec = {20};

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_TRUE (applier.is_draw_skipped(10,
                                         1) );
    EXPECT_TRUE (applier.is_draw_skipped(1,
                                         20) );
    EXPECT_FALSE(applier.is_draw_skipped(1,
                                         1) );
}

TEST_F(RecipeApplierTest, ADrawIsPushedIfItsKeyItsPassOrItInItsPassIsNamed)
{
    Analysis::Recipe::Recipe recipe;

    {
        const Analysis::Recipe::RecipeApplier applier(recipe);

        EXPECT_FALSE(applier.is_draw_pushed(1,
                                            2) );
    }

    recipe.pushed.pass_key_vec = {10};
    recipe.pushed.draw_key_vec = {20};
    recipe.pushed.pass_draw_vec.push_back({30, 40});

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_TRUE (applier.is_draw_pushed(10,
                                        1) );
    EXPECT_TRUE (applier.is_draw_pushed(1,
                                        20) );
    EXPECT_TRUE (applier.is_draw_pushed(40,
                                        30) );
    EXPECT_FALSE(applier.is_draw_pushed(30,
                                        40) );
    EXPECT_FALSE(applier.is_draw_pushed(1,
                                        1) );
}

TEST_F(RecipeApplierTest, ADrawIsPushedByItsSizeIfItsCoverKeyIsNamed)
{
    Analysis::Recipe::Recipe recipe;

    recipe.pushed.cover_key_vec = {50};

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_TRUE (applier.is_cover_pushed(50) );
    EXPECT_FALSE(applier.is_cover_pushed(51) );
    EXPECT_FALSE(applier.is_draw_pushed(1,
                                        50) );
}

TEST_F(RecipeApplierTest, WithAPassADrawIsDecidedByTheNamesOfItsPassAndItsStructure)
{
    Analysis::Recipe::DrawQuery query;
    Analysis::Recipe::Recipe    recipe;

    recipe.pushed.draw_key_vec     = {30};
    recipe.skipped.draw_key_vec    = {20};
    recipe.unaffected.pass_key_vec = {10};

    const Analysis::Recipe::RecipeApplier applier(recipe);

    query.has_pass = true;

    query.draw_key = 1;
    query.pass_key = 10;
    EXPECT_EQ(applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::UNAFFECTED);

    query.draw_key = 20;
    query.pass_key = 1;
    EXPECT_EQ(applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::SKIPPED);

    query.draw_key = 30;
    query.pass_key = 1;
    EXPECT_EQ(applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::AFFECTED);
    EXPECT_TRUE(applier.decide_draw(query).is_pushed);

    query.draw_key = 1;
    EXPECT_EQ(applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::AFFECTED);
    EXPECT_FALSE(applier.decide_draw(query).is_pushed);
}

TEST_F(RecipeApplierTest, WithoutAPassADrawIsDecidedByItsSignatureAndItsViewMatrix)
{
    Analysis::Recipe::DrawRule  pushed_rule;
    Analysis::Recipe::DrawQuery query;
    Analysis::Recipe::Recipe    recipe;
    Analysis::Recipe::DrawRule  skipped_rule;

    pushed_rule.draw_class    = Analysis::Recipe::DrawClass::UNAFFECTED;
    pushed_rule.is_pushed     = true;
    pushed_rule.signature_key = 7;

    skipped_rule.draw_class    = Analysis::Recipe::DrawClass::SKIPPED;
    skipped_rule.signature_key = 8;

    recipe.draw_rule_vec.push_back(pushed_rule);
    recipe.draw_rule_vec.push_back(skipped_rule);

    const Analysis::Recipe::RecipeApplier applier(recipe);

    query.view_matrix = make_identity();

    query.signature_key = 7;
    EXPECT_EQ(applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_TRUE(applier.decide_draw(query).is_pushed);

    query.signature_key = 8;
    EXPECT_EQ(applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::SKIPPED);
    EXPECT_FALSE(applier.decide_draw(query).is_pushed);

    query.signature_key = 9;
    EXPECT_EQ(applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::AFFECTED);
}

TEST_F(RecipeApplierTest, ADrawPushedByItsCoverKeyIsLeftAloneUnlessTheCallerWaivesThat)
{
    Analysis::Recipe::DrawQuery query;
    Analysis::Recipe::Recipe    recipe;

    recipe.pushed.cover_key_vec = {50};

    const Analysis::Recipe::RecipeApplier applier(recipe);

    query.cover_key     = 50;
    query.draw_key      = 1;
    query.has_pass      = true;
    query.pass_key      = 2;
    query.signature_key = 3;
    query.view_matrix   = make_identity();

    /* With a pass or without, the draw is left alone and pushed. */
    EXPECT_EQ  (applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_TRUE(applier.decide_draw(query).is_pushed);

    query.has_pass = false;

    EXPECT_EQ  (applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_TRUE(applier.decide_draw(query).is_pushed);

    /* A caller that cannot leave the draw alone gets it affected, and still pushed. */
    query.is_cover_hold_waived = true;

    EXPECT_EQ  (applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::AFFECTED);
    EXPECT_TRUE(applier.decide_draw(query).is_pushed);
}

TEST_F(RecipeApplierTest, ABakedFlatLayerIsLeftAloneAndNotPushedEvenIfTheCallerWaivesTheCoverHold)
{
    Analysis::Recipe::DrawQuery query;
    Analysis::Recipe::Recipe    recipe;

    recipe.comfort.uses_automatic_layers = true;
    recipe.flat_layer_draw_key_vec       = {60};

    const Analysis::Recipe::RecipeApplier applier(recipe);

    query.cover_key            = 60;
    query.is_cover_hold_waived = true;
    query.view_matrix          = make_identity();

    EXPECT_EQ   (applier.decide_draw(query).draw_class, Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_FALSE(applier.decide_draw(query).is_pushed);
}

TEST_F(RecipeApplierTest, APartThatProtrudesIsPushedByThePartParallaxAndTheOthersByTheOverlayParallax)
{
    Analysis::Recipe::Recipe recipe;

    recipe.comfort.overlay_parallax    = 0.015f;
    recipe.comfort.part_parallax       = 0.004f;
    recipe.pushed.cover_key_vec        = {50};
    recipe.pushed.part_cover_key_vec   = {60};

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_TRUE(applier.is_cover_pushed(50) );
    EXPECT_TRUE(applier.is_cover_pushed(60) );
    EXPECT_FLOAT_EQ(applier.get_cover_push_parallax(50),
                    0.015f);
    EXPECT_FLOAT_EQ(applier.get_cover_push_parallax(60),
                    0.004f);
    EXPECT_FLOAT_EQ(applier.get_cover_push_parallax(70),
                    0.015f);

    /* A recipe with no part parallax pushes the parts as it pushes the rest. */
    recipe.comfort.part_parallax = 0.0f;

    const Analysis::Recipe::RecipeApplier same_applier(recipe);

    EXPECT_FLOAT_EQ(same_applier.get_cover_push_parallax(60),
                    0.015f);
}

TEST_F(RecipeApplierTest, AnEyeOffsetMovesTheEyesApartByHalfTheSeparationEachAlongTheAxis)
{
    struct AxisCase
    {
        Analysis::Recipe::Axis axis;
        size_t                 n_element;
    };

    const AxisCase case_array[] = {{Analysis::Recipe::Axis::X, 12},
                                   {Analysis::Recipe::Axis::Y, 13},
                                   {Analysis::Recipe::Axis::Z, 14}};

    for (const auto& axis_case : case_array)
    {
        Analysis::Recipe::MatrixRule rule;
        auto                         left  = make_identity();
        auto                         right = make_identity();

        rule.action_kind    = Analysis::Recipe::ActionKind::EYE_OFFSET;
        rule.multiply_order = Analysis::Recipe::MultiplyOrder::POST;
        rule.offset_axis    = axis_case.axis;

        ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                                Analysis::Recipe::Eye::LEFT,
                                                                0.2f,
                                                                &left) );
        ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                                Analysis::Recipe::Eye::RIGHT,
                                                                0.2f,
                                                                &right) );

        EXPECT_FLOAT_EQ(left.at(axis_case.n_element),
                        0.1f);
        EXPECT_FLOAT_EQ(right.at(axis_case.n_element),
                        -0.1f);
    }
}

TEST_F(RecipeApplierTest, AnEyeOffsetGrowsWithTheSeparationAndIsNothingWithoutIt)
{
    Analysis::Recipe::MatrixRule rule;

    rule.action_kind    = Analysis::Recipe::ActionKind::EYE_OFFSET;
    rule.multiply_order = Analysis::Recipe::MultiplyOrder::POST;

    auto narrow = make_identity();
    auto wider  = make_identity();
    auto none   = make_identity();

    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            0.2f,
                                                            &narrow) );
    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            0.4f,
                                                            &wider) );
    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            0.0f,
                                                            &none) );

    EXPECT_FLOAT_EQ(wider.at(12),
                    2.0f * narrow.at(12) );
    EXPECT_EQ      (none,
                    make_identity() );
}

TEST_F(RecipeApplierTest, AnEyeOffsetIsMultipliedInTheOrderTheRuleSays)
{
    /* A scale by 2: after it the offset is as it is, before it the offset is scaled too. */
    const std::array<float, 16> scale = {2, 0, 0, 0,
                                         0, 2, 0, 0,
                                         0, 0, 2, 0,
                                         0, 0, 0, 1};

    Analysis::Recipe::MatrixRule rule;
    auto                         post = scale;
    auto                         pre  = scale;

    rule.action_kind = Analysis::Recipe::ActionKind::EYE_OFFSET;
    rule.offset_axis = Analysis::Recipe::Axis::X;

    rule.multiply_order = Analysis::Recipe::MultiplyOrder::POST;

    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            1.0f,
                                                            &post) );

    rule.multiply_order = Analysis::Recipe::MultiplyOrder::PRE;

    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            1.0f,
                                                            &pre) );

    EXPECT_FLOAT_EQ(post.at(12),
                    0.5f);
    EXPECT_FLOAT_EQ(pre.at(12),
                    0.5f * 2.0f);
}

TEST_F(RecipeApplierTest, AnEyeOffsetOfAnotherKindOfMatrixIsLeftAlone)
{
    Analysis::Recipe::MatrixRule rule;
    auto                         matrix = make_identity();

    rule.action_kind    = Analysis::Recipe::ActionKind::EYE_OFFSET;
    rule.matrix_kind    = Analysis::Recipe::MatrixKind::PERSPECTIVE;
    rule.multiply_order = Analysis::Recipe::MultiplyOrder::POST;

    /* An identity is not a perspective projection. */
    EXPECT_FALSE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                             Analysis::Recipe::Eye::LEFT,
                                                             1.0f,
                                                             &matrix) );
    EXPECT_EQ   (matrix,
                 make_identity() );
}

TEST_F(RecipeApplierTest, ADrawIsAffectedUnlessARuleSaysOtherwise)
{
    Analysis::Recipe::DrawRule rule;
    Analysis::Recipe::Recipe   recipe;

    rule.draw_class    = Analysis::Recipe::DrawClass::UNAFFECTED;
    rule.signature_key = 7;

    recipe.draw_rule_vec.push_back(rule);

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_EQ(applier.classify_draw(7,
                                    make_identity() ),
              Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_EQ(applier.classify_draw(8,
                                    make_identity() ),
              Analysis::Recipe::DrawClass::AFFECTED);
}

TEST_F(RecipeApplierTest, ADrawRuleWithMatricesAppliesWhileTheViewMatrixIsOneOfThem)
{
    Analysis::Recipe::Recipe   recipe;
    Analysis::Recipe::DrawRule skipped_rule;
    Analysis::Recipe::DrawRule unaffected_rule;

    unaffected_rule.draw_class    = Analysis::Recipe::DrawClass::UNAFFECTED;
    unaffected_rule.signature_key = 7;
    unaffected_rule.view_matrix_vec.push_back(make_translation(2.5f) );
    unaffected_rule.view_matrix_vec.push_back(make_translation(3.0f) );

    skipped_rule.draw_class    = Analysis::Recipe::DrawClass::SKIPPED;
    skipped_rule.signature_key = 7;
    skipped_rule.view_matrix_vec.push_back(make_translation(9.0f) );

    recipe.draw_rule_vec.push_back(unaffected_rule);
    recipe.draw_rule_vec.push_back(skipped_rule);

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_EQ(applier.classify_draw(7,
                                    make_translation(2.5f) ),
              Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_EQ(applier.classify_draw(7,
                                    make_translation(3.0f) ),
              Analysis::Recipe::DrawClass::UNAFFECTED);
    EXPECT_EQ(applier.classify_draw(7,
                                    make_translation(9.0f) ),
              Analysis::Recipe::DrawClass::SKIPPED);

    /* No rule applies to another camera, so the draw is affected. */
    EXPECT_EQ(applier.classify_draw(7,
                                    make_translation(4.0f) ),
              Analysis::Recipe::DrawClass::AFFECTED);
    EXPECT_EQ(applier.classify_draw(7,
                                    make_identity() ),
              Analysis::Recipe::DrawClass::AFFECTED);
}

TEST_F(RecipeApplierTest, ASignatureIsPushedIfTheRuleThatAppliesSaysSo)
{
    Analysis::Recipe::DrawRule plain_rule;
    Analysis::Recipe::DrawRule pushed_rule;
    Analysis::Recipe::Recipe   recipe;

    pushed_rule.draw_class    = Analysis::Recipe::DrawClass::AFFECTED;
    pushed_rule.is_pushed     = true;
    pushed_rule.signature_key = 7;
    pushed_rule.view_matrix_vec.push_back(make_translation(2.5f) );

    plain_rule.draw_class    = Analysis::Recipe::DrawClass::UNAFFECTED;
    plain_rule.signature_key = 7;

    recipe.draw_rule_vec.push_back(pushed_rule);
    recipe.draw_rule_vec.push_back(plain_rule);

    const Analysis::Recipe::RecipeApplier applier(recipe);

    /* The first rule that applies decides: with the camera of the pushed rule it is pushed and affected, with any other it is the plain one's. */
    EXPECT_TRUE (applier.is_signature_pushed(7,
                                             make_translation(2.5f) ) );
    EXPECT_EQ   (applier.classify_draw      (7,
                                             make_translation(2.5f) ),
                 Analysis::Recipe::DrawClass::AFFECTED);
    EXPECT_FALSE(applier.is_signature_pushed(7,
                                             make_identity() ) );
    EXPECT_FALSE(applier.is_signature_pushed(8,
                                             make_translation(2.5f) ) );
}

TEST_F(RecipeApplierTest, ASignatureIsKnownIfTheRecipeListsItOrListsNone)
{
    Analysis::Recipe::Recipe recipe;

    EXPECT_TRUE(Analysis::Recipe::RecipeApplier(recipe).is_signature_known(5) );

    recipe.known_signature_key_vec.push_back(7);

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_TRUE (applier.is_signature_known(7) );
    EXPECT_FALSE(applier.is_signature_known(5) );
}

TEST_F(RecipeApplierTest, AnEyeConvergenceShearsAPerspectiveProjectionSoThatWhatIsAtTheDistanceIsAtTheSamePlaceInBothEyes)
{
    Analysis::Recipe::MatrixRule rule;
    auto                         left = make_identity();

    rule.action_kind = Analysis::Recipe::ActionKind::EYE_CONVERGENCE;
    rule.matrix_kind = Analysis::Recipe::MatrixKind::PERSPECTIVE;

    left.at(0)  = 2.0f;
    left.at(11) = 1.0f;
    left.at(15) = 0.0f;

    auto right = left;

    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            0.2f,
                                                            &left,
                                                            5.0f) );
    ASSERT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::RIGHT,
                                                            0.2f,
                                                            &right,
                                                            5.0f) );

    /* Row 2, column 0: the view of the left eye moves a point to the right by half the separation (see make_eye_offset_matrix); a
     * point 5 away and straight ahead is then at 2 * 0.1 + 5 * left[8] = 0 in clip space.
     */
    EXPECT_FLOAT_EQ(left.at(8),
                    -0.04f);
    EXPECT_FLOAT_EQ(right.at(8),
                    0.04f);
    EXPECT_FLOAT_EQ(left.at(0),
                    2.0f);
}

TEST_F(RecipeApplierTest, AnEyeConvergenceDoesNothingWithoutADistanceOrASeparationAndIsOnlyForAPerspectiveProjection)
{
    Analysis::Recipe::MatrixRule rule;
    auto                         perspective = make_identity();

    rule.action_kind = Analysis::Recipe::ActionKind::EYE_CONVERGENCE;

    perspective.at(11) = 1.0f;
    perspective.at(15) = 0.0f;

    auto matrix = perspective;

    /* It is a rule about the matrix, though it changes nothing: the converter keeps it as the one of the eye. */
    EXPECT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            0.2f,
                                                            &matrix) );
    EXPECT_EQ  (matrix,
                perspective);
    EXPECT_TRUE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                            Analysis::Recipe::Eye::LEFT,
                                                            0.0f,
                                                            &matrix,
                                                            5.0f) );
    EXPECT_EQ  (matrix,
                perspective);

    matrix = make_identity();

    EXPECT_FALSE(Analysis::Recipe::RecipeApplier::apply_rule(rule,
                                                             Analysis::Recipe::Eye::LEFT,
                                                             0.2f,
                                                             &matrix,
                                                             5.0f) );
    EXPECT_EQ   (matrix,
                 make_identity() );
}

TEST_F(RecipeApplierTest, TheConvergenceDistanceOfASignatureIsTheRecipesAndZeroForOneItDoesNotHave)
{
    Analysis::Recipe::Recipe recipe;

    recipe.convergence_vec.push_back({12.5f, 7});

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_FLOAT_EQ(applier.get_convergence_distance(7),
                    12.5f);
    EXPECT_FLOAT_EQ(applier.get_convergence_distance(8),
                    0.0f);
}

TEST_F(RecipeApplierTest, TheConvergenceDistanceOfAShapeIsTheRecipesAndZeroForOneItDoesNotHave)
{
    Analysis::Recipe::Recipe recipe;

    recipe.scene_convergence_vec.push_back({3.5f, 11});

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_FLOAT_EQ(applier.get_scene_convergence_distance(11),
                    3.5f);
    EXPECT_FLOAT_EQ(applier.get_scene_convergence_distance(12),
                    0.0f);
}

TEST_F(RecipeApplierTest, TheLimitOfTheSeparationOfAShapeIsTheRecipesAndZeroForOneItDoesNotHave)
{
    Analysis::Recipe::Recipe recipe;

    recipe.scene_separation_vec.push_back({0.25f, 11});

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_FLOAT_EQ(applier.get_scene_max_separation(11),
                    0.25f);
    EXPECT_FLOAT_EQ(applier.get_scene_max_separation(12),
                    0.0f);
}

TEST_F(RecipeApplierTest, AnOverlayIsPushedOnlyByTheRuleOfAProjectionAndOnlyInAPerspectiveMatrix)
{
    Analysis::Recipe::MatrixRule convergence_rule;
    Analysis::Recipe::MatrixRule offset_rule;

    convergence_rule.action_kind = Analysis::Recipe::ActionKind::EYE_CONVERGENCE;
    offset_rule.action_kind      = Analysis::Recipe::ActionKind::EYE_OFFSET;

    std::array<float, 16> perspective  = {1.0f, 0.0f, 0.0f, 0.0f,
                                          0.0f, 1.0f, 0.0f, 0.0f,
                                          0.0f, 0.0f, 1.0f, 1.0f,
                                          0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 16> orthographic = {1.0f, 0.0f, 0.0f, 0.0f,
                                          0.0f, 1.0f, 0.0f, 0.0f,
                                          0.0f, 0.0f, 1.0f, 0.0f,
                                          0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 16> matrix       = perspective;

    EXPECT_TRUE(Analysis::Recipe::RecipeApplier::apply_overlay_shift(convergence_rule,
                                                                     Analysis::Recipe::Eye::LEFT,
                                                                     0.05f,
                                                                     &matrix) );
    EXPECT_FLOAT_EQ(matrix.at(8),
                    0.05f);

    matrix = perspective;

    EXPECT_TRUE(Analysis::Recipe::RecipeApplier::apply_overlay_shift(convergence_rule,
                                                                     Analysis::Recipe::Eye::RIGHT,
                                                                     0.05f,
                                                                     &matrix) );
    EXPECT_FLOAT_EQ(matrix.at(8),
                    -0.05f);

    matrix = perspective;

    EXPECT_FALSE(Analysis::Recipe::RecipeApplier::apply_overlay_shift(offset_rule,
                                                                      Analysis::Recipe::Eye::LEFT,
                                                                      0.05f,
                                                                      &matrix) );
    EXPECT_EQ   (matrix,
                 perspective);

    matrix = orthographic;

    EXPECT_FALSE(Analysis::Recipe::RecipeApplier::apply_overlay_shift(convergence_rule,
                                                                      Analysis::Recipe::Eye::LEFT,
                                                                      0.05f,
                                                                      &matrix) );
    EXPECT_EQ   (matrix,
                 orthographic);
}

TEST_F(RecipeApplierTest, TheParallaxOfTheOverlayIsTheRecipesAndZeroWithoutOne)
{
    Analysis::Recipe::Recipe recipe;

    EXPECT_FLOAT_EQ(Analysis::Recipe::RecipeApplier(recipe).get_overlay_parallax(),
                    0.0f);

    recipe.comfort.overlay_parallax = 0.04f;

    EXPECT_FLOAT_EQ(Analysis::Recipe::RecipeApplier(recipe).get_overlay_parallax(),
                    0.04f);
}

TEST(RecipeApplierPerEyeTextures, ATextureIsOneThatEachEyeHasACopyOfWhenTheRecipeNamesItsKeyAndItsOrdinal)
{
    Analysis::Recipe::Recipe        recipe;
    Analysis::Recipe::PerEyeTexture texture;

    texture.description_key = 0x1234;
    texture.ordinal         = 1;

    recipe.per_eye_texture_vec.push_back(texture);

    const Analysis::Recipe::RecipeApplier applier(recipe);

    Analysis::APIDumpAnalysis::ResourceIdentity identity;

    identity.description_key = 0x1234;
    identity.ordinal         = 1;

    EXPECT_TRUE(applier.is_per_eye_texture(identity) );

    /* Made like it, but not the one the recipe names. */
    identity.ordinal = 0;

    EXPECT_FALSE(applier.is_per_eye_texture(identity) );

    identity.description_key = 0x9999;
    identity.ordinal         = 1;

    EXPECT_FALSE(applier.is_per_eye_texture(identity) );
}

TEST_F(RecipeApplierTest, ADrawInOnePassBeatsTheKeyOfTheDrawAndTheKeyOfThePassAndLeavesTheSameDrawInOtherPassesAlone)
{
    Analysis::Recipe::Recipe   recipe;
    Analysis::Recipe::PassDraw unaffected_pass_draw;
    Analysis::Recipe::PassDraw affected_pass_draw;

    /* The draw 30 is in the passes 10 and 11 (a scene is drawn by one of them, and an overlay by the other). */
    recipe.unaffected.draw_key_vec = {30};
    recipe.unaffected.pass_key_vec = {12};

    unaffected_pass_draw.draw_key = 30;
    unaffected_pass_draw.pass_key = 11;
    affected_pass_draw.draw_key   = 30;
    affected_pass_draw.pass_key   = 12;

    recipe.affected.pass_draw_vec   = {affected_pass_draw};
    recipe.skipped.pass_draw_vec    = {unaffected_pass_draw};
    recipe.unaffected.pass_draw_vec = {unaffected_pass_draw};

    const Analysis::Recipe::RecipeApplier applier(recipe);

    /* The draw in its pass is named unaffected; in the pass that names it affected, that wins over the key of the draw and of the pass. */
    EXPECT_FALSE(applier.is_draw_affected(11,
                                          30) );
    EXPECT_TRUE (applier.is_draw_affected(12,
                                          30) );

    /* In a pass that does not name it, the key of the draw says. */
    EXPECT_FALSE(applier.is_draw_affected(10,
                                          30) );

    /* Skipped in the one pass, and played in the others. */
    EXPECT_TRUE (applier.is_draw_skipped(11,
                                         30) );
    EXPECT_FALSE(applier.is_draw_skipped(10,
                                         30) );
    EXPECT_FALSE(applier.is_draw_skipped(11,
                                         31) );
}

TEST_F(RecipeApplierTest, TheViewAnchorMatrixMovesTheSceneTowardTheRightForTheLeftEyeAndConvergesAtTheDistance)
{
    Analysis::Recipe::Recipe recipe;

    recipe.view_anchor_shift.convergence_distance = 4.0f;
    recipe.view_anchor_shift.is_enabled           = true;

    const Analysis::Recipe::RecipeApplier applier(recipe);

    const auto left  = applier.make_view_anchor_matrix(Analysis::Recipe::Eye::LEFT,
                                                       0.2f,
                                                       0);
    const auto right = applier.make_view_anchor_matrix(Analysis::Recipe::Eye::RIGHT,
                                                       0.2f,
                                                       0);

    /* The columns one after the other: x' = x + (h / d) * z + h, with h half of the separation. */
    EXPECT_FLOAT_EQ(left.at(12),
                    0.1f);
    EXPECT_FLOAT_EQ(left.at(8),
                    0.1f / 4.0f);
    EXPECT_FLOAT_EQ(right.at(12),
                    -0.1f);
    EXPECT_FLOAT_EQ(right.at(8),
                    -0.1f / 4.0f);

    /* A point straight ahead at the distance of convergence (z is minus the distance) is where it was: the pictures of the eyes are not apart there. */
    EXPECT_NEAR(left.at(12) + left.at(8) * -4.0f,
                0.0f,
                1e-6f);
    EXPECT_NEAR(right.at(12) + right.at(8) * -4.0f,
                0.0f,
                1e-6f);

    /* The rest is the identity. */
    EXPECT_FLOAT_EQ(left.at(0),
                    1.0f);
    EXPECT_FLOAT_EQ(left.at(5),
                    1.0f);
    EXPECT_FLOAT_EQ(left.at(10),
                    1.0f);
    EXPECT_FLOAT_EQ(left.at(15),
                    1.0f);
    EXPECT_FLOAT_EQ(left.at(9),
                    0.0f);
}

TEST_F(RecipeApplierTest, TheViewAnchorMatrixTakesTheDistanceAndTheLimitOfTheScene)
{
    Analysis::Recipe::Recipe recipe;

    recipe.scene_convergence_vec.push_back({2.0f, 11});
    recipe.scene_separation_vec.push_back ({0.05f, 11});
    recipe.view_anchor_shift.convergence_distance = 4.0f;
    recipe.view_anchor_shift.is_enabled           = true;

    const Analysis::Recipe::RecipeApplier applier(recipe);

    /* The scene of the shape has its own distance and a limit that is less than the separation asked for. */
    const auto in_scene = applier.make_view_anchor_matrix(Analysis::Recipe::Eye::LEFT,
                                                          0.2f,
                                                          11);

    EXPECT_FLOAT_EQ(in_scene.at(12),
                    0.025f);
    EXPECT_FLOAT_EQ(in_scene.at(8),
                    0.025f / 2.0f);

    /* A shape that the recipe does not know has the recipe's distance, and no limit. */
    const auto elsewhere = applier.make_view_anchor_matrix(Analysis::Recipe::Eye::LEFT,
                                                           0.2f,
                                                           99);

    EXPECT_FLOAT_EQ(elsewhere.at(12),
                    0.1f);
    EXPECT_FLOAT_EQ(elsewhere.at(8),
                    0.1f / 4.0f);
}

TEST_F(RecipeApplierTest, WithNoDistanceTheViewAnchorMatrixIsTheTranslationAlone)
{
    Analysis::Recipe::Recipe recipe;

    recipe.view_anchor_shift.is_enabled = true;

    const Analysis::Recipe::RecipeApplier applier(recipe);

    const auto left = applier.make_view_anchor_matrix(Analysis::Recipe::Eye::LEFT,
                                                      0.2f,
                                                      0);

    EXPECT_FLOAT_EQ(left.at(12),
                    0.1f);
    EXPECT_FLOAT_EQ(left.at(8),
                    0.0f);
    EXPECT_TRUE(applier.has_view_anchor_shift() );
}

TEST_F(RecipeApplierTest, WhatTheRulesLeaveAloneIsPushedOnlyIfTheRecipeSaysSoOrListsTheDraw)
{
    Analysis::Recipe::DrawQuery query;
    Analysis::Recipe::Recipe    recipe;

    query.signature_key = 7;

    recipe.is_baked = true;

    {
        const Analysis::Recipe::RecipeApplier applier(recipe);

        EXPECT_FALSE(applier.decide_draw(query).is_pushed);
    }

    recipe.comfort.pushes_unaffected = true;

    {
        /* A signature the baked recipe does not know is affected: not left alone, so not pushed. */
        const Analysis::Recipe::RecipeApplier applier(recipe);

        EXPECT_FALSE(applier.decide_draw(query).is_pushed);
    }

    Analysis::Recipe::DrawRule rule;

    rule.draw_class    = Analysis::Recipe::DrawClass::UNAFFECTED;
    rule.signature_key = 7;

    recipe.draw_rule_vec.push_back(rule);

    const Analysis::Recipe::RecipeApplier applier(recipe);

    EXPECT_TRUE(applier.decide_draw(query).is_pushed);
}
