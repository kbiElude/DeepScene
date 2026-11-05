/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for RecipeLoader: a valid recipe loads as written, and each kind of invalid recipe is refused with a message that says
 * where.
 */
#include <gtest/gtest.h>
#include "Analysis/RecipeLoader/recipe_loader.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Hooks/D3D8/types.h"
#include "data_fixtures.h"

class RecipeLoaderTest : public ::testing::Test
{
protected:
    /* Builds a valid recipe around the given top-level fields and rules. */
    static std::string get_recipe_text(const std::string& in_top_part,
                                       const std::string& in_rule_part)
    {
        return "{\"recipe_version\": 0, \"key_scheme_version\": 1," + in_top_part + "\"rules\": [" + in_rule_part + "]}";
    }

    static std::string get_rule_text(const std::string& in_call,
                                     const std::string& in_when,
                                     const std::string& in_action)
    {
        return "{\"call\": " + in_call + ", \"when\": " + in_when + ", \"action\": " + in_action + "}";
    }

    static std::string get_matrix_text(const std::string& in_translation_x)
    {
        return "[1,0,0,0, 0,1,0,0, 0,0,1,0, " + in_translation_x + ",0,0,1]";
    }

    static std::string get_action_text(const std::string& in_type_and_order)
    {
        return "{" + in_type_and_order + ", \"left\": " + get_matrix_text("0.5") + ", \"right\": " + get_matrix_text("-0.5") + "}";
    }

    /* Expects the recipe to be refused with a message containing the text. */
    void expect_refused(const std::string& in_text,
                        const std::string& in_message_part)
    {
        Analysis::RecipeLoader::Recipe recipe;
        std::string                    error;

        EXPECT_FALSE(Analysis::RecipeLoader::RecipeLoader::parse(in_text,
                                                                &recipe,
                                                                &error) ) << in_text;
        EXPECT_NE   (error.find(in_message_part),
                     std::string::npos) << in_message_part << " not in: " << error;
    }

    const std::string m_good_action = get_action_text("\"type\": \"multiply\", \"order\": \"post\"");
    const std::string m_good_call   = "\"IDirect3DDevice8::SetTransform\"";
};

TEST_F(RecipeLoaderTest, TheExampleFileIsRead)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::load(Tests::DataFixtures::get_recipe_example_filename(),
                                                              &recipe,
                                                              &error) ) << error;

    EXPECT_EQ(recipe.version,
              0u);
    EXPECT_EQ(recipe.key_scheme_version,
              1u);
    EXPECT_EQ(recipe.target_name,
              "fr08");
    EXPECT_FALSE(recipe.target_description.empty() );
    EXPECT_EQ(recipe.unknown_frame_policy,
              Analysis::RecipeLoader::UnknownFramePolicy::MONO);
    ASSERT_EQ(recipe.known_shape_key_vec.size(),
              1u);
    EXPECT_EQ(recipe.known_shape_key_vec.at(0),
              0x0123456789abcdefull);

    EXPECT_EQ  (recipe.unaffected.pass_key_vec,
                (std::vector<uint64_t>{0x1111111111111111ull}) );
    EXPECT_EQ  (recipe.unaffected.draw_key_vec,
                (std::vector<uint64_t>{0x2222222222222222ull}) );
    EXPECT_EQ  (recipe.affected.draw_key_vec,
                (std::vector<uint64_t>{0x3333333333333333ull}) );
    EXPECT_TRUE(recipe.affected.pass_key_vec.empty() );

    ASSERT_EQ(recipe.rule_vec.size(),
              3u);

    const auto& offset_rule = recipe.rule_vec.at(2);

    EXPECT_EQ(offset_rule.action_kind,
              Analysis::RecipeLoader::ActionKind::EYE_OFFSET);
    EXPECT_EQ(offset_rule.offset_axis,
              Analysis::RecipeLoader::Axis::Y);
    EXPECT_EQ(offset_rule.multiply_order,
              Analysis::RecipeLoader::MultiplyOrder::PRE);

    const auto& view_rule = recipe.rule_vec.at(0);

    EXPECT_EQ(view_rule.name,
              "scene view");
    EXPECT_EQ(view_rule.function,
              APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM);
    EXPECT_EQ(view_rule.action_kind,
              Analysis::RecipeLoader::ActionKind::MULTIPLY);
    EXPECT_EQ(view_rule.multiply_order,
              Analysis::RecipeLoader::MultiplyOrder::POST);
    EXPECT_EQ(view_rule.matrix_kind,
              Analysis::RecipeLoader::MatrixKind::ANY);
    EXPECT_EQ(view_rule.matrix_argument_index,
              2u);
    ASSERT_EQ(view_rule.argument_condition_vec.size(),
              1u);
    EXPECT_EQ(view_rule.argument_condition_vec.at(0).argument_index,
              1u);
    EXPECT_EQ(view_rule.argument_condition_vec.at(0).value,
              static_cast<uint32_t>(Interception::Hooks::D3D8::D3DTS_VIEW) );
    ASSERT_EQ(view_rule.pass_key_vec.size(),
              1u);
    EXPECT_EQ(view_rule.pass_key_vec.at(0),
              0xb5eee7f8dc36efb0ull);
    EXPECT_TRUE(view_rule.shape_key_vec.empty() );
    EXPECT_FLOAT_EQ(view_rule.left_matrix_fp32vec16.at(12),
                    0.032f);
    EXPECT_FLOAT_EQ(view_rule.right_matrix_fp32vec16.at(12),
                    -0.032f);

    const auto& projection_rule = recipe.rule_vec.at(1);

    EXPECT_EQ(projection_rule.action_kind,
              Analysis::RecipeLoader::ActionKind::REPLACE);
    EXPECT_EQ(projection_rule.matrix_kind,
              Analysis::RecipeLoader::MatrixKind::PERSPECTIVE);
    ASSERT_EQ(projection_rule.argument_condition_vec.size(),
              1u);
    EXPECT_EQ(projection_rule.argument_condition_vec.at(0).value,
              static_cast<uint32_t>(Interception::Hooks::D3D8::D3DTS_PROJECTION) );
    EXPECT_TRUE(projection_rule.pass_key_vec.empty() );
}

