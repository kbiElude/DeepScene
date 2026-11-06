/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for FrameRewriter: the matrix of a call comes out of the rewrite as the rules say, for the eye, and the call as it was
 * is left alone.
 */
#include <gtest/gtest.h>
#include "interception/hooks/D3D8/types.h"
#include "frame_rewriter.h"
#include "map_chunk_source.h"
#include <string.h>

constexpr uint32_t MATRIX_CHUNK_ID = 3;

static std::array<float, 16> make_translation(const float& in_x)
{
    return {1,    0, 0, 0,
            0,    1, 0, 0,
            0,    0, 1, 0,
            in_x, 0, 0, 1};
}

static Capture::APIDumpLoader::DataChunk make_chunk(const std::array<float, 16>& in_matrix)
{
    Capture::APIDumpLoader::DataChunk result(sizeof(in_matrix) );

    ::memcpy(result.data(),
             in_matrix.data(),
             sizeof(in_matrix) );

    return result;
}

static std::array<float, 16> read_matrix(const Capture::APIDumpLoader::DataChunk& in_chunk)
{
    std::array<float, 16> result = {};

    ::memcpy(result.data(),
             in_chunk.data(),
             sizeof(result) );

    return result;
}

class FrameRewriterTest : public ::testing::Test
{
protected:
    /* A view transform with the matrix a translation of 10, and a rule that adds one to the left and takes one off the right. */
    void SetUp() override
    {
        Analysis::Recipe::ArgumentCondition condition;
        Analysis::Recipe::Rule              rule;

        condition.argument_index = 1;
        condition.value          = Interception::Hooks::D3D8::D3DTS_VIEW;

        rule.action_kind              = Analysis::Recipe::ActionKind::MULTIPLY;
        rule.argument_condition_vec.push_back(condition);
        rule.function                 = APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM;
        rule.left_matrix_fp32vec16    = make_translation(1.0f);
        rule.matrix_argument_index    = 2;
        rule.multiply_order           = Analysis::Recipe::MultiplyOrder::POST;
        rule.right_matrix_fp32vec16   = make_translation(-1.0f);

        m_recipe.rule_vec.push_back(rule);

        m_call.func = APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_SETTRANSFORM;

        m_call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr) );
        m_call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_u32(Interception::Hooks::D3D8::D3DTS_VIEW) );
        m_call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_data_chunk_id(MATRIX_CHUNK_ID) );

        m_call.thread_id     = 9;
        m_call.timestamp_qpc = 77;

        m_source.chunk_map[MATRIX_CHUNK_ID] = make_chunk(make_translation(10.0f) );
    }

    Capture::APIDumpLoader::DumpedAPICall m_call;
    Analysis::Recipe::Recipe              m_recipe;
    Tests::MapChunkSource                 m_source;
};

TEST_F(FrameRewriterTest, TheMatrixOfTheCallIsChangedForTheEye)
{
    const Analysis::Recipe::RecipeApplier   applier(m_recipe);
    Stereo::Replay::EngineDLL::OverlayChunkSource  left_overlay(m_source);
    Stereo::Replay::EngineDLL::OverlayChunkSource  right_overlay(m_source);
    const Stereo::Replay::EngineDLL::FrameRewriter rewriter(applier);
    Capture::APIDumpLoader::DumpedAPICall left_call;
    Capture::APIDumpLoader::DumpedAPICall right_call;

    ASSERT_TRUE(rewriter.rewrite(m_call,
                                 {},
                                 Analysis::Recipe::Eye::LEFT,
                                0.0f,
                                &left_overlay,
                                &left_call) );
    ASSERT_TRUE(rewriter.rewrite(m_call,
                                 {},
                                 Analysis::Recipe::Eye::RIGHT,
                                0.0f,
                                &right_overlay,
                                &right_call) );

    const auto* left_chunk_ptr  = left_overlay.get_data_chunk_ptr(left_call.arg_in_vec.at(2).get_data_chunk_id() );
    const auto* right_chunk_ptr = right_overlay.get_data_chunk_ptr(right_call.arg_in_vec.at(2).get_data_chunk_id() );

    ASSERT_NE(left_chunk_ptr,
              nullptr);
    ASSERT_NE(right_chunk_ptr,
              nullptr);
    EXPECT_FLOAT_EQ(read_matrix(*left_chunk_ptr).at(12),
                    11.0f);
    EXPECT_FLOAT_EQ(read_matrix(*right_chunk_ptr).at(12),
                    9.0f);
}

