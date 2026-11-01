/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the D3D8 front end following shaders, draws from the application's memory, resets, and what it does not follow.
 */
#include <gtest/gtest.h>
#include "d3d8_scene.h"
#include <set>

TEST(D3D8FrontendDraws, AVertexShaderIsAShaderResource)
{
    Tests::D3D8CallBuilder     builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    builder.create_vertex_shader({1, 2, 3, 4},
                                 {5, 6, 7, 8, 9, 10});
    harness.feed(builder);

    const auto& resource = harness.get_model().resource_vec.at(2);

    EXPECT_EQ  (resource.description.kind,
                Analysis::APIDumpAnalysis::ResourceKind::SHADER);
    EXPECT_EQ  (resource.description.shader_stage,
                Analysis::APIDumpAnalysis::ShaderStage::VERTEX);
    EXPECT_TRUE(resource.description.has_content_hash);
    EXPECT_EQ  (resource.description.size_in_bytes,
                6u);
}

TEST(D3D8FrontendDraws, ShadersWithTheSameCodeAreTheSameWhateverTheirHandles)
{
    Tests::D3D8CallBuilder     builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    builder.create_vertex_shader({1, 2, 3, 4}, {5, 6});
    builder.create_vertex_shader({1, 2, 3, 4}, {5, 6});
    builder.create_vertex_shader({1, 2, 3, 4}, {5, 7});
    harness.feed(builder);

    const auto& resource_vec = harness.get_model().resource_vec;

    EXPECT_EQ(resource_vec.at(2).match_key,
              resource_vec.at(3).match_key);
    EXPECT_NE(resource_vec.at(2).match_key,
              resource_vec.at(4).match_key);
}

TEST(D3D8FrontendDraws, ADrawWithAShaderIsOfAnotherKindThanOneWithoutIt)
{
    Tests::D3D8Scene scene;

    const uint32_t vertex_shader = scene.get_builder().create_vertex_shader({1, 2, 3, 4}, {5, 6});
    const uint32_t pixel_shader  = scene.get_builder().create_pixel_shader ({7, 8});

    scene.bind_back_buffer();
    scene.draw(1);

    scene.get_builder().set_vertex_shader(vertex_shader);
    scene.draw(1);

    scene.get_builder().set_pixel_shader(pixel_shader);
    scene.draw(1);
    scene.present();

    const auto& model = scene.get_harness().get_model();

    EXPECT_EQ(model.draw_structure_map.size(),
              3u);

    uint32_t n_with_one_shader  = 0;
    uint32_t n_with_two_shaders = 0;
    uint32_t n_with_no_shader   = 0;

    for (const auto& draw_pair : model.draw_structure_map)
    {
        switch (draw_pair.second.shader_key_vec.size() )
        {
            case 0:  n_with_no_shader++;   break;
            case 1:  n_with_one_shader++;  break;
            case 2:  n_with_two_shaders++; break;

            default:
            {
                break;
            }
        }
    }

    EXPECT_EQ(n_with_no_shader,
              1u);
    EXPECT_EQ(n_with_one_shader,
              1u);
    EXPECT_EQ(n_with_two_shaders,
              1u);
    EXPECT_EQ(scene.get_harness().get_frontend().get_n_unfollowed_calls(),
              0u);
}

