/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for RecipeApplier: which rules are about a call, and what a rule does to a matrix.
 */
#include <gtest/gtest.h>
#include "analysis/recipe_applier/recipe_applier.h"
#include "Interception/Hooks/D3D8/types.h"

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
    static Analysis::RecipeLoader::Rule make_view_rule()
    {
        Analysis::RecipeLoader::Rule rule;
        Analysis::RecipeLoader::ArgumentCondition condition;

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

    static std::vector<const Analysis::RecipeLoader::Rule*> match(const Analysis::RecipeLoader::Recipe&      in_recipe,
                                                                  const Capture::APIDumpLoader::DumpedAPICall&     in_call,
                                                                  const Analysis::RecipeApplier::CallPlace&  in_place)
    {
        const Analysis::RecipeApplier::RecipeApplier applier(in_recipe);

        return applier.get_matching_rule_ptr_vec(in_call,
                                                 in_place);
    }
};

TEST_F(RecipeApplierTest, ARuleIsAboutTheFunctionAndTheArgumentsItNames)
{
    Analysis::RecipeLoader::Recipe recipe;

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
    Analysis::RecipeLoader::Recipe recipe;
    auto                           rule = make_view_rule();

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
    Analysis::RecipeLoader::Recipe recipe;

    recipe.rule_vec.push_back(make_view_rule() );

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

    EXPECT_TRUE (applier.has_rule_for(SET_TRANSFORM) );
    EXPECT_FALSE(applier.has_rule_for(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETMATERIAL) );
}

TEST_F(RecipeApplierTest, ARuleWithNoConditionsIsAboutEveryCallOfItsFunction)
{
    Analysis::RecipeLoader::Recipe recipe;
    auto                           rule = make_view_rule();

    rule.argument_condition_vec.clear();

    recipe.rule_vec.push_back(rule);

    EXPECT_EQ(match(recipe,
                    make_set_transform(Interception::Hooks::D3D8::D3DTS_PROJECTION),
                    {}).size(),
              1u);
}

TEST_F(RecipeApplierTest, ARuleWithPassKeysIsAboutTheCallsOfThosePasses)
{
    Analysis::RecipeLoader::Recipe recipe;
    auto                           rule = make_view_rule();

    rule.pass_key_vec = {0x11, 0x22};

    recipe.rule_vec.push_back(rule);

    Analysis::RecipeApplier::CallPlace in_pass;
    Analysis::RecipeApplier::CallPlace in_other_pass;
    Analysis::RecipeApplier::CallPlace in_no_pass;

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
    Analysis::RecipeLoader::Recipe recipe;
    auto                           rule = make_view_rule();

    rule.shape_key_vec = {0xaa};

    recipe.rule_vec.push_back(rule);

    Analysis::RecipeApplier::CallPlace known_frame;
    Analysis::RecipeApplier::CallPlace other_frame;

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
    Analysis::RecipeLoader::Recipe recipe;
    auto                           rule = make_view_rule();

    rule.pass_key_vec  = {0x11};
    rule.shape_key_vec = {0xaa};

    recipe.rule_vec.push_back(rule);

    Analysis::RecipeApplier::CallPlace place;

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
    Analysis::RecipeLoader::Recipe recipe;
    auto                           first  = make_view_rule();
    auto                           second = make_view_rule();

    first.name  = "first";
    second.name = "second";

    recipe.rule_vec.push_back(first);
    recipe.rule_vec.push_back(second);

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);
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
    Analysis::RecipeLoader::Rule rule = make_view_rule();

    rule.action_kind              = Analysis::RecipeLoader::ActionKind::MULTIPLY;
    rule.left_matrix_fp32vec16    = make_scale(2.0f);
    rule.right_matrix_fp32vec16   = make_scale(2.0f);

    auto matrix = make_translation(1.0f);

    /* Row vectors: M * X scales the translation too, X * M leaves it. */
    rule.multiply_order = Analysis::RecipeLoader::MultiplyOrder::POST;

    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
                                                                  0.0f,
                                                                  &matrix) );
    EXPECT_FLOAT_EQ(matrix.at(12),
                    2.0f);
    EXPECT_FLOAT_EQ(matrix.at(0),
                    2.0f);

    matrix = make_translation(1.0f);

    rule.multiply_order = Analysis::RecipeLoader::MultiplyOrder::PRE;

    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
                                                                  0.0f,
                                                                  &matrix) );
    EXPECT_FLOAT_EQ(matrix.at(12),
                    1.0f);
    EXPECT_FLOAT_EQ(matrix.at(0),
                    2.0f);
}