TEST_F(FrameRewriterTest, TheCallIsCopiedWithOnlyTheMatrixArgumentChanged)
{
    const Analysis::Recipe::RecipeApplier   applier(m_recipe);
    Stereo::Replay::EngineDLL::OverlayChunkSource  overlay(m_source);
    const Stereo::Replay::EngineDLL::FrameRewriter rewriter(applier);
    Capture::APIDumpLoader::DumpedAPICall          rewritten;

    ASSERT_TRUE(rewriter.rewrite(m_call,
                                 {},
                                 Analysis::Recipe::Eye::LEFT,
                                0.0f,
                                &overlay,
                                &rewritten) );

    EXPECT_EQ(rewritten.func,
              m_call.func);
    EXPECT_EQ(rewritten.thread_id,
              9u);
    EXPECT_EQ(rewritten.timestamp_qpc,
              77u);
    ASSERT_EQ(rewritten.arg_in_vec.size(),
              3u);
    EXPECT_EQ(rewritten.arg_in_vec.at(1).get_u32(),
              static_cast<uint32_t>(Interception::Hooks::D3D8::D3DTS_VIEW) );
    EXPECT_NE(rewritten.arg_in_vec.at(2).get_data_chunk_id(),
              MATRIX_CHUNK_ID);
}

TEST_F(FrameRewriterTest, WhatWasPlayedIsLeftAsItWas)
{
    const Analysis::Recipe::RecipeApplier   applier(m_recipe);
    Stereo::Replay::EngineDLL::OverlayChunkSource  overlay(m_source);
    const Stereo::Replay::EngineDLL::FrameRewriter rewriter(applier);
    Capture::APIDumpLoader::DumpedAPICall          rewritten;

    ASSERT_TRUE(rewriter.rewrite(m_call,
                                 {},
                                 Analysis::Recipe::Eye::LEFT,
                                0.0f,
                                &overlay,
                                &rewritten) );

    EXPECT_EQ(m_call.arg_in_vec.at(2).get_data_chunk_id(),
              MATRIX_CHUNK_ID);
    EXPECT_FLOAT_EQ(read_matrix(*m_source.get_data_chunk_ptr(MATRIX_CHUNK_ID) ).at(12),
                    10.0f);

    /* Through the overlay, the old chunk is still there. */
    EXPECT_FLOAT_EQ(read_matrix(*overlay.get_data_chunk_ptr(MATRIX_CHUNK_ID) ).at(12),
                    10.0f);
}

TEST_F(FrameRewriterTest, ACallNoRuleIsAboutIsNotRewritten)
{
    const Analysis::Recipe::RecipeApplier   applier(m_recipe);
    Stereo::Replay::EngineDLL::OverlayChunkSource  overlay(m_source);
    const Stereo::Replay::EngineDLL::FrameRewriter rewriter(applier);
    Capture::APIDumpLoader::DumpedAPICall          rewritten;

    m_call.arg_in_vec.at(1) = APIInterceptor::APIFunctionArgument::create_u32(Interception::Hooks::D3D8::D3DTS_PROJECTION);

    EXPECT_FALSE(rewriter.rewrite(m_call,
                                  {},
                                  Analysis::Recipe::Eye::LEFT,
                                 0.0f,
                                 &overlay,
                                 &rewritten) );
}

TEST_F(FrameRewriterTest, OnlyACallARuleIsAboutHasAMatchingRule)
{
    const Analysis::Recipe::RecipeApplier   applier(m_recipe);
    const Stereo::Replay::EngineDLL::FrameRewriter rewriter(applier);

    EXPECT_TRUE(rewriter.has_matching_rule(m_call,
                                           {}) );

    m_call.arg_in_vec.at(1) = APIInterceptor::APIFunctionArgument::create_u32(Interception::Hooks::D3D8::D3DTS_PROJECTION);

    EXPECT_FALSE(rewriter.has_matching_rule(m_call,
                                            {}) );
}