TEST_F(RecipeLoaderTest, AFileThatIsNotThereIsReported)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    EXPECT_FALSE(Analysis::RecipeLoader::RecipeLoader::load("this_recipe_does_not_exist.json",
                                                           &recipe,
                                                           &error) );
    EXPECT_NE   (error.find("Cannot open"),
                 std::string::npos);
}

TEST_F(RecipeLoaderTest, ARecipeWithNoRulesIsRead)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_TRUE(recipe.rule_vec.empty() );
    EXPECT_TRUE(recipe.known_shape_key_vec.empty() );
    EXPECT_EQ  (recipe.unknown_frame_policy,
                Analysis::RecipeLoader::UnknownFramePolicy::MONO);
}

TEST_F(RecipeLoaderTest, TheOptionalPartsOfARuleCanBeLeftOut)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                             "{\"call\": " + m_good_call + ", \"action\": " + m_good_action + "}"),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.rule_vec.size(),
              1u);
    EXPECT_TRUE(recipe.rule_vec.at(0).name.empty() );
    EXPECT_TRUE(recipe.rule_vec.at(0).argument_condition_vec.empty() );
    EXPECT_EQ  (recipe.rule_vec.at(0).matrix_kind,
                Analysis::RecipeLoader::MatrixKind::ANY);
}

TEST_F(RecipeLoaderTest, ARuleCanBeAboutTheTransformsOfDirect3D7)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                             get_rule_text("\"IDirect3DDevice7::SetTransform\"",
                                                                                           "{\"state\": \"D3DTRANSFORMSTATE_VIEW\"}",
                                                                                           m_good_action) ),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.rule_vec.size(),
              1u);
    EXPECT_EQ(recipe.rule_vec.at(0).function,
              APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTRANSFORM);
    EXPECT_EQ(recipe.rule_vec.at(0).matrix_argument_index,
              2u);
    ASSERT_EQ(recipe.rule_vec.at(0).argument_condition_vec.size(),
              1u);
    EXPECT_EQ(recipe.rule_vec.at(0).argument_condition_vec.at(0).argument_index,
              1u);
    EXPECT_EQ(recipe.rule_vec.at(0).argument_condition_vec.at(0).value,
              static_cast<uint32_t>(Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_VIEW) );
}

TEST_F(RecipeLoaderTest, TheNamesOfTheTransformsAreThoseOfTheAPIOfTheFunction)
{
    expect_refused(get_recipe_text("",
                                   get_rule_text("\"IDirect3DDevice7::SetTransform\"",
                                                 "{\"state\": \"D3DTS_VIEW\"}",
                                                 m_good_action) ),
                   "rules[0].when.state: \"D3DTS_VIEW\" is not a transform state");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{\"state\": \"D3DTRANSFORMSTATE_VIEW\"}",
                                                 m_good_action) ),
                   "rules[0].when.state: \"D3DTRANSFORMSTATE_VIEW\" is not a transform state");
}

TEST_F(RecipeLoaderTest, EveryPolicyForUnknownFramesIsRead)
{
    const struct
    {
        const char*                                text_ptr;
        Analysis::RecipeLoader::UnknownFramePolicy policy;
    } case_array[] =
    {
        {"mono", Analysis::RecipeLoader::UnknownFramePolicy::MONO},
        {"skip", Analysis::RecipeLoader::UnknownFramePolicy::SKIP},
        {"fail", Analysis::RecipeLoader::UnknownFramePolicy::FAIL}
    };

    for (const auto& policy_case : case_array)
    {
        Analysis::RecipeLoader::Recipe recipe;
        std::string                    error;

        ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text(std::string("\"on_unknown_frame\": \"") + policy_case.text_ptr + "\",",
                                                                                 ""),
                                                               &recipe,
                                                               &error) ) << error;
        EXPECT_EQ  (recipe.unknown_frame_policy,
                    policy_case.policy);
    }
}

TEST_F(RecipeLoaderTest, ATextThatIsNotJsonIsRefused)
{
    expect_refused("{\"recipe_version\": 0,}",
                   "Not valid JSON");
    expect_refused("",
                   "Not valid JSON");
}

TEST_F(RecipeLoaderTest, ARecipeThatIsNotAnObjectIsRefused)
{
    expect_refused("[]",
                   "must be a JSON object");
}

TEST_F(RecipeLoaderTest, TheVersionsMustBeThoseOfThisBuild)
{
    expect_refused("{\"recipe_version\": 1, \"key_scheme_version\": 1, \"rules\": []}",
                   "recipe_version: this version reads recipes of version 0, not 1");
    expect_refused("{\"key_scheme_version\": 1, \"rules\": []}",
                   "recipe_version is missing");
    expect_refused("{\"recipe_version\": 0, \"key_scheme_version\": 2, \"rules\": []}",
                   "key_scheme_version: the keys of version 2");
    expect_refused("{\"recipe_version\": 0, \"rules\": []}",
                   "key_scheme_version is missing");
    expect_refused("{\"recipe_version\": \"0\", \"key_scheme_version\": 1, \"rules\": []}",
                   "recipe_version: expected a whole number");
    expect_refused("{\"recipe_version\": 0.5, \"key_scheme_version\": 1, \"rules\": []}",
                   "recipe_version: expected a whole number");
    expect_refused("{\"recipe_version\": -1, \"key_scheme_version\": 1, \"rules\": []}",
                   "recipe_version: expected a whole number");
}

TEST_F(RecipeLoaderTest, FieldsThatAreNotKnownAreRefused)
{
    expect_refused(get_recipe_text("\"extra\": 1,",
                                   ""),
                   "unknown field \"extra\"");
    expect_refused(get_recipe_text("\"target\": {\"nme\": \"x\"},",
                                   ""),
                   "target: unknown field \"nme\"");
    expect_refused(get_recipe_text("",
                                   "{\"call\": " + m_good_call + ", \"action\": " + m_good_action + ", \"colour\": 1}"),
                   "rules[0]: unknown field \"colour\"");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{\"stat\": \"D3DTS_VIEW\"}",
                                                 m_good_action) ),
                   "rules[0].when: unknown field \"stat\"");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 "{\"type\": \"replace\", \"left\": " + get_matrix_text("0") + ", \"right\": " +
                                                 get_matrix_text("0") + ", \"scale\": 2}") ),
                   "rules[0].action: unknown field \"scale\"");
}