TEST_F(RecipeApplierTest, EachEyeGetsItsOwnMatrix)
{
    Analysis::RecipeLoader::Rule rule = make_view_rule();

    rule.action_kind    = Analysis::RecipeLoader::ActionKind::MULTIPLY;
    rule.multiply_order = Analysis::RecipeLoader::MultiplyOrder::POST;

    auto left  = make_identity();
    auto right = make_identity();

    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
                                                                   0.0f,
                                                                  &left) );
    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::RIGHT,
                                                                   0.0f,
                                                                  &right) );

    EXPECT_FLOAT_EQ(left.at(12),
                    1.0f);
    EXPECT_FLOAT_EQ(right.at(12),
                    -1.0f);
}

TEST_F(RecipeApplierTest, ReplacingPutsTheRecipesMatrixInPlaceOfTheCalls)
{
    Analysis::RecipeLoader::Rule rule = make_view_rule();

    rule.action_kind = Analysis::RecipeLoader::ActionKind::REPLACE;

    auto matrix = make_scale(5.0f);

    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::RIGHT,
                                                                  0.0f,
                                                                  &matrix) );
    EXPECT_EQ(matrix,
              make_translation(-1.0f) );
}

TEST_F(RecipeApplierTest, ARuleAboutPerspectiveOrOrthographicMatricesLeavesTheOthersAlone)
{
    Analysis::RecipeLoader::Rule rule = make_view_rule();

    rule.action_kind = Analysis::RecipeLoader::ActionKind::REPLACE;

    /* D3D perspective: the last column is (0, 0, 1, 0); orthographic: (0, 0, 0, 1). */
    std::array<float, 16> perspective = make_identity();
    std::array<float, 16> orthographic = make_identity();

    perspective.at(11)  = 1.0f;
    perspective.at(15)  = 0.0f;

    rule.matrix_kind = Analysis::RecipeLoader::MatrixKind::PERSPECTIVE;

    auto matrix = perspective;

    EXPECT_TRUE (Analysis::RecipeApplier::RecipeApplier::apply_rule(rule, Analysis::RecipeLoader::Eye::LEFT, 0.0f, &matrix) );

    matrix = orthographic;

    EXPECT_FALSE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule, Analysis::RecipeLoader::Eye::LEFT, 0.0f, &matrix) );
    EXPECT_EQ   (matrix,
                 orthographic);

    rule.matrix_kind = Analysis::RecipeLoader::MatrixKind::ORTHOGRAPHIC;

    EXPECT_TRUE (Analysis::RecipeApplier::RecipeApplier::apply_rule(rule, Analysis::RecipeLoader::Eye::LEFT, 0.0f, &matrix) );

    matrix = perspective;

    EXPECT_FALSE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule, Analysis::RecipeLoader::Eye::LEFT, 0.0f, &matrix) );
    EXPECT_EQ   (matrix,
                 perspective);

    rule.matrix_kind = Analysis::RecipeLoader::MatrixKind::ANY;

    EXPECT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule, Analysis::RecipeLoader::Eye::LEFT, 0.0f, &matrix) );
}

TEST_F(RecipeApplierTest, AFrameIsKnownIfItsShapeIsListedOrNoShapeIs)
{
    Analysis::RecipeLoader::Recipe recipe;

    EXPECT_TRUE(Analysis::RecipeApplier::RecipeApplier(recipe).is_frame_known(0x1234) );

    recipe.known_shape_key_vec = {0xaa, 0xbb};

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

    EXPECT_TRUE (applier.is_frame_known(0xbb) );
    EXPECT_FALSE(applier.is_frame_known(0xcc) );
}

