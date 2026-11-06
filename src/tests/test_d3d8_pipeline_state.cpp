/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the key of the state of a D3D8 draw: what tells draws apart, and what does not.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/d3d8_pipeline_state.h"
#include "Interception/Hooks/D3D8/types.h"

TEST(D3D8PipelineState, StateKeyOfNothingSetIsStable)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState a;
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState b;

    EXPECT_EQ(a.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key,
              b.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key);
}

TEST(D3D8PipelineState, AStateThatChoosesHowThingsAreDoneChangesTheKey)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState state;

    const auto key_before = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    state.get_fixed_function_state().set_render_state(Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE,
                                                      1);

    const auto key_blend_on = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    state.get_fixed_function_state().set_render_state(Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE,
                                                      0);

    const auto key_blend_off = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    EXPECT_NE(key_before,
              key_blend_on);
    EXPECT_NE(key_blend_on,
              key_blend_off);
    EXPECT_NE(key_before,
              key_blend_off);
}

TEST(D3D8PipelineState, AStateThatIsANumberIsInTheKeyOnlyForBeingSet)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState state;

    const auto key_before = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    state.get_fixed_function_state().set_render_state(Interception::Hooks::D3D8::D3DRS_TEXTUREFACTOR,
                                                      0xFF112233);

    const auto key_first = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    state.get_fixed_function_state().set_render_state(Interception::Hooks::D3D8::D3DRS_TEXTUREFACTOR,
                                                      0xFF445566);

    const auto key_second = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    EXPECT_NE(key_before,
              key_first);
    EXPECT_EQ(key_first,
              key_second);
}

TEST(D3D8PipelineState, TextureStageStatesFollowTheSameRules)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState state;

    state.get_fixed_function_state().set_texture_stage_state(0,
                                                             Interception::Hooks::D3D8::D3DTSS_COLOROP,
                                                             4);

    const auto key_op_4 = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    state.get_fixed_function_state().set_texture_stage_state(0,
                                                             Interception::Hooks::D3D8::D3DTSS_COLOROP,
                                                             7);

    const auto key_op_7 = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    EXPECT_NE(key_op_4,
              key_op_7);

    state.get_fixed_function_state().set_texture_stage_state(1,
                                                             Interception::Hooks::D3D8::D3DTSS_BORDERCOLOR,
                                                             0x11111111);

    const auto key_border_a = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    state.get_fixed_function_state().set_texture_stage_state(1,
                                                             Interception::Hooks::D3D8::D3DTSS_BORDERCOLOR,
                                                             0x22222222);

    EXPECT_EQ(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key,
              key_border_a);

    /* The same state of another stage is another state. */
    state.get_fixed_function_state().set_texture_stage_state(1,
                                                             Interception::Hooks::D3D8::D3DTSS_COLOROP,
                                                             7);

    EXPECT_NE(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key,
              key_border_a);
}

TEST(D3D8PipelineState, LightsCountByIndexAndTypeWhenTheyAreOn)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState state;

    const auto key_no_light = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    state.get_fixed_function_state().set_light(0,
                                               3);

    /* A light that is off does not matter. */
    EXPECT_EQ(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key,
              key_no_light);

    state.get_fixed_function_state().set_light_enable(0,
                                                      true);

    const auto key_directional = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    state.get_fixed_function_state().set_light(0,
                                               1);

    const auto key_point = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    state.get_fixed_function_state().set_light_enable(0,
                                                      false);

    EXPECT_NE(key_no_light,
              key_directional);
    EXPECT_NE(key_directional,
              key_point);
    EXPECT_EQ(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key,
              key_no_light);
}

TEST(D3D8PipelineState, ADrawIsDescribedByWhatIsBound)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState state;

    state.set_index_buffer(Analysis::APIDumpAnalysis::make_resource_id(9) );
    state.set_stream_source(0,
                            Analysis::APIDumpAnalysis::make_resource_id(7),
                            36);
    state.get_fixed_function_state().set_texture(2,
                                                 {0, 0, Analysis::APIDumpAnalysis::make_resource_id(5)});
    state.get_fixed_function_state().set_texture(0,
                                                 {0, 1, Analysis::APIDumpAnalysis::make_resource_id(4)});

    const auto indexed = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_STRIP, 6, true);

    EXPECT_TRUE(indexed.is_indexed);
    EXPECT_EQ  (indexed.index_buffer_id,
                Analysis::APIDumpAnalysis::make_resource_id(9) );
    EXPECT_EQ  (indexed.primitive_topology,
                Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_STRIP);
    EXPECT_EQ  (indexed.vertex_count,
                6u);
    ASSERT_EQ  (indexed.vertex_buffer_id_vec.size(),
                1u);
    EXPECT_EQ  (indexed.vertex_buffer_id_vec.at(0),
                Analysis::APIDumpAnalysis::make_resource_id(7) );

    /* Textures come in the order of their stages. */
    ASSERT_EQ(indexed.texture_binding_vec.size(),
              2u);
    EXPECT_EQ(indexed.texture_binding_vec.at(0).binding_point.index,
              0u);
    EXPECT_EQ(indexed.texture_binding_vec.at(0).view.mip_level,
              1u);
    EXPECT_EQ(indexed.texture_binding_vec.at(1).binding_point.index,
              2u);

    /* No indices are used for a draw that is not indexed. */
    EXPECT_EQ(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).index_buffer_id,
              Analysis::APIDumpAnalysis::ResourceId::NONE);
}