TEST_F(RecipeLoaderTest, TheTargetMustBeText)
{
    expect_refused(get_recipe_text("\"target\": 5,",
                                   ""),
                   "target: expected an object");
    expect_refused(get_recipe_text("\"target\": {\"name\": 5},",
                                   ""),
                   "target.name: expected a string");
    expect_refused(get_recipe_text("\"target\": {\"description\": []},",
                                   ""),
                   "target.description: expected a string");
}

TEST_F(RecipeLoaderTest, ThePolicyForUnknownFramesMustBeOneOfThree)
{
    expect_refused(get_recipe_text("\"on_unknown_frame\": \"ignore\",",
                                   ""),
                   "on_unknown_frame: expected \"mono\", \"skip\" or \"fail\", not \"ignore\"");
    expect_refused(get_recipe_text("\"on_unknown_frame\": 1,",
                                   ""),
                   "on_unknown_frame: expected");
}

TEST_F(RecipeLoaderTest, KeysMustBeSixteenHexDigitsAfter0x)
{
    const char* bad_key_ptr_array[] =
    {
        "\"0123456789abcdef\"",
        "\"0x123\"",
        "\"0x0123456789abcdeg\"",
        "\"0x0123456789abcdef0\"",
        "12",
        "\"\""
    };

    for (const auto& bad_key_ptr : bad_key_ptr_array)
    {
        expect_refused(get_recipe_text(std::string("\"known_shape_keys\": [") + bad_key_ptr + "],",
                                       ""),
                       "known_shape_keys[0]: expected a key");
    }

    expect_refused(get_recipe_text("\"known_shape_keys\": \"0x0123456789abcdef\",",
                                   ""),
                   "known_shape_keys: expected an array of keys");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{\"pass_keys\": [\"0x0123456789abcdef\", \"x\"]}",
                                                 m_good_action) ),
                   "rules[0].when.pass_keys[1]: expected a key");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{\"shape_keys\": [7]}",
                                                 m_good_action) ),
                   "rules[0].when.shape_keys[0]: expected a key");
}

TEST_F(RecipeLoaderTest, KeysInCapitalLettersAreRead)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"known_shape_keys\": [\"0xFEDCBA9876543210\"],",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;
    EXPECT_EQ  (recipe.known_shape_key_vec.at(0),
                0xfedcba9876543210ull);
}

TEST_F(RecipeLoaderTest, RulesMustBeAnArrayOfObjects)
{
    expect_refused("{\"recipe_version\": 0, \"key_scheme_version\": 1}",
                   "rules is missing");
    expect_refused("{\"recipe_version\": 0, \"key_scheme_version\": 1, \"rules\": {}}",
                   "rules: expected an array");
    expect_refused(get_recipe_text("",
                                   "5"),
                   "rules[0]: expected an object");
    expect_refused(get_recipe_text("",
                                   "{\"call\": " + m_good_call + ", \"action\": " + m_good_action + "}, 3"),
                   "rules[1]: expected an object");
}

TEST_F(RecipeLoaderTest, ACallMustBeOneThatARuleCanBeAbout)
{
    expect_refused(get_recipe_text("",
                                   "{\"action\": " + m_good_action + "}"),
                   "rules[0].call: expected the name of a function");
    expect_refused(get_recipe_text("",
                                   get_rule_text("\"IDirect3DDevice8::Present\"",
                                                 "{}",
                                                 m_good_action) ),
                   "is not a function that a rule can be about");
    expect_refused(get_recipe_text("",
                                   get_rule_text("5",
                                                 "{}",
                                                 m_good_action) ),
                   "rules[0].call: expected the name of a function");
}

TEST_F(RecipeLoaderTest, ARuleNeedsAnAction)
{
    expect_refused(get_recipe_text("",
                                   "{\"call\": " + m_good_call + "}"),
                   "rules[0].action is missing");
}

TEST_F(RecipeLoaderTest, TheStateOfATransformIsANameOrANumber)
{
    const struct
    {
        const char* state_ptr;
        uint32_t    expected;
    } case_array[] =
    {
        {"\"D3DTS_VIEW\"",       Interception::Hooks::D3D8::D3DTS_VIEW},
        {"\"D3DTS_PROJECTION\"", Interception::Hooks::D3D8::D3DTS_PROJECTION},
        {"\"D3DTS_WORLD\"",      Interception::Hooks::D3D8::D3DTS_WORLD},
        {"\"D3DTS_WORLD3\"",     Interception::Hooks::D3D8::D3DTS_WORLD3},
        {"\"D3DTS_TEXTURE7\"",   Interception::Hooks::D3D8::D3DTS_TEXTURE7},
        {"2",                    Interception::Hooks::D3D8::D3DTS_VIEW},
        {"300",                  300}
    };

    for (const auto& state_case : case_array)
    {
        Analysis::RecipeLoader::Recipe recipe;
        std::string                    error;

        ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                                 get_rule_text(m_good_call,
                                                                                               std::string("{\"state\": ") + state_case.state_ptr + "}",
                                                                                               m_good_action) ),
                                                               &recipe,
                                                               &error) ) << error;
        EXPECT_EQ  (recipe.rule_vec.at(0).argument_condition_vec.at(0).value,
                    state_case.expected);
    }

    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{\"state\": \"D3DTS_VIEWY\"}",
                                                 m_good_action) ),
                   "rules[0].when.state: \"D3DTS_VIEWY\" is not a transform state");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{\"state\": true}",
                                                 m_good_action) ),
                   "rules[0].when.state: expected a transform state");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{\"state\": -3}",
                                                 m_good_action) ),
                   "rules[0].when.state: expected a whole number");
}