TEST_F(RecipeApplierTest, ThePolicyForUnknownFramesIsThatOfTheRecipe)
{
    Analysis::RecipeLoader::Recipe recipe;

    recipe.unknown_frame_policy = Analysis::RecipeLoader::UnknownFramePolicy::SKIP;

    EXPECT_EQ(Analysis::RecipeApplier::RecipeApplier(recipe).get_unknown_frame_policy(),
              Analysis::RecipeLoader::UnknownFramePolicy::SKIP);
}

TEST_F(RecipeApplierTest, EveryDrawIsAffectedUnlessARecipeNamesItOrItsPass)
{
    Analysis::RecipeLoader::Recipe recipe;

    {
        const Analysis::RecipeApplier::RecipeApplier applier(recipe);

        EXPECT_TRUE(applier.is_draw_affected(1,
                                             2) );
    }

    recipe.unaffected.pass_key_vec = {10};
    recipe.unaffected.draw_key_vec = {20};

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

    EXPECT_FALSE(applier.is_draw_affected(10,
                                          1) );
    EXPECT_FALSE(applier.is_draw_affected(1,
                                          20) );
    EXPECT_TRUE (applier.is_draw_affected(1,
                                          1) );
}

TEST_F(RecipeApplierTest, ADrawKeyBeatsAPassKeyAndAnAffectedKeyBeatsNothing)
{
    Analysis::RecipeLoader::Recipe recipe;

    recipe.unaffected.pass_key_vec = {10, 11};
    recipe.unaffected.draw_key_vec = {20};
    recipe.affected.draw_key_vec   = {30};
    recipe.affected.pass_key_vec   = {11};

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

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
    Analysis::RecipeLoader::Recipe recipe;

    {
        const Analysis::RecipeApplier::RecipeApplier applier(recipe);

        EXPECT_FALSE(applier.is_draw_skipped(1,
                                             2) );
    }

    recipe.skipped.pass_key_vec = {10};
    recipe.skipped.draw_key_vec = {20};

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

    EXPECT_TRUE (applier.is_draw_skipped(10,
                                         1) );
    EXPECT_TRUE (applier.is_draw_skipped(1,
                                         20) );
    EXPECT_FALSE(applier.is_draw_skipped(1,
                                         1) );
}

TEST_F(RecipeApplierTest, AnEyeOffsetMovesTheEyesApartByHalfTheSeparationEachAlongTheAxis)
{
    struct AxisCase
    {
        Analysis::RecipeLoader::Axis axis;
        size_t                       n_element;
    };

    const AxisCase case_array[] = {{Analysis::RecipeLoader::Axis::X, 12},
                                   {Analysis::RecipeLoader::Axis::Y, 13},
                                   {Analysis::RecipeLoader::Axis::Z, 14}};

    for (const auto& axis_case : case_array)
    {
        Analysis::RecipeLoader::Rule rule;
        auto                         left  = make_identity();
        auto                         right = make_identity();

        rule.action_kind    = Analysis::RecipeLoader::ActionKind::EYE_OFFSET;
        rule.multiply_order = Analysis::RecipeLoader::MultiplyOrder::POST;
        rule.offset_axis    = axis_case.axis;

        ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                       Analysis::RecipeLoader::Eye::LEFT,
                                                                       0.2f,
                                                                      &left) );
        ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                       Analysis::RecipeLoader::Eye::RIGHT,
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
    Analysis::RecipeLoader::Rule rule;

    rule.action_kind    = Analysis::RecipeLoader::ActionKind::EYE_OFFSET;
    rule.multiply_order = Analysis::RecipeLoader::MultiplyOrder::POST;

    auto narrow = make_identity();
    auto wider   = make_identity();
    auto none  = make_identity();

    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
                                                                   0.2f,
                                                                  &narrow) );
    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
                                                                   0.4f,
                                                                  &wider) );
    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
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

    Analysis::RecipeLoader::Rule rule;
    auto                         post = scale;
    auto                         pre  = scale;

    rule.action_kind = Analysis::RecipeLoader::ActionKind::EYE_OFFSET;
    rule.offset_axis = Analysis::RecipeLoader::Axis::X;

    rule.multiply_order = Analysis::RecipeLoader::MultiplyOrder::POST;

    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
                                                                   1.0f,
                                                                  &post) );

    rule.multiply_order = Analysis::RecipeLoader::MultiplyOrder::PRE;

    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
                                                                   1.0f,
                                                                  &pre) );

    EXPECT_FLOAT_EQ(post.at(12),
                    0.5f);
    EXPECT_FLOAT_EQ(pre.at(12),
                    0.5f * 2.0f);
}

