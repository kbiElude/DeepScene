/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for EyeMatrixBinder: a call that sets a matrix is executed where a draw needs it, as the application made it for a draw the
 * recipe leaves alone and as the recipe changes it for the others, and only when the next draw needs the other one.
 */
#include <gtest/gtest.h>
#include "Interception/Hooks/D3D8/types.h"
#include "eye_matrix_binder.h"
#include "test_doubles.h"
#include <string.h>

constexpr uint32_t MATRIX_CHUNK_ID = 3;

class EyeMatrixBinderTest : public ::testing::Test
{
protected:
    /* What was executed: the state the call was for, and the x of the translation of its matrix. */
    struct Executed
    {
        float    translation_x;
        uint32_t state;

        bool operator==(const Executed& in_other) const
        {
            return (translation_x == in_other.translation_x &&
                    state         == in_other.state);
        }
    };

    /* A rule which adds 1 to the x of the translation of the view for the left eye and takes 1 off for the right one, and the
     * same for the projection.
     */
    void SetUp() override
    {
        for (const auto state : {Interception::Hooks::D3D8::D3DTS_VIEW, Interception::Hooks::D3D8::D3DTS_PROJECTION})
        {
            Analysis::RecipeLoader::ArgumentCondition condition;
            Analysis::RecipeLoader::Rule              rule;

            condition.argument_index = 1;
            condition.value          = state;

            rule.action_kind            = Analysis::RecipeLoader::ActionKind::MULTIPLY;
            rule.function               = APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM;
            rule.left_matrix_fp32vec16  = make_translation(1.0f);
            rule.matrix_argument_index  = 2;
            rule.multiply_order         = Analysis::RecipeLoader::MultiplyOrder::POST;
            rule.right_matrix_fp32vec16 = make_translation(-1.0f);

            rule.argument_condition_vec.push_back(condition);

            m_recipe.rule_vec.push_back(rule);
        }

        m_applier_ptr.reset(new Analysis::RecipeApplier::RecipeApplier(m_recipe) );
        m_rewriter_ptr.reset(new Stereo::Replay::EngineDLL::FrameRewriter(*m_applier_ptr) );
        m_binder_ptr.reset(new Stereo::Replay::EngineDLL::EyeMatrixBinder(*m_rewriter_ptr,
                                                                          [this](const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                                                 const Stereo::Replay::EngineDLL::IChunkSource& in_chunk_source)
                                                                          {
                                                                              Executed executed = {};

                                                                              executed.state = in_call.arg_in_vec.at(1).get_u32();

                                                                              const auto* chunk_ptr = in_chunk_source.get_data_chunk_ptr(in_call.arg_in_vec.at(2).get_data_chunk_id() );

                                                                              if (chunk_ptr != nullptr)
                                                                              {
                                                                                  float matrix[16];

                                                                                  ::memcpy(matrix,
                                                                                           chunk_ptr->data(),
                                                                                           sizeof(matrix) );

                                                                                  executed.translation_x = matrix[12];
                                                                              }

                                                                              m_executed_vec.push_back(executed);

                                                                              return m_execute_succeeds;
                                                                          }) );
    }

    static std::array<float, 16> make_translation(const float& in_x)
    {
        return {1,    0, 0, 0,
                0,    1, 0, 0,
                0,    0, 1, 0,
                in_x, 0, 0, 1};
    }

    /* A SetTransform of the state with a matrix whose translation is x; the call is taken or not by the binder. */
    bool set_transform(const Interception::Hooks::D3D8::D3DTRANSFORMSTATETYPE& in_state,
                       const float&                              in_x,
                       const Analysis::RecipeLoader::Eye& in_eye            = Analysis::RecipeLoader::Eye::LEFT,
                       const float&                       in_eye_separation = 0.0f)
    {
        Capture::APIDumpLoader::DumpedAPICall call;
        const auto                            matrix = make_translation(in_x);
        Capture::APIDumpLoader::DataChunk     chunk(sizeof(matrix) );

        ::memcpy(chunk.data(),
                 matrix.data(),
                 sizeof(matrix) );

        m_source.chunk_map[MATRIX_CHUNK_ID] = chunk;

        call.func = APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM;

        call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr) );
        call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(in_state) );
        call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_data_chunk_id(MATRIX_CHUNK_ID) );

        return m_binder_ptr->on_call(call,
                                     m_source,
                                     {},
                                     in_eye,
                                     in_eye_separation);
    }

    std::vector<Executed> m_executed_vec;
    bool                  m_execute_succeeds = true;
    Tests::MapChunkSource       m_source;

    /* Each one refers to the one before it, so they are made and destroyed in this order. */
    Analysis::RecipeLoader::Recipe                              m_recipe;
    std::unique_ptr<Analysis::RecipeApplier::RecipeApplier>     m_applier_ptr;
    std::unique_ptr<Stereo::Replay::EngineDLL::FrameRewriter>   m_rewriter_ptr;
    std::unique_ptr<Stereo::Replay::EngineDLL::EyeMatrixBinder> m_binder_ptr;
};