TEST_F(RecipeLoaderTest, TheKindOfMatrixIsOneOfThree)
{
    const struct
    {
        const char*                        text_ptr;
        Analysis::RecipeLoader::MatrixKind kind;
    } case_array[] =
    {
        {"any",          Analysis::RecipeLoader::MatrixKind::ANY},
        {"orthographic", Analysis::RecipeLoader::MatrixKind::ORTHOGRAPHIC},
        {"perspective",  Analysis::RecipeLoader::MatrixKind::PERSPECTIVE}
    };

    for (const auto& kind_case : case_array)
    {
        Analysis::RecipeLoader::Recipe recipe;
        std::string                    error;

        ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                                 get_rule_text(m_good_call,
                                                                                               std::string("{\"matrix\": \"") + kind_case.text_ptr + "\"}",
                                                                                               m_good_action) ),
                                                               &recipe,
                                                               &error) ) << error;
        EXPECT_EQ  (recipe.rule_vec.at(0).matrix_kind,
                    kind_case.kind);
    }

    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{\"matrix\": \"round\"}",
                                                 m_good_action) ),
                   "rules[0].when.matrix: expected");
}

TEST_F(RecipeLoaderTest, AMultiplyAndAnEyeOffsetNeedAnOrderAndAReplaceHasNone)
{
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 get_action_text("\"type\": \"multiply\"") ) ),
                   "rules[0].action.order is missing");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 get_action_text("\"type\": \"multiply\", \"order\": \"before\"") ) ),
                   "rules[0].action.order: expected \"post\" or \"pre\"");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 get_action_text("\"type\": \"replace\", \"order\": \"post\"") ) ),
                   "rules[0].action.order: replace has no order");

    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                             get_rule_text(m_good_call,
                                                                                           "{}",
                                                                                           get_action_text("\"type\": \"multiply\", \"order\": \"pre\"") ) ),
                                                           &recipe,
                                                           &error) ) << error;
    EXPECT_EQ  (recipe.rule_vec.at(0).multiply_order,
                Analysis::RecipeLoader::MultiplyOrder::PRE);
}

TEST_F(RecipeLoaderTest, TheTypeOfAnActionIsEyeOffsetMultiplyOrReplace)
{
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 get_action_text("\"order\": \"post\"") ) ),
                   "rules[0].action.type is missing");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 get_action_text("\"type\": \"add\"") ) ),
                   "rules[0].action.type: expected \"eye_convergence\", \"eye_offset\", \"multiply\" or \"replace\"");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 "5") ),
                   "rules[0].action: expected an object");
}

TEST_F(RecipeLoaderTest, MatricesNeedSixteenNumbersEach)
{
    const std::string identity     = get_matrix_text("0");
    const std::string no_left      = "{\"type\": \"replace\", \"right\": " + identity + "}";
    const std::string no_right     = "{\"type\": \"replace\", \"left\": " + identity + "}";
    const std::string short_left   = "{\"type\": \"replace\", \"left\": [1,2,3], \"right\": " + identity + "}";
    const std::string text_in_left = "{\"type\": \"replace\", \"left\": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,\"x\",1], \"right\": " + identity + "}";
    const std::string huge_right   = "{\"type\": \"replace\", \"left\": " + identity + ", \"right\": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,1e300,1]}";
    const std::string not_array    = "{\"type\": \"replace\", \"left\": 1, \"right\": " + identity + "}";

    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 no_left) ),
                   "rules[0].action.left is missing");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 no_right) ),
                   "rules[0].action.right is missing");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 short_left) ),
                   "rules[0].action.left: expected 16 numbers");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 text_in_left) ),
                   "rules[0].action.left[14]: expected a number");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 huge_right) ),
                   "rules[0].action.right[14]: expected a number that fits a float");
    expect_refused(get_recipe_text("",
                                   get_rule_text(m_good_call,
                                                 "{}",
                                                 not_array) ),
                   "rules[0].action.left: expected 16 numbers");
}

TEST_F(RecipeLoaderTest, TheNamesOfRulesAreUnique)
{
    const std::string rule_text = "{\"name\": \"a\", \"call\": " + m_good_call + ", \"action\": " + m_good_action + "}";

    expect_refused(get_recipe_text("",
                                   rule_text + "," + rule_text),
                   "rules[1].name: \"a\" is used by another rule");

    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    /* Unnamed rules do not clash. */
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                             "{\"call\": " + m_good_call + ", \"action\": " + m_good_action + "}," +
                                                                             "{\"call\": " + m_good_call + ", \"action\": " + m_good_action + "}"),
                                                           &recipe,
                                                           &error) ) << error;
    EXPECT_EQ  (recipe.rule_vec.size(),
                2u);
}

TEST_F(RecipeLoaderTest, AFailedReadLeavesNothingBehind)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::load(Tests::DataFixtures::get_recipe_example_filename(),
                                                              &recipe,
                                                              &error) );

    EXPECT_FALSE(Analysis::RecipeLoader::RecipeLoader::parse("{\"recipe_version\": 3}",
                                                            &recipe,
                                                            &error) );
    EXPECT_TRUE (recipe.rule_vec.empty() );
    EXPECT_TRUE (recipe.known_shape_key_vec.empty() );
}

TEST_F(RecipeLoaderTest, ATextWhichStartsWithAByteOrderMarkLoads)
{
    Analysis::RecipeLoader::Recipe recipe;
    std::string                    error;

    EXPECT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(std::string("\xEF\xBB\xBF") + get_recipe_text("", ""),
                                                           &recipe,
                                                           &error) ) << error;
}

TEST_F(RecipeLoaderTest, AnEmptyListOfKnownShapesIsRefused)
{
    expect_refused(get_recipe_text("\"known_shape_keys\": [],", ""),
                   "known_shape_keys: the list is empty");
}