TEST_F(RecipeApplierTest, AnEyeOffsetOfAnotherKindOfMatrixIsLeftAlone)
{
    Analysis::RecipeLoader::Rule rule;
    auto                         matrix = make_identity();

    rule.action_kind    = Analysis::RecipeLoader::ActionKind::EYE_OFFSET;
    rule.matrix_kind    = Analysis::RecipeLoader::MatrixKind::PERSPECTIVE;
    rule.multiply_order = Analysis::RecipeLoader::MultiplyOrder::POST;

    /* An identity is not a perspective projection. */
    EXPECT_FALSE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                    Analysis::RecipeLoader::Eye::LEFT,
                                                                    1.0f,
                                                                   &matrix) );
    EXPECT_EQ   (matrix,
                 make_identity() );
}

TEST_F(RecipeApplierTest, ADrawIsAffectedUnlessARuleSaysOtherwise)
{
    Analysis::RecipeLoader::DrawRule rule;
    Analysis::RecipeLoader::Recipe   recipe;

    rule.draw_class    = Analysis::RecipeLoader::DrawClass::UNAFFECTED;
    rule.signature_key = 7;

    recipe.draw_rule_vec.push_back(rule);

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

    EXPECT_EQ(applier.classify_draw(7,
                                    make_identity() ),
              Analysis::RecipeLoader::DrawClass::UNAFFECTED);
    EXPECT_EQ(applier.classify_draw(8,
                                    make_identity() ),
              Analysis::RecipeLoader::DrawClass::AFFECTED);
}

TEST_F(RecipeApplierTest, ADrawRuleWithMatricesAppliesWhileTheViewMatrixIsOneOfThem)
{
    Analysis::RecipeLoader::Recipe   recipe;
    Analysis::RecipeLoader::DrawRule skipped_rule;
    Analysis::RecipeLoader::DrawRule unaffected_rule;

    unaffected_rule.draw_class    = Analysis::RecipeLoader::DrawClass::UNAFFECTED;
    unaffected_rule.signature_key = 7;
    unaffected_rule.view_matrix_vec.push_back(make_translation(2.5f) );
    unaffected_rule.view_matrix_vec.push_back(make_translation(3.0f) );

    skipped_rule.draw_class    = Analysis::RecipeLoader::DrawClass::SKIPPED;
    skipped_rule.signature_key = 7;
    skipped_rule.view_matrix_vec.push_back(make_translation(9.0f) );

    recipe.draw_rule_vec.push_back(unaffected_rule);
    recipe.draw_rule_vec.push_back(skipped_rule);

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

    EXPECT_EQ(applier.classify_draw(7,
                                    make_translation(2.5f) ),
              Analysis::RecipeLoader::DrawClass::UNAFFECTED);
    EXPECT_EQ(applier.classify_draw(7,
                                    make_translation(3.0f) ),
              Analysis::RecipeLoader::DrawClass::UNAFFECTED);
    EXPECT_EQ(applier.classify_draw(7,
                                    make_translation(9.0f) ),
              Analysis::RecipeLoader::DrawClass::SKIPPED);

    /* No rule applies to another camera, so the draw is affected. */
    EXPECT_EQ(applier.classify_draw(7,
                                    make_translation(4.0f) ),
              Analysis::RecipeLoader::DrawClass::AFFECTED);
    EXPECT_EQ(applier.classify_draw(7,
                                    make_identity() ),
              Analysis::RecipeLoader::DrawClass::AFFECTED);
}