TEST_F(FrameRewriterTest, ACallWhoseMatrixIsNotThereOrNotAMatrixIsNotRewritten)
{
    const Analysis::Recipe::RecipeApplier   applier(m_recipe);
    Stereo::Replay::EngineDLL::OverlayChunkSource  overlay(m_source);
    const Stereo::Replay::EngineDLL::FrameRewriter rewriter(applier);
    Capture::APIDumpLoader::DumpedAPICall          rewritten;

    m_source.chunk_map[MATRIX_CHUNK_ID] = Capture::APIDumpLoader::DataChunk(12);

    EXPECT_FALSE(rewriter.rewrite(m_call,
                                  {},
                                  Analysis::Recipe::Eye::LEFT,
                                 0.0f,
                                 &overlay,
                                 &rewritten) );

    m_source.chunk_map.clear();

    EXPECT_FALSE(rewriter.rewrite(m_call,
                                  {},
                                  Analysis::Recipe::Eye::LEFT,
                                 0.0f,
                                 &overlay,
                                 &rewritten) );
}

TEST_F(FrameRewriterTest, ARuleForAPassRewritesOnlyTheCallsOfThatPass)
{
    m_recipe.rule_vec.at(0).pass_key_vec = {0xabc};

    const Analysis::Recipe::RecipeApplier   applier(m_recipe);
    Stereo::Replay::EngineDLL::OverlayChunkSource  overlay(m_source);
    const Stereo::Replay::EngineDLL::FrameRewriter rewriter(applier);
    Capture::APIDumpLoader::DumpedAPICall rewritten;
    Analysis::Recipe::CallPlace           place;

    place.has_pass = true;
    place.pass_key = 0xdef;

    EXPECT_FALSE(rewriter.rewrite(m_call,
                                  place,
                                  Analysis::Recipe::Eye::LEFT,
                                 0.0f,
                                 &overlay,
                                 &rewritten) );

    place.pass_key = 0xabc;

    EXPECT_TRUE(rewriter.rewrite(m_call,
                                 place,
                                 Analysis::Recipe::Eye::LEFT,
                                0.0f,
                                &overlay,
                                &rewritten) );
}

TEST_F(FrameRewriterTest, ARuleForAKindOfMatrixLeavesTheCallAloneIfItIsAnother)
{
    m_recipe.rule_vec.at(0).matrix_kind = Analysis::Recipe::MatrixKind::PERSPECTIVE;

    const Analysis::Recipe::RecipeApplier   applier(m_recipe);
    Stereo::Replay::EngineDLL::OverlayChunkSource  overlay(m_source);
    const Stereo::Replay::EngineDLL::FrameRewriter rewriter(applier);
    Capture::APIDumpLoader::DumpedAPICall          rewritten;

    /* A translation is not a perspective projection. */
    EXPECT_FALSE(rewriter.rewrite(m_call,
                                  {},
                                  Analysis::Recipe::Eye::LEFT,
                                 0.0f,
                                 &overlay,
                                 &rewritten) );
}

TEST_F(FrameRewriterTest, RulesApplyOneAfterTheOtherToTheMatrixTheLastLeft)
{
    m_recipe.rule_vec.push_back(m_recipe.rule_vec.at(0) );

    const Analysis::Recipe::RecipeApplier   applier(m_recipe);
    Stereo::Replay::EngineDLL::OverlayChunkSource  overlay(m_source);
    const Stereo::Replay::EngineDLL::FrameRewriter rewriter(applier);
    Capture::APIDumpLoader::DumpedAPICall          rewritten;

    ASSERT_TRUE(rewriter.rewrite(m_call,
                                 {},
                                 Analysis::Recipe::Eye::LEFT,
                                0.0f,
                                &overlay,
                                &rewritten) );
    EXPECT_FLOAT_EQ(read_matrix(*overlay.get_data_chunk_ptr(rewritten.arg_in_vec.at(2).get_data_chunk_id() ) ).at(12),
                    12.0f);
}