TEST_F(RecipeLoaderTest, WithoutSelectorsEveryDrawIsAffected)
{
    Analysis::RecipeLoader::Recipe recipe;
    std::string                    error;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("", ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_TRUE (recipe.affected.draw_key_vec.empty() );
    EXPECT_TRUE (recipe.affected.pass_key_vec.empty() );
    EXPECT_TRUE (recipe.unaffected.draw_key_vec.empty() );
    EXPECT_TRUE (recipe.unaffected.pass_key_vec.empty() );
}

TEST_F(RecipeLoaderTest, TheEyeSeparationIsNotAFieldOfARecipe)
{
    expect_refused(get_recipe_text("\"eye_separation\": 0.2,", ""),
                   "eye_separation");
}

TEST_F(RecipeLoaderTest, AnEyeOffsetNeedsAnAxisAndAnOrderAndTakesNoMatrices)
{
    const std::string call = "\"IDirect3DDevice8::SetTransform\"";
    const std::string when = "{\"state\": \"D3DTS_VIEW\"}";

    Analysis::RecipeLoader::Recipe recipe;
    std::string                    error;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("", get_rule_text(call, when, "{\"type\": \"eye_offset\", \"order\": \"post\", \"axis\": \"z\"}") ),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.rule_vec.size(),
              1u);
    EXPECT_EQ(recipe.rule_vec.at(0).action_kind,
              Analysis::RecipeLoader::ActionKind::EYE_OFFSET);
    EXPECT_EQ(recipe.rule_vec.at(0).offset_axis,
              Analysis::RecipeLoader::Axis::Z);
    EXPECT_EQ(recipe.rule_vec.at(0).multiply_order,
              Analysis::RecipeLoader::MultiplyOrder::POST);

    expect_refused(get_recipe_text("", get_rule_text(call, when, "{\"type\": \"eye_offset\", \"order\": \"post\"}") ),
                   ".axis is missing");
    expect_refused(get_recipe_text("", get_rule_text(call, when, "{\"type\": \"eye_offset\", \"axis\": \"x\"}") ),
                   ".order is missing");
    expect_refused(get_recipe_text("", get_rule_text(call, when, "{\"type\": \"eye_offset\", \"order\": \"post\", \"axis\": \"w\"}") ),
                   ".axis: expected");
    expect_refused(get_recipe_text("", get_rule_text(call, when, "{\"type\": \"eye_offset\", \"order\": \"post\", \"axis\": \"x\", \"left\": " + get_matrix_text("0") + "}") ),
                   "eye_offset takes no matrices");
    expect_refused(get_recipe_text("", get_rule_text(call, when, get_action_text("\"type\": \"multiply\", \"order\": \"post\", \"axis\": \"x\"") ) ),
                   "only eye_offset has an axis");
}

TEST_F(RecipeLoaderTest, SelectorsThatAreNotRightAreRefusedWithTheirPlace)
{
    expect_refused(get_recipe_text("\"unaffected\": [],", ""),
                   "unaffected: expected an object");
    expect_refused(get_recipe_text("\"unaffected\": {},", ""),
                   "unaffected: names no draw and no pass");
    expect_refused(get_recipe_text("\"unaffected\": {\"pass_keys\": []},", ""),
                   "unaffected.pass_keys: the list is empty");
    expect_refused(get_recipe_text("\"affected\": {\"draw_keys\": [\"0x0000000000000012\", 5]},", ""),
                   "affected.draw_keys[1]");
    expect_refused(get_recipe_text("\"affected\": {\"frame_keys\": [\"0x0000000000000012\"]},", ""),
                   "frame_keys");
}

TEST_F(RecipeLoaderTest, SkippedDrawsAndPassesAreRead)
{
    Analysis::RecipeLoader::Recipe recipe;
    std::string                    error;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"skipped\": {\"pass_keys\": [\"0x0000000000000012\"], \"draw_keys\": [\"0x0000000000000034\"]},", ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_EQ(recipe.skipped.pass_key_vec,
              (std::vector<uint64_t>{0x12ull}) );
    EXPECT_EQ(recipe.skipped.draw_key_vec,
              (std::vector<uint64_t>{0x34ull}) );

    /* A skipped draw may also be named affected or unaffected: skipping wins, so it is not a conflict. */
    EXPECT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"skipped\": {\"draw_keys\": [\"0x0000000000000034\"]}, \"unaffected\": {\"draw_keys\": [\"0x0000000000000034\"]},", ""),
                                                           &recipe,
                                                           &error) ) << error;

    expect_refused(get_recipe_text("\"skipped\": {},", ""),
                   "skipped: names no draw and no pass");
}

TEST_F(RecipeLoaderTest, AKeyInBothListsOfTheSameKindIsRefusedButOneKeyOfEachKindIsNot)
{
    expect_refused(get_recipe_text("\"unaffected\": {\"draw_keys\": [\"0x0000000000000012\"]}, \"affected\": {\"draw_keys\": [\"0x0000000000000012\"]},", ""),
                   "a key is in both");
    expect_refused(get_recipe_text("\"unaffected\": {\"pass_keys\": [\"0x0000000000000012\"]}, \"affected\": {\"pass_keys\": [\"0x0000000000000034\", \"0x0000000000000012\"]},", ""),
                   "a key is in both");

    /* A pass and a draw may have the same key: they are keys of different things. */
    Analysis::RecipeLoader::Recipe recipe;
    std::string                    error;

    EXPECT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"unaffected\": {\"pass_keys\": [\"0x0000000000000012\"]}, \"affected\": {\"draw_keys\": [\"0x0000000000000012\"]},", ""),
                                                           &recipe,
                                                           &error) ) << error;
}

TEST_F(RecipeLoaderTest, BakedDrawRulesAreRead)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": ["
                                                                            "{\"signature\": \"0x0000000000000001\", \"class\": \"skipped\"},"
                                                                            "{\"signature\": \"0x0000000000000002\", \"class\": \"unaffected\","
                                                                            " \"when\": {\"constant\": \"view_matrix\", \"equals_any\": [" + get_matrix_text("2.5") + "," + get_matrix_text("3") + "]}},"
                                                                            "{\"signature\": \"0x0000000000000002\", \"class\": \"affected\"}]},",
                                                                            ""),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.draw_rule_vec.size(),
              3u);
    EXPECT_TRUE(recipe.is_baked);

    EXPECT_EQ  (recipe.draw_rule_vec.at(0).draw_class,
                Analysis::RecipeLoader::DrawClass::SKIPPED);
    EXPECT_EQ  (recipe.draw_rule_vec.at(0).signature_key,
                1u);
    EXPECT_TRUE(recipe.draw_rule_vec.at(0).view_matrix_vec.empty() );

    EXPECT_EQ  (recipe.draw_rule_vec.at(1).draw_class,
                Analysis::RecipeLoader::DrawClass::UNAFFECTED);
    ASSERT_EQ  (recipe.draw_rule_vec.at(1).view_matrix_vec.size(),
                2u);
    EXPECT_FLOAT_EQ(recipe.draw_rule_vec.at(1).view_matrix_vec.at(0).at(12),
                    2.5f);
    EXPECT_FLOAT_EQ(recipe.draw_rule_vec.at(1).view_matrix_vec.at(1).at(12),
                    3.0f);

    EXPECT_EQ  (recipe.draw_rule_vec.at(2).draw_class,
                Analysis::RecipeLoader::DrawClass::AFFECTED);
}