TEST_F(RecipeApplierTest, ASignatureIsKnownIfTheRecipeListsItOrListsNone)
{
    Analysis::RecipeLoader::Recipe recipe;

    EXPECT_TRUE(Analysis::RecipeApplier::RecipeApplier(recipe).is_signature_known(5) );

    recipe.known_signature_key_vec.push_back(7);

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

    EXPECT_TRUE (applier.is_signature_known(7) );
    EXPECT_FALSE(applier.is_signature_known(5) );
}

TEST_F(RecipeApplierTest, AnEyeConvergenceShearsAPerspectiveProjectionSoThatWhatIsAtTheDistanceIsAtTheSamePlaceInBothEyes)
{
    Analysis::RecipeLoader::Rule rule;
    auto                         left  = make_identity();

    rule.action_kind = Analysis::RecipeLoader::ActionKind::EYE_CONVERGENCE;
    rule.matrix_kind = Analysis::RecipeLoader::MatrixKind::PERSPECTIVE;

    left.at(0)  = 2.0f;
    left.at(11) = 1.0f;
    left.at(15) = 0.0f;

    auto right = left;

    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
                                                                   0.2f,
                                                                  &left,
                                                                   5.0f) );
    ASSERT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::RIGHT,
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
    Analysis::RecipeLoader::Rule rule;
    auto                         perspective = make_identity();

    rule.action_kind = Analysis::RecipeLoader::ActionKind::EYE_CONVERGENCE;

    perspective.at(11) = 1.0f;
    perspective.at(15) = 0.0f;

    auto matrix = perspective;

    /* It is a rule about the matrix, though it changes nothing: the converter keeps it as the one of the eye. */
    EXPECT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
                                                                   0.2f,
                                                                  &matrix) );
    EXPECT_EQ  (matrix,
                perspective);
    EXPECT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                   Analysis::RecipeLoader::Eye::LEFT,
                                                                   0.0f,
                                                                  &matrix,
                                                                   5.0f) );
    EXPECT_EQ  (matrix,
                perspective);

    matrix = make_identity();

    EXPECT_FALSE(Analysis::RecipeApplier::RecipeApplier::apply_rule(rule,
                                                                    Analysis::RecipeLoader::Eye::LEFT,
                                                                    0.2f,
                                                                   &matrix,
                                                                    5.0f) );
    EXPECT_EQ   (matrix,
                 make_identity() );
}

TEST_F(RecipeApplierTest, TheConvergenceDistanceOfASignatureIsTheRecipesAndZeroForOneItDoesNotHave)
{
    Analysis::RecipeLoader::Recipe recipe;

    recipe.convergence_vec.push_back({12.5f, 7});

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

    EXPECT_FLOAT_EQ(applier.get_convergence_distance(7),
                    12.5f);
    EXPECT_FLOAT_EQ(applier.get_convergence_distance(8),
                    0.0f);
}

TEST_F(RecipeApplierTest, TheConvergenceDistanceOfAShapeIsTheRecipesAndZeroForOneItDoesNotHave)
{
    Analysis::RecipeLoader::Recipe recipe;

    recipe.scene_convergence_vec.push_back({3.5f, 11});

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

    EXPECT_FLOAT_EQ(applier.get_scene_convergence_distance(11),
                    3.5f);
    EXPECT_FLOAT_EQ(applier.get_scene_convergence_distance(12),
                    0.0f);
}

TEST_F(RecipeApplierTest, TheLimitOfTheSeparationOfAShapeIsTheRecipesAndZeroForOneItDoesNotHave)
{
    Analysis::RecipeLoader::Recipe recipe;

    recipe.scene_separation_vec.push_back({0.25f, 11});

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

    EXPECT_FLOAT_EQ(applier.get_scene_max_separation(11),
                    0.25f);
    EXPECT_FLOAT_EQ(applier.get_scene_max_separation(12),
                    0.0f);
}