TEST_F(EyeMatrixBinderTest, ACallNoRuleIsAboutIsNotTaken)
{
    Capture::APIDumpLoader::DumpedAPICall call;

    call.func = APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETMATERIAL;

    EXPECT_FALSE(m_binder_ptr->on_call(call,
                                       m_source,
                                       {},
                                       Analysis::RecipeLoader::Eye::LEFT,
                                          0.0f) );
}

TEST_F(EyeMatrixBinderTest, ACallWhichIsTakenIsNotExecutedUntilADrawNeedsIt)
{
    EXPECT_TRUE(set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                              10.0f) );
    EXPECT_TRUE(m_executed_vec.empty() );

    ASSERT_TRUE(m_binder_ptr->before_draw(true, /* in_is_affected */
                                          Analysis::RecipeLoader::Eye::LEFT,
                                          0.0f) );

    /* The left eye: 10 + 1. */
    EXPECT_EQ(m_executed_vec,
              (std::vector<Executed>{{11.0f, Interception::Hooks::D3D8::D3DTS_VIEW}}) );
}

TEST_F(EyeMatrixBinderTest, ADrawTheRecipeLeavesAloneGetsTheMatrixAsTheApplicationMadeIt)
{
    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  10.0f);

    ASSERT_TRUE(m_binder_ptr->before_draw(false, /* in_is_affected */
                                          Analysis::RecipeLoader::Eye::RIGHT,
                                          0.0f) );

    EXPECT_EQ(m_executed_vec,
              (std::vector<Executed>{{10.0f, Interception::Hooks::D3D8::D3DTS_VIEW}}) );
}

TEST_F(EyeMatrixBinderTest, ACallIsExecutedAgainOnlyWhenTheNextDrawNeedsTheOtherMatrix)
{
    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  10.0f);

    for (const bool is_affected : {true, true, false, false, true})
    {
        ASSERT_TRUE(m_binder_ptr->before_draw(is_affected,
                                              Analysis::RecipeLoader::Eye::LEFT,
                                          0.0f) );
    }

    EXPECT_EQ(m_executed_vec,
              (std::vector<Executed>{{11.0f, Interception::Hooks::D3D8::D3DTS_VIEW},
                                     {10.0f, Interception::Hooks::D3D8::D3DTS_VIEW},
                                     {11.0f, Interception::Hooks::D3D8::D3DTS_VIEW}}) );
}

TEST_F(EyeMatrixBinderTest, EveryMatrixTheDrawNeedsIsExecuted)
{
    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  10.0f);
    set_transform(Interception::Hooks::D3D8::D3DTS_PROJECTION,
                  20.0f);

    ASSERT_TRUE(m_binder_ptr->before_draw(true, /* in_is_affected */
                                          Analysis::RecipeLoader::Eye::RIGHT,
                                          0.0f) );

    ASSERT_EQ(m_executed_vec.size(),
              2u);

    /* The right eye: 1 less. In no particular order. */
    const bool is_view_first = (m_executed_vec.at(0).state == Interception::Hooks::D3D8::D3DTS_VIEW);
    const auto view          = m_executed_vec.at(is_view_first ? 0 : 1);
    const auto projection    = m_executed_vec.at(is_view_first ? 1 : 0);

    EXPECT_EQ(view.translation_x,
              9.0f);
    EXPECT_EQ(projection.state,
              static_cast<uint32_t>(Interception::Hooks::D3D8::D3DTS_PROJECTION) );
    EXPECT_EQ(projection.translation_x,
              19.0f);
}

TEST_F(EyeMatrixBinderTest, WhatWasKeptServesTheOtherEyeWithoutTheApplicationSettingItAgain)
{
    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  10.0f);

    ASSERT_TRUE(m_binder_ptr->before_draw(true, /* in_is_affected */
                                          Analysis::RecipeLoader::Eye::LEFT,
                                          0.0f) );
    ASSERT_TRUE(m_binder_ptr->before_draw(true, /* in_is_affected */
                                          Analysis::RecipeLoader::Eye::RIGHT,
                                          0.0f) );

    EXPECT_EQ(m_executed_vec,
              (std::vector<Executed>{{11.0f, Interception::Hooks::D3D8::D3DTS_VIEW},
                                     {9.0f,  Interception::Hooks::D3D8::D3DTS_VIEW}}) );
}

TEST_F(EyeMatrixBinderTest, ANewCallForTheSameMatrixReplacesTheOneKept)
{
    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  10.0f);
    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  30.0f);

    ASSERT_TRUE(m_binder_ptr->before_draw(true, /* in_is_affected */
                                          Analysis::RecipeLoader::Eye::LEFT,
                                          0.0f) );

    EXPECT_EQ(m_executed_vec,
              (std::vector<Executed>{{31.0f, Interception::Hooks::D3D8::D3DTS_VIEW}}) );
}