TEST_F(RecipeLoaderTest, ARecipeWithoutABakedSectionIsNotBaked)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                            ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_FALSE(recipe.is_baked);
    EXPECT_TRUE (recipe.draw_rule_vec.empty() );
}

TEST_F(RecipeLoaderTest, WhatIsWrongInABakedSectionIsRefused)
{
    const std::string rule_part = "{\"signature\": \"0x0000000000000001\", \"class\": \"unaffected\"}";

    expect_refused(get_recipe_text("\"baked\": [],",
                                   ""),
                   "baked: expected an object");
    expect_refused(get_recipe_text("\"baked\": {\"draw_rules\": []},",
                                   ""),
                   "signature_scheme_version is missing");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 99, \"draw_rules\": []},",
                                   ""),
                   "bake the recipe again");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1},",
                                   ""),
                   "draw_rules is missing");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [{\"signature\": \"0x0000000000000001\"}]},",
                                   ""),
                   "class is missing");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [{\"signature\": \"0x0000000000000001\", \"class\": \"half\"}]},",
                                   ""),
                   "expected \"affected\", \"skipped\" or \"unaffected\"");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [{\"class\": \"skipped\"}]},",
                                   ""),
                   "signature is missing");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [{\"signature\": \"0x0000000000000001\", \"class\": \"skipped\", \"pass\": 1}]},",
                                   ""),
                   "unknown field \"pass\"");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [{\"signature\": \"0x0000000000000001\", \"class\": \"skipped\","
                                   " \"when\": {\"constant\": \"projection\", \"equals_any\": [" + get_matrix_text("1") + "]}}]},",
                                   ""),
                   "the only constant a rule can be about");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [{\"signature\": \"0x0000000000000001\", \"class\": \"skipped\","
                                   " \"when\": {\"constant\": \"view_matrix\", \"equals_any\": []}}]},",
                                   ""),
                   "one at least");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [{\"signature\": \"0x0000000000000001\", \"class\": \"skipped\","
                                   " \"when\": {\"constant\": \"view_matrix\", \"equals_any\": [[1, 2]]}}]},",
                                   ""),
                   "expected 16 numbers");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [" + rule_part + "," +
                                   "{\"signature\": \"0x0000000000000001\", \"class\": \"affected\"}]},",
                                   ""),
                   "never reached");
}

TEST_F(RecipeLoaderTest, TheKnownSignaturesOfABakedSectionAreRead)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [],"
                                                                            " \"known_signatures\": [\"0x0000000000000003\", \"0x0000000000000001\"]},",
                                                                            ""),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.known_signature_key_vec.size(),
              2u);
    EXPECT_EQ(recipe.known_signature_key_vec.at(0),
              3u);
    EXPECT_EQ(recipe.known_signature_key_vec.at(1),
              1u);

    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"known_signatures\": [\"1\"]},",
                                   ""),
                   "known_signatures[0]");
}

TEST_F(RecipeLoaderTest, AnEyeConvergenceTakesNothing)
{
    const std::string call = "\"IDirect3DDevice8::SetTransform\"";
    const std::string when = "{\"state\": \"D3DTS_PROJECTION\", \"matrix\": \"perspective\"}";

    Analysis::RecipeLoader::Recipe recipe;
    std::string                    error;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("", get_rule_text(call, when, "{\"type\": \"eye_convergence\"}") ),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.rule_vec.size(),
              1u);
    EXPECT_EQ(recipe.rule_vec.at(0).action_kind,
              Analysis::RecipeLoader::ActionKind::EYE_CONVERGENCE);
    EXPECT_EQ(recipe.rule_vec.at(0).matrix_kind,
              Analysis::RecipeLoader::MatrixKind::PERSPECTIVE);

    expect_refused(get_recipe_text("", get_rule_text(call, when, "{\"type\": \"eye_convergence\", \"order\": \"post\"}") ),
                   "eye_convergence takes no order or matrices");
    expect_refused(get_recipe_text("", get_rule_text(call, when, "{\"type\": \"eye_convergence\", \"left\": " + get_matrix_text("0") + "}") ),
                   "eye_convergence takes no order or matrices");
    expect_refused(get_recipe_text("", get_rule_text(call, when, "{\"type\": \"eye_convergence\", \"axis\": \"x\"}") ),
                   "only eye_offset has an axis");
}

TEST_F(RecipeLoaderTest, BakedConvergenceDistancesAreRead)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [],"
                                                                            " \"convergence\": [{\"signature\": \"0x0000000000000001\", \"distance\": 12.5},"
                                                                            " {\"signature\": \"0x0000000000000002\", \"distance\": 3}]},",
                                                                            ""),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.convergence_vec.size(),
              2u);
    EXPECT_EQ(recipe.convergence_vec.at(0).signature_key,
              1u);
    EXPECT_FLOAT_EQ(recipe.convergence_vec.at(0).distance,
                    12.5f);
    EXPECT_FLOAT_EQ(recipe.convergence_vec.at(1).distance,
                    3.0f);
}

TEST_F(RecipeLoaderTest, WhatIsWrongInTheBakedConvergenceDistancesIsRefused)
{
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"convergence\": {}},",
                                   ""),
                   "baked.convergence: expected an array");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"convergence\": [{\"distance\": 1}]},",
                                   ""),
                   "baked.convergence[0].signature is missing");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"convergence\": [{\"signature\": \"0x0000000000000001\"}]},",
                                   ""),
                   "baked.convergence[0].distance is missing");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"convergence\": [{\"signature\": \"0x0000000000000001\", \"distance\": 0}]},",
                                   ""),
                   "baked.convergence[0].distance: expected a number above zero");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"convergence\": [{\"signature\": \"0x0000000000000001\", \"distance\": -2}]},",
                                   ""),
                   "baked.convergence[0].distance: expected a number above zero");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"convergence\": [{\"signature\": \"0x0000000000000001\", \"distance\": 1},"
                                   " {\"signature\": \"0x0000000000000001\", \"distance\": 2}]},",
                                   ""),
                   "the signature has a distance already");
}