TEST(D3D8FrontendDraws, AShaderThatIsDeletedIsDestroyedAndNoLongerSet)
{
    Tests::D3D8Scene scene;

    const uint32_t vertex_shader = scene.get_builder().create_vertex_shader({1, 2}, {3, 4});
    const uint32_t pixel_shader  = scene.get_builder().create_pixel_shader ({5, 6});

    scene.get_builder().delete_vertex_shader(vertex_shader);
    scene.get_builder().delete_pixel_shader (pixel_shader);
    scene.feed();

    const auto& resource_vec = scene.get_harness().get_model().resource_vec;

    EXPECT_TRUE(resource_vec.at(resource_vec.size() - 1).is_destroyed);
    EXPECT_TRUE(resource_vec.at(resource_vec.size() - 2).is_destroyed);

    /* A handle that is gone is not a shader any more. */
    scene.get_builder().set_pixel_shader(pixel_shader);
    scene.feed();

    EXPECT_EQ(scene.get_harness().get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST(D3D8FrontendDraws, TheVertexLayoutOfAShaderComesFromItsDeclaration)
{
    Tests::D3D8Scene scene;

    const uint32_t first  = scene.get_builder().create_vertex_shader({1, 2, 3, 4}, {5, 6});
    const uint32_t second = scene.get_builder().create_vertex_shader({1, 2, 3, 4}, {7, 8});
    const uint32_t third  = scene.get_builder().create_vertex_shader({9, 9, 9, 9}, {5, 6});

    scene.bind_back_buffer();

    scene.get_builder().set_vertex_shader(first);
    scene.draw(1);
    scene.get_builder().set_vertex_shader(second);
    scene.draw(1);
    scene.get_builder().set_vertex_shader(third);
    scene.draw(1);
    scene.present();

    std::set<uint64_t> layout_key_set;

    for (const auto& draw_pair : scene.get_harness().get_model().draw_structure_map)
    {
        layout_key_set.insert(draw_pair.second.vertex_layout_key);
    }

    /* The same declaration, however the code differs; and another declaration. */
    EXPECT_EQ(layout_key_set.size(),
              2u);
}

TEST(D3D8FrontendDraws, DrawsFromTheApplicationsMemoryAreDrawsWithoutBuffers)
{
    Tests::D3D8Scene scene;

    scene.bind_back_buffer();
    scene.get_builder().draw_primitive_up(Interception::Hooks::D3D8::D3DPT_TRIANGLELIST, 2, 24);
    scene.get_builder().draw_primitive_up(Interception::Hooks::D3D8::D3DPT_TRIANGLELIST, 2, 36);
    scene.feed();
    scene.present();

    const auto& model = scene.get_harness().get_model();

    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              2u);
    ASSERT_EQ(model.draw_structure_map.size(),
              2u);

    for (const auto& draw_pair : model.draw_structure_map)
    {
        EXPECT_TRUE(draw_pair.second.vertex_buffer_key_vec.empty() );
        EXPECT_EQ  (draw_pair.second.index_buffer_key,
                    0u);
    }
}

TEST(D3D8FrontendDraws, ResettingTheDeviceMakesANewBackBuffer)
{
    Tests::D3D8CallBuilder     builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    builder.reset();
    harness.feed(builder);

    const auto& resource_vec = harness.get_model().resource_vec;

    ASSERT_EQ(resource_vec.size(),
              4u);

    EXPECT_TRUE (resource_vec.at(0).is_destroyed);
    EXPECT_TRUE (resource_vec.at(1).is_destroyed);
    EXPECT_FALSE(resource_vec.at(2).is_destroyed);
    EXPECT_FALSE(resource_vec.at(3).is_destroyed);
    EXPECT_EQ   (resource_vec.at(2).description.kind,
                 Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE);

    /* The new buffers are the same as the ones before, as far as the analysis is concerned. */
    EXPECT_EQ(resource_vec.at(2).match_key,
              resource_vec.at(0).match_key);
}

TEST(D3D8FrontendDraws, WhatTheFrontEndDoesNotFollowIsCounted)
{
    Tests::D3D8CallBuilder     builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    builder.begin_state_block();
    builder.begin_state_block();
    harness.feed(builder);

    EXPECT_EQ(harness.get_frontend().get_n_unsupported_calls(),
              2u);
    EXPECT_EQ(harness.get_frontend().get_n_unfollowed_calls(),
              0u);
}

constexpr uint32_t FVF_XYZ_NORMAL_DIFFUSE_TEX1 = 0x152;
constexpr uint32_t STRIDE                      = 36;

/* What the application has to set again after a reset: the streams are gone with the buffers. */
static void bind_scene_buffers(Tests::D3D8Scene* inout_scene_ptr)
{
    inout_scene_ptr->get_builder().set_vertex_shader(FVF_XYZ_NORMAL_DIFFUSE_TEX1);
    inout_scene_ptr->get_builder().set_stream_source(0,
                                                     inout_scene_ptr->get_vertex_buffer(),
                                                     STRIDE);
    inout_scene_ptr->get_builder().set_indices      (inout_scene_ptr->get_index_buffer(),
                                                     0);

    inout_scene_ptr->feed();
}

TEST(D3D8FrontendDraws, ResettingTheDeviceReturnsItsStateToTheDefault)
{
    Tests::D3D8Scene scene;

    scene.bind_back_buffer();
    scene.get_builder().set_render_state(Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE,
                                         1);
    scene.draw(1);

    ASSERT_EQ(scene.get_harness().get_model().draw_structure_map.size(),
              1u);

    scene.get_builder().reset();
    scene.feed();

    bind_scene_buffers(&scene);

    /* Alpha blending is off again, so this draw is not like the one before. */
    scene.draw(1);

    EXPECT_EQ(scene.get_harness().get_model().draw_structure_map.size(),
              2u);
}

TEST(D3D8FrontendDraws, ACallWhichFailedDoesNotChangeTheState)
{
    Tests::D3D8Scene scene;

    scene.bind_back_buffer();
    scene.draw(1);

    scene.get_builder().fail_next_device_call();
    scene.get_builder().set_render_state     (Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE,
                                              1);
    scene.draw(1);

    EXPECT_EQ(scene.get_harness().get_model().draw_structure_map.size(),
              1u);
}

TEST(D3D8FrontendDraws, TheViewportOfADeviceIsTheWholeBackBufferUntilItIsSet)
{
    Tests::D3D8Scene scene;

    scene.draw   (1);
    scene.present();

    const auto& model     = scene.get_harness().get_model();
    const auto& structure = model.pass_structure_interner.get_structure(0);

    EXPECT_GT(model.resource_vec.at(0).description.width,
              0u);
    EXPECT_EQ(structure.viewport.width,
              static_cast<float>(model.resource_vec.at(0).description.width) );
    EXPECT_EQ(structure.viewport.height,
              static_cast<float>(model.resource_vec.at(0).description.height) );
}

TEST(D3D8FrontendDraws, ResettingTheDeviceDestroysItsOwnDepthBufferAndNotTheOneOfTheApplication)
{
    Tests::D3D8CallBuilder     builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const uint64_t depth_surface = builder.create_depth_stencil_surface(512,
                                                                         512,
                                                                         Interception::Hooks::D3D8::D3DFMT_D24S8);

    builder.set_render_target(builder.get_render_target(),
                              depth_surface);
    builder.reset();
    harness.feed(builder);

    const auto& resource_vec = harness.get_model().resource_vec;

    ASSERT_GE(resource_vec.size(),
              3u);

    EXPECT_TRUE (resource_vec.at(0).is_destroyed); /* back buffer      */
    EXPECT_TRUE (resource_vec.at(1).is_destroyed); /* its depth buffer */
    EXPECT_FALSE(resource_vec.at(2).is_destroyed); /* the application's */
}

TEST(D3D8FrontendDraws, ADeviceDoesNotKnowTheShadersOfTheDeviceBeforeIt)
{
    Tests::D3D8CallBuilder     builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const uint32_t handle = builder.create_vertex_shader({1, 2, 3, 4},
                                                         {5, 6});

    builder.set_vertex_shader(handle);
    builder.draw_primitive   (Interception::Hooks::D3D8::D3DPT_TRIANGLELIST,
                              0,
                              1);

    /* On another device the handle is no shader, so the draw has none. */
    builder.create_device();
    builder.set_vertex_shader(handle);
    builder.draw_primitive   (Interception::Hooks::D3D8::D3DPT_TRIANGLELIST,
                              0,
                              1);
    harness.feed(builder);

    uint32_t n_draws_with_a_shader    = 0;
    uint32_t n_draws_without_a_shader = 0;

    for (const auto& draw_pair : harness.get_model().draw_structure_map)
    {
        if (draw_pair.second.shader_key_vec.empty() )
        {
            ++n_draws_without_a_shader;
        }
        else
        {
            ++n_draws_with_a_shader;
        }
    }

    EXPECT_EQ(n_draws_with_a_shader,
              1u);
    EXPECT_EQ(n_draws_without_a_shader,
              1u);
}