TEST_F(EyeMatrixBinderTest, ACallTheRecipeDoesNotChangeIsLeftToTheCallerAndDropsTheOneKept)
{
    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  10.0f);

    /* From now on the rule is about perspective matrices only: a translation is not one. */
    m_recipe.rule_vec.at(0).matrix_kind = Analysis::RecipeLoader::MatrixKind::PERSPECTIVE;

    m_applier_ptr.reset(new Analysis::RecipeApplier::RecipeApplier(m_recipe) );
    m_rewriter_ptr.reset(new Stereo::Replay::EngineDLL::FrameRewriter(*m_applier_ptr) );
    m_binder_ptr.reset(new Stereo::Replay::EngineDLL::EyeMatrixBinder(*m_rewriter_ptr,
                                                                      [this](const Capture::APIDumpLoader::DumpedAPICall&,
                                                                             const Stereo::Replay::EngineDLL::IChunkSource&)
                                                                      {
                                                                          m_executed_vec.push_back({});

                                                                          return true;
                                                                      }) );

    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  10.0f);

    EXPECT_FALSE(set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                               20.0f) );

    ASSERT_TRUE(m_binder_ptr->before_draw(true, /* in_is_affected */
                                          Analysis::RecipeLoader::Eye::LEFT,
                                          0.0f) );

    EXPECT_TRUE(m_executed_vec.empty() );
}

TEST_F(EyeMatrixBinderTest, ForgettingDropsWhatWasKept)
{
    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  10.0f);

    m_binder_ptr->forget();

    ASSERT_TRUE(m_binder_ptr->before_draw(true, /* in_is_affected */
                                          Analysis::RecipeLoader::Eye::LEFT,
                                          0.0f) );

    EXPECT_TRUE(m_executed_vec.empty() );
}

TEST_F(EyeMatrixBinderTest, ACallWhichFailsFailsTheDraw)
{
    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  10.0f);

    m_execute_succeeds = false;

    EXPECT_FALSE(m_binder_ptr->before_draw(true, /* in_is_affected */
                                           Analysis::RecipeLoader::Eye::LEFT,
                                          0.0f) );
}

TEST_F(EyeMatrixBinderTest, AMatrixMadeForAnotherSeparationIsMadeAgainWhenTheSeparationChanges)
{
    /* The view moves by half of the separation: the rule of the fixture is replaced by an eye offset. */
    Analysis::RecipeLoader::Rule rule = m_recipe.rule_vec.at(0);

    rule.action_kind = Analysis::RecipeLoader::ActionKind::EYE_OFFSET;
    rule.offset_axis = Analysis::RecipeLoader::Axis::X;

    m_recipe.rule_vec.clear();
    m_recipe.rule_vec.push_back(rule);

    m_applier_ptr.reset(new Analysis::RecipeApplier::RecipeApplier(m_recipe) );
    m_rewriter_ptr.reset(new Stereo::Replay::EngineDLL::FrameRewriter(*m_applier_ptr) );
    m_binder_ptr.reset(new Stereo::Replay::EngineDLL::EyeMatrixBinder(*m_rewriter_ptr,
                                                                      [this](const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                                             const Stereo::Replay::EngineDLL::IChunkSource& in_chunk_source)
                                                                      {
                                                                          float matrix[16];

                                                                          const auto* chunk_ptr = in_chunk_source.get_data_chunk_ptr(in_call.arg_in_vec.at(2).get_data_chunk_id() );

                                                                          ::memcpy(matrix,
                                                                                   chunk_ptr->data(),
                                                                                   sizeof(matrix) );

                                                                          m_executed_vec.push_back({matrix[12], Interception::Hooks::D3D8::D3DTS_VIEW});

                                                                          return true;
                                                                      }) );

    set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                  10.0f,
                  Analysis::RecipeLoader::Eye::LEFT,
                  0.2f);

    const auto before_draw = [this](const bool& in_is_affected,
                                    const float& in_eye_separation)
    {
        return m_binder_ptr->before_draw(in_is_affected,
                                         Analysis::RecipeLoader::Eye::LEFT,
                                         in_eye_separation);
    };

    ASSERT_TRUE(before_draw(true, 0.2f) );
    ASSERT_TRUE(before_draw(true, 0.2f) );
    ASSERT_TRUE(before_draw(true, 0.4f) );
    ASSERT_TRUE(before_draw(true, 0.4f) );
    ASSERT_TRUE(before_draw(false, 0.4f) );
    ASSERT_TRUE(before_draw(true, 0.4f) );

    ASSERT_EQ(m_executed_vec.size(),
              4u);

    EXPECT_FLOAT_EQ(m_executed_vec.at(0).translation_x,
                    10.1f);
    EXPECT_FLOAT_EQ(m_executed_vec.at(1).translation_x,
                    10.2f);
    EXPECT_FLOAT_EQ(m_executed_vec.at(2).translation_x,
                    10.0f);
    EXPECT_FLOAT_EQ(m_executed_vec.at(3).translation_x,
                    10.2f);
}