TEST_F(RecipeLoaderTest, BakedSceneConvergenceDistancesAreReadAndWhatIsWrongInThemIsRefused)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [],"
                                                                            " \"scene_convergence\": [{\"shape\": \"0x00000000000000ab\", \"distance\": 7.5}]},",
                                                                            ""),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.scene_convergence_vec.size(),
              1u);
    EXPECT_EQ(recipe.scene_convergence_vec.at(0).shape_key,
              0xabu);
    EXPECT_FLOAT_EQ(recipe.scene_convergence_vec.at(0).distance,
                    7.5f);
    EXPECT_TRUE(recipe.convergence_vec.empty() );

    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"scene_convergence\": [{\"signature\": \"0x01\", \"distance\": 1}]},",
                                   ""),
                   "baked.scene_convergence[0]: unknown field \"signature\"");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"scene_convergence\": [{\"shape\": \"0x0000000000000001\", \"distance\": 1},"
                                   " {\"shape\": \"0x0000000000000001\", \"distance\": 2}]},",
                                   ""),
                   "the shape has a distance already");
}

TEST_F(RecipeLoaderTest, TheScreenParallaxOfARecipeIsReadAndWhatIsWrongInItIsRefused)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    /* Without it, nothing which no matrix moves is moved. */
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_FALSE(recipe.screen_parallax.is_enabled);

    /* The angle of view is 90 degrees unless it is said. */
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"screen_parallax\": {\"convergence_distance\": 5},",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_TRUE    (recipe.screen_parallax.is_enabled);
    EXPECT_FLOAT_EQ(recipe.screen_parallax.convergence_distance,
                    5.0f);
    EXPECT_FLOAT_EQ(recipe.screen_parallax.field_of_view_degrees,
                    90.0f);

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"screen_parallax\": {\"convergence_distance\": 2.5, \"field_of_view_degrees\": 60},",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_FLOAT_EQ(recipe.screen_parallax.convergence_distance,
                    2.5f);
    EXPECT_FLOAT_EQ(recipe.screen_parallax.field_of_view_degrees,
                    60.0f);

    /* The distance cannot be found out, so it must be said. */
    expect_refused(get_recipe_text("\"screen_parallax\": {},",
                                   ""),
                   "screen_parallax.convergence_distance");
    expect_refused(get_recipe_text("\"screen_parallax\": {\"convergence_distance\": 0},",
                                   ""),
                   "screen_parallax.convergence_distance");
    expect_refused(get_recipe_text("\"screen_parallax\": {\"convergence_distance\": \"far\"},",
                                   ""),
                   "screen_parallax.convergence_distance");
    expect_refused(get_recipe_text("\"screen_parallax\": {\"convergence_distance\": 5, \"field_of_view_degrees\": 180},",
                                   ""),
                   "screen_parallax.field_of_view_degrees");
    expect_refused(get_recipe_text("\"screen_parallax\": {\"convergence_distance\": 5, \"field_of_view_degrees\": 0},",
                                   ""),
                   "screen_parallax.field_of_view_degrees");
    expect_refused(get_recipe_text("\"screen_parallax\": {\"convergence_distance\": 5, \"strength\": 1},",
                                   ""),
                   "strength");
    expect_refused(get_recipe_text("\"screen_parallax\": 5,",
                                   ""),
                   "screen_parallax");
}

TEST_F(RecipeLoaderTest, TheComfortOfARecipeIsReadAndWhatIsWrongInItIsRefused)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    /* Without it, there is no limit. */
    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_FLOAT_EQ(recipe.comfort.max_near_parallax,
                    0.0f);

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"comfort\": {\"max_near_parallax\": 0.03},",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;

    const float default_near_fraction = recipe.comfort.near_fraction;

    EXPECT_FLOAT_EQ(recipe.comfort.max_near_parallax,
                    0.03f);
    EXPECT_GT      (default_near_fraction,
                    0.0f);

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"comfort\": {\"max_near_parallax\": 0.05, \"near_fraction\": 0.01},",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_FLOAT_EQ(recipe.comfort.max_near_parallax,
                    0.05f);
    EXPECT_FLOAT_EQ(recipe.comfort.near_fraction,
                    0.01f);

    expect_refused(get_recipe_text("\"comfort\": {\"max_near_parallax\": 0},",
                                   ""),
                   "comfort.max_near_parallax");
    expect_refused(get_recipe_text("\"comfort\": {\"max_near_parallax\": 1.5},",
                                   ""),
                   "comfort.max_near_parallax");
    expect_refused(get_recipe_text("\"comfort\": {\"max_near_parallax\": \"a lot\"},",
                                   ""),
                   "comfort.max_near_parallax");
    expect_refused(get_recipe_text("\"comfort\": {\"near_fraction\": 1},",
                                   ""),
                   "comfort.near_fraction");
    expect_refused(get_recipe_text("\"comfort\": {\"near_fraction\": 0},",
                                   ""),
                   "comfort.near_fraction");
    expect_refused(get_recipe_text("\"comfort\": {\"strength\": 1},",
                                   ""),
                   "unknown field \"strength\"");
    expect_refused(get_recipe_text("\"comfort\": 3,",
                                   ""),
                   "comfort: expected an object");
}

TEST_F(RecipeLoaderTest, BakedSceneSeparationLimitsAreReadAndWhatIsWrongInThemIsRefused)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [],"
                                                                            " \"scene_max_separation\": [{\"shape\": \"0x00000000000000ab\", \"separation\": 0.25}]},",
                                                                            ""),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.scene_separation_vec.size(),
              1u);
    EXPECT_EQ(recipe.scene_separation_vec.at(0).shape_key,
              0xabu);
    EXPECT_FLOAT_EQ(recipe.scene_separation_vec.at(0).max_separation,
                    0.25f);
    EXPECT_TRUE(recipe.scene_convergence_vec.empty() );

    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"scene_max_separation\": [{\"shape\": \"0x0000000000000001\", \"distance\": 1}]},",
                                   ""),
                   "baked.scene_max_separation[0]: unknown field \"distance\"");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"scene_max_separation\": [{\"shape\": \"0x0000000000000001\", \"separation\": 1},"
                                   " {\"shape\": \"0x0000000000000001\", \"separation\": 2}]},",
                                   ""),
                   "the shape has a separation already");
}