TEST_F(RecipeApplierTest, AnOverlayIsPushedOnlyByTheRuleOfAProjectionAndOnlyInAPerspectiveMatrix)
{
    Analysis::RecipeLoader::Rule convergence_rule;
    Analysis::RecipeLoader::Rule offset_rule;

    convergence_rule.action_kind = Analysis::RecipeLoader::ActionKind::EYE_CONVERGENCE;
    offset_rule.action_kind      = Analysis::RecipeLoader::ActionKind::EYE_OFFSET;

    std::array<float, 16> perspective = {1.0f, 0.0f, 0.0f, 0.0f,
                                         0.0f, 1.0f, 0.0f, 0.0f,
                                         0.0f, 0.0f, 1.0f, 1.0f,
                                         0.0f, 0.0f, 0.0f, 0.0f};
    std::array<float, 16> orthographic = {1.0f, 0.0f, 0.0f, 0.0f,
                                          0.0f, 1.0f, 0.0f, 0.0f,
                                          0.0f, 0.0f, 1.0f, 0.0f,
                                          0.0f, 0.0f, 0.0f, 1.0f};
    std::array<float, 16> matrix      = perspective;

    EXPECT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_overlay_shift(convergence_rule,
                                                                            Analysis::RecipeLoader::Eye::LEFT,
                                                                            0.05f,
                                                                           &matrix) );
    EXPECT_FLOAT_EQ(matrix.at(8),
                    0.05f);

    matrix = perspective;

    EXPECT_TRUE(Analysis::RecipeApplier::RecipeApplier::apply_overlay_shift(convergence_rule,
                                                                            Analysis::RecipeLoader::Eye::RIGHT,
                                                                            0.05f,
                                                                           &matrix) );
    EXPECT_FLOAT_EQ(matrix.at(8),
                    -0.05f);

    matrix = perspective;

    EXPECT_FALSE(Analysis::RecipeApplier::RecipeApplier::apply_overlay_shift(offset_rule,
                                                                             Analysis::RecipeLoader::Eye::LEFT,
                                                                             0.05f,
                                                                            &matrix) );
    EXPECT_EQ   (matrix,
                 perspective);

    matrix = orthographic;

    EXPECT_FALSE(Analysis::RecipeApplier::RecipeApplier::apply_overlay_shift(convergence_rule,
                                                                             Analysis::RecipeLoader::Eye::LEFT,
                                                                             0.05f,
                                                                            &matrix) );
    EXPECT_EQ   (matrix,
                 orthographic);
}

TEST_F(RecipeApplierTest, TheParallaxOfTheOverlayIsTheRecipesAndZeroWithoutOne)
{
    Analysis::RecipeLoader::Recipe recipe;

    EXPECT_FLOAT_EQ(Analysis::RecipeApplier::RecipeApplier(recipe).get_overlay_parallax(),
                    0.0f);

    recipe.comfort.overlay_parallax = 0.04f;

    EXPECT_FLOAT_EQ(Analysis::RecipeApplier::RecipeApplier(recipe).get_overlay_parallax(),
                    0.04f);
}

TEST(RecipeApplierPerEyeTextures, ATextureIsOneThatEachEyeHasACopyOfWhenTheRecipeNamesItsKeyAndItsOrdinal)
{
    Analysis::RecipeLoader::Recipe recipe;
    Analysis::RecipeLoader::PerEyeTexture texture;

    texture.description_key = 0x1234;
    texture.ordinal         = 1;

    recipe.per_eye_texture_vec.push_back(texture);

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

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
    Analysis::RecipeLoader::Recipe recipe;
    Analysis::RecipeLoader::PassDraw unaffected_pass_draw;
    Analysis::RecipeLoader::PassDraw affected_pass_draw;

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

    const Analysis::RecipeApplier::RecipeApplier applier(recipe);

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