TEST(D3D8PipelineState, UnbindingTakesAThingOut)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState state;

    state.set_stream_source(0,
                            Analysis::APIDumpAnalysis::make_resource_id(7),
                            36);
    state.get_fixed_function_state().set_texture(1,
                                                 {0, 0, Analysis::APIDumpAnalysis::make_resource_id(5)});
    state.set_stream_source(0,
                            Analysis::APIDumpAnalysis::ResourceId::NONE,
                            0);
    state.get_fixed_function_state().set_texture(1,
                                                 Analysis::APIDumpAnalysis::ResourceView() );

    const auto draw = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false);

    EXPECT_TRUE(draw.vertex_buffer_id_vec.empty() );
    EXPECT_TRUE(draw.texture_binding_vec.empty() );
}

TEST(D3D8PipelineState, TheVertexLayoutFollowsTheShaderAndTheStrides)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState state;

    state.set_vertex_shader(0x152);
    state.set_stream_source(0,
                            Analysis::APIDumpAnalysis::make_resource_id(1),
                            36);

    const auto layout_a = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).vertex_layout_key;

    state.set_stream_source(0,
                            Analysis::APIDumpAnalysis::make_resource_id(2),
                            36);

    /* Another buffer of the same layout. */
    EXPECT_EQ(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).vertex_layout_key,
              layout_a);

    state.set_stream_source(0,
                            Analysis::APIDumpAnalysis::make_resource_id(2),
                            24);

    EXPECT_NE(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).vertex_layout_key,
              layout_a);

    state.set_stream_source(0,
                            Analysis::APIDumpAnalysis::make_resource_id(2),
                            36);
    state.set_vertex_shader(0x2C2);

    EXPECT_NE(state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).vertex_layout_key,
              layout_a);
}

TEST(D3D8PipelineState, ADrawReadsWhatItRendersToWhenItBlendsOrTests)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState state;

    /* Direct3D starts with blending off, and the depth test on. */
    auto draw = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false);

    EXPECT_FALSE(draw.reads_color_attachments);
    EXPECT_TRUE (draw.reads_depth_stencil);

    state.get_fixed_function_state().set_render_state(Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE,
                                                      1);
    state.get_fixed_function_state().set_render_state(Interception::Hooks::D3D8::D3DRS_ZENABLE,
                                                      0);

    draw = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false);

    EXPECT_TRUE (draw.reads_color_attachments);
    EXPECT_FALSE(draw.reads_depth_stencil);

    state.get_fixed_function_state().set_render_state(Interception::Hooks::D3D8::D3DRS_STENCILENABLE,
                                                      1);

    draw = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false);

    EXPECT_TRUE(draw.reads_depth_stencil);

    state.get_fixed_function_state().set_render_state(Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE,
                                                      0);

    draw = state.describe_user_pointer_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false, 12);

    EXPECT_FALSE(draw.reads_color_attachments);
}

TEST(D3D8PipelineState, AnIndexNoDeviceHasIsRefusedAndChangesNothing)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState state;
    Analysis::APIDumpAnalysis::ResourceView                 view;

    view.resource_id = Analysis::APIDumpAnalysis::make_resource_id(1);

    const auto key_before = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key;

    EXPECT_FALSE(state.get_fixed_function_state().set_render_state(0xFFFFFFFF,
                                                                   1) );
    EXPECT_FALSE(state.get_fixed_function_state().set_texture_stage_state(0,
                                                                          0xFFFFFFFF,
                                                                          1) );
    EXPECT_FALSE(state.get_fixed_function_state().set_texture_stage_state(1000000,
                                                                          1,
                                                                          1) );
    EXPECT_FALSE(state.get_fixed_function_state().set_light(0xFFFFFFFF,
                                                            1) );
    EXPECT_FALSE(state.get_fixed_function_state().set_light_enable(0xFFFFFFFF,
                                                                   true) );
    EXPECT_FALSE(state.get_fixed_function_state().set_texture(1000000,
                                                              view) );
    EXPECT_FALSE(state.set_stream_source(1000000,
                                         view.resource_id,
                                         32) );

    const auto draw = state.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false);

    EXPECT_EQ(draw.state_key,
              key_before);
    EXPECT_TRUE(draw.texture_binding_vec.empty() );
    EXPECT_TRUE(draw.vertex_buffer_id_vec.empty() );
}

TEST(D3D8PipelineState, TheIndexesOfAnOrdinaryDeviceAreAccepted)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState state;
    Analysis::APIDumpAnalysis::ResourceView                 view;

    view.resource_id = Analysis::APIDumpAnalysis::make_resource_id(1);

    EXPECT_TRUE(state.get_fixed_function_state().set_render_state(Interception::Hooks::D3D8::D3DRS_ZENABLE,
                                                                  1) );
    EXPECT_TRUE(state.get_fixed_function_state().set_texture_stage_state(7,
                                                                         Interception::Hooks::D3D8::D3DTSS_COLOROP,
                                                                         1) );
    EXPECT_TRUE(state.get_fixed_function_state().set_light(7,
                                                           1) );
    EXPECT_TRUE(state.get_fixed_function_state().set_light_enable(7,
                                                                  true) );
    EXPECT_TRUE(state.get_fixed_function_state().set_texture(7,
                                                             view) );
    EXPECT_TRUE(state.set_stream_source(15,
                                        view.resource_id,
                                        32) );
}

TEST(D3D8PipelineState, ResettingGivesTheStateOfANewDevice)
{
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState used;
    Analysis::APIDumpAnalysis::Frontends::D3D8PipelineState fresh;

    used.get_fixed_function_state().set_render_state(Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE,
                                                     1);
    used.get_fixed_function_state().set_light_enable(2,
                                                     true);
    used.reset();

    EXPECT_EQ(used.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key,
              fresh.describe_draw(Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST, 3, false).state_key);
}