TEST_F(RecipeLoaderTest, TheParallaxOfTheOverlayIsReadAndZeroSwitchesItOff)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_FLOAT_EQ(recipe.comfort.overlay_parallax,
                    0.0f);

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"comfort\": {\"overlay_parallax\": 0.04},",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_FLOAT_EQ(recipe.comfort.overlay_parallax,
                    0.04f);

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"comfort\": {\"overlay_parallax\": 0},",
                                                                             ""),
                                                           &recipe,
                                                           &error) ) << error;

    EXPECT_FLOAT_EQ(recipe.comfort.overlay_parallax,
                    0.0f);

    expect_refused(get_recipe_text("\"comfort\": {\"overlay_parallax\": -0.1},",
                                   ""),
                   "comfort.overlay_parallax");
    expect_refused(get_recipe_text("\"comfort\": {\"overlay_parallax\": 0.7},",
                                   ""),
                   "comfort.overlay_parallax");
    expect_refused(get_recipe_text("\"comfort\": {\"overlay_parallax\": true},",
                                   ""),
                   "comfort.overlay_parallax");
}

TEST_F(RecipeLoaderTest, BakedCoveringSignaturesAreRead)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [],"
                                                                            " \"covering_draws\": [\"0x00000000000000ab\", \"0x00000000000000cd\"]},",
                                                                            ""),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.covering_draw_key_vec.size(),
              2u);
    EXPECT_EQ(recipe.covering_draw_key_vec.at(0),
              0xabu);
    EXPECT_EQ(recipe.covering_draw_key_vec.at(1),
              0xcdu);
}

TEST_F(RecipeLoaderTest, PerEyeTexturesAreReadAndWhatIsWrongInThemIsRefused)
{
    std::string                    error;
    Analysis::RecipeLoader::Recipe recipe;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [],"
                                                                            " \"per_eye_textures\": [{\"description_key\": \"0x14779f78666e20cb\", \"ordinal\": 1}]},",
                                                                            ""),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.per_eye_texture_vec.size(),
              1u);
    EXPECT_EQ(recipe.per_eye_texture_vec.at(0).description_key,
              0x14779f78666e20cbull);
    EXPECT_EQ(recipe.per_eye_texture_vec.at(0).ordinal,
              1u);

    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"per_eye_textures\": [{\"description_key\": \"0x1\"}]},",
                                   ""),
                   "baked.per_eye_textures[0]: both the description_key and the ordinal are needed");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"per_eye_textures\": [{\"description_key\": \"0x0000000000000001\", \"ordinal\": 0, \"name\": \"x\"}]},",
                                   ""),
                   "baked.per_eye_textures[0]: unknown field \"name\"");
    expect_refused(get_recipe_text("\"baked\": {\"signature_scheme_version\": 1, \"draw_rules\": [], \"per_eye_textures\": [{\"description_key\": \"0x0000000000000001\", \"ordinal\": 0},"
                                   " {\"description_key\": \"0x0000000000000001\", \"ordinal\": 0}]},",
                                   ""),
                   "the texture is in the list already");
}

TEST_F(RecipeLoaderTest, ADrawInOnePassIsNamedByTheKeysOfBoth)
{
    Analysis::RecipeLoader::Recipe recipe;
    std::string                    error;

    ASSERT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"skipped\": {\"pass_draws\": [{\"pass_key\": \"0x0000000000000012\", \"draw_key\": \"0x0000000000000034\"}]},", ""),
                                                           &recipe,
                                                           &error) ) << error;

    ASSERT_EQ(recipe.skipped.pass_draw_vec.size(),
              1u);
    EXPECT_EQ(recipe.skipped.pass_draw_vec.at(0).pass_key,
              0x12u);
    EXPECT_EQ(recipe.skipped.pass_draw_vec.at(0).draw_key,
              0x34u);
    EXPECT_TRUE(recipe.skipped.draw_key_vec.empty() );
    EXPECT_TRUE(recipe.skipped.pass_key_vec.empty() );

    expect_refused(get_recipe_text("\"skipped\": {\"pass_draws\": []},", ""),
                   "skipped.pass_draws: the list is empty");
    expect_refused(get_recipe_text("\"skipped\": {\"pass_draws\": [{\"pass_key\": \"0x0000000000000012\"}]},", ""),
                   "needs both a pass_key and a draw_key");
    expect_refused(get_recipe_text("\"skipped\": {\"pass_draws\": [{\"pass_key\": \"0x0000000000000012\", \"draw_key\": \"0x0000000000000034\", \"x\": 1}]},", ""),
                   "unknown field \"x\"");
    expect_refused(get_recipe_text("\"skipped\": {\"pass_draws\": [\"0x0000000000000012\"]},", ""),
                   "expected an object with a pass_key and a draw_key");
    expect_refused(get_recipe_text("\"skipped\": {\"pass_draws\": {}},", ""),
                   "expected an array");

    /* The same draw in the same pass cannot be affected and unaffected; in different passes it can. */
    expect_refused(get_recipe_text("\"unaffected\": {\"pass_draws\": [{\"pass_key\": \"0x0000000000000012\", \"draw_key\": \"0x0000000000000034\"}]}, "
                                   "\"affected\": {\"pass_draws\": [{\"pass_key\": \"0x0000000000000012\", \"draw_key\": \"0x0000000000000034\"}]},", ""),
                   "a key is in both");
    EXPECT_TRUE(Analysis::RecipeLoader::RecipeLoader::parse(get_recipe_text("\"unaffected\": {\"pass_draws\": [{\"pass_key\": \"0x0000000000000012\", \"draw_key\": \"0x0000000000000034\"}]}, "
                                                                            "\"affected\": {\"pass_draws\": [{\"pass_key\": \"0x0000000000000056\", \"draw_key\": \"0x0000000000000034\"}]},", ""),
                                                           &recipe,
                                                           &error) ) << error;
}
