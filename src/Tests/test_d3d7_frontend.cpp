/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the D3D7 front end following the surfaces, the device, the draws and the frames.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/resource_identity.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "d3d7_scene.h"

TEST(D3D7Frontend, SurfacesAreResourcesNamedLikeThePrinterNamesThem)
{
    Tests::D3D7Scene scene;

    const auto& resource_vec = scene.get_harness().get_model().resource_vec;

    ASSERT_EQ(resource_vec.size(),
              3u);

    /* The primary surface, the back buffer of its chain, and the depth buffer. */
    EXPECT_EQ(resource_vec.at(0).debug_name,
              std::string("Surface#1") );
    EXPECT_EQ(resource_vec.at(0).description.kind,
              Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE);
    EXPECT_EQ(resource_vec.at(1).debug_name,
              std::string("Surface#2") );
    EXPECT_EQ(resource_vec.at(2).debug_name,
              std::string("Surface#3") );
}

TEST(D3D7Frontend, ThePrimarySurfaceIsAsLargeAsTheScreen)
{
    Tests::D3D7Scene scene;

    const auto& description = scene.get_harness().get_model().resource_vec.at(0).description;

    EXPECT_EQ(description.width,
              640u);
    EXPECT_EQ(description.height,
              480u);
}

TEST(D3D7Frontend, TheBackBufferOfTheChainIsATargetOfTheSizeOfTheScreen)
{
    Tests::D3D7Scene scene;

    const auto& description = scene.get_harness().get_model().resource_vec.at(1).description;

    EXPECT_EQ  (description.kind,
                Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE);
    EXPECT_TRUE(description.is_render_target);
    EXPECT_EQ  (description.width,
                640u);
}

TEST(D3D7Frontend, ADepthBufferIsADepthResource)
{
    Tests::D3D7Scene scene;

    const auto& description = scene.get_harness().get_model().resource_vec.at(2).description;

    EXPECT_TRUE(description.is_depth_stencil);
    EXPECT_EQ  (description.format.format_class,
                Analysis::APIDumpAnalysis::FormatClass::DEPTH);
}

TEST(D3D7Frontend, AFlipEndsAFrame)
{
    Tests::D3D7Scene scene;

    scene.draw   (2);
    scene.present();
    scene.draw   (1);
    scene.present();

    EXPECT_EQ(scene.get_harness().get_model().frame_record_vec.size(),
              2u);
}

TEST(D3D7Frontend, ABlitToThePrimarySurfaceEndsAFrame)
{
    Tests::D3D7Scene scene;

    scene.draw(1);

    scene.get_builder().blt(scene.get_primary_surface(),
                            scene.get_back_buffer_surface(),
                            false); /* in_is_primary: a blit to something else does not end it */
    scene.feed();

    EXPECT_EQ(scene.get_harness().get_model().frame_record_vec.size(),
              0u);

    scene.get_builder().blt(scene.get_primary_surface(),
                            scene.get_back_buffer_surface(),
                            true); /* in_is_primary */
    scene.feed();

    EXPECT_EQ(scene.get_harness().get_model().frame_record_vec.size(),
              1u);
}

TEST(D3D7Frontend, DrawsAreOfThePassThatRendersToTheBackBuffer)
{
    Tests::D3D7Scene scene;

    scene.get_builder().clear(Interception::Core::D3D7Utils::D3DCLEAR_TARGET | Interception::Core::D3D7Utils::D3DCLEAR_ZBUFFER,
                              0xFF000000,
                              1.0f,
                              0);
    scene.draw   (3);
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.frame_record_vec.size(),
              1u);
    EXPECT_EQ(model.frame_record_vec.at(0).n_passes,
              1u);
    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              3u);

    const auto& structure = model.pass_structure_interner.get_structure(0);

    EXPECT_TRUE(structure.clears_color);
    EXPECT_TRUE(structure.clears_depth);
    EXPECT_EQ  (structure.color_view_key_vec.size(),
                1u);
    EXPECT_NE  (structure.depth_stencil_view_key,
                0u);
}

TEST(D3D7Frontend, ThePassHasNoDepthBufferIfNoneIsAttachedToTheTarget)
{
    Tests::D3D7Scene scene;

    const uint64_t target = scene.get_builder().create_primary_surface(1);

    scene.get_builder().set_render_target(scene.get_builder().get_attached_surface(target,
                                                                                    Interception::Core::D3D7Utils::DDSCAPS_BACKBUFFER) );
    scene.feed();
    scene.draw   (1);
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.frame_record_vec.size(),
              1u);
    EXPECT_EQ(model.pass_structure_interner.get_structure(0).depth_stencil_view_key,
              0u);
}

TEST(D3D7Frontend, TheViewportIsTheWholeOfTheTargetUntilTheApplicationSaysOtherwise)
{
    Tests::D3D7Scene scene;

    scene.draw   (1);
    scene.present();

    EXPECT_EQ(scene.get_harness().get_model().pass_structure_interner.get_structure(0).viewport.width,
              640.0f);

    scene.get_builder().set_viewport(0,
                                    0,
                                    320,
                                    240);
    scene.draw   (1);
    scene.present();

    ASSERT_EQ(scene.get_harness().get_model().pass_structure_interner.get_n_structures(),
              2u);
    EXPECT_EQ(scene.get_harness().get_model().pass_structure_interner.get_structure(1).viewport.width,
              320.0f);
}

TEST(D3D7Frontend, DrawsWithDifferentRenderStatesAreOfDifferentKinds)
{
    Tests::D3D7Scene scene;

    scene.draw(1);
    scene.get_builder().set_render_state(Interception::Core::D3D7Utils::D3DRENDERSTATE_ALPHABLENDENABLE,
                                         1);
    scene.draw(1);
    scene.present();

    EXPECT_EQ(scene.get_harness().get_model().draw_structure_map.size(),
              2u);
}

TEST(D3D7Frontend, SettingAStateToWhatItIsDoesNotMakeANewKindOfDraw)
{
    Tests::D3D7Scene scene;

    scene.get_builder().set_render_state(Interception::Core::D3D7Utils::D3DRENDERSTATE_ALPHABLENDENABLE,
                                         1);
    scene.draw(1);
    scene.get_builder().set_render_state(Interception::Core::D3D7Utils::D3DRENDERSTATE_ALPHABLENDENABLE,
                                         1);
    scene.draw(1);
    scene.present();

    EXPECT_EQ(scene.get_harness().get_model().draw_structure_map.size(),
              1u);
}

TEST(D3D7Frontend, TheValueOfAFreeNumberStateDoesNotMakeANewKindOfDraw)
{
    Tests::D3D7Scene scene;

    scene.get_builder().set_render_state(Interception::Core::D3D7Utils::D3DRENDERSTATE_TEXTUREFACTOR,
                                         0xFF112233);
    scene.draw(1);
    scene.get_builder().set_render_state(Interception::Core::D3D7Utils::D3DRENDERSTATE_TEXTUREFACTOR,
                                         0xFF445566);
    scene.draw(1);
    scene.present();

    EXPECT_EQ(scene.get_harness().get_model().draw_structure_map.size(),
              1u);
}

TEST(D3D7Frontend, ATextureMakesAnotherKindOfDrawThanNoneDoes)
{
    Tests::D3D7Scene scene;

    Tests::D3D7CallBuilder::SurfaceSpec spec;

    spec.caps   = Interception::Core::D3D7Utils::DDSCAPS_TEXTURE;
    spec.flags  = Interception::Core::D3D7Utils::DDSD_CAPS | Interception::Core::D3D7Utils::DDSD_HEIGHT | Interception::Core::D3D7Utils::DDSD_WIDTH;
    spec.height = 64;
    spec.width  = 64;

    const uint64_t texture = scene.get_builder().create_surface(spec);

    scene.draw(1);
    scene.get_builder().set_texture(0,
                                    texture);
    scene.draw(1);
    scene.get_builder().set_texture(0,
                                    0);
    scene.draw(1);
    scene.present();

    EXPECT_EQ(scene.get_harness().get_model().draw_structure_map.size(),
              2u);
}

TEST(D3D7Frontend, TheLevelsOfATextureAreAPartOfIt)
{
    Tests::D3D7Scene scene;

    Tests::D3D7CallBuilder::SurfaceSpec spec;

    spec.caps          = Interception::Core::D3D7Utils::DDSCAPS_COMPLEX | Interception::Core::D3D7Utils::DDSCAPS_MIPMAP | Interception::Core::D3D7Utils::DDSCAPS_TEXTURE;
    spec.flags         = Interception::Core::D3D7Utils::DDSD_CAPS | Interception::Core::D3D7Utils::DDSD_HEIGHT | Interception::Core::D3D7Utils::DDSD_MIPMAPCOUNT | Interception::Core::D3D7Utils::DDSD_WIDTH;
    spec.height        = 64;
    spec.mip_map_count = 7;
    spec.width         = 64;

    const uint64_t texture = scene.get_builder().create_surface(spec);
    const uint64_t level_1 = scene.get_builder().get_attached_surface(texture,
                                                                      Interception::Core::D3D7Utils::DDSCAPS_MIPMAP);
    scene.feed();

    const auto* level_ptr = scene.get_harness().get_frontend().get_tracker().get_object_ptr(level_1);
    const auto* top_ptr   = scene.get_harness().get_frontend().get_tracker().get_object_ptr(texture);

    ASSERT_NE(level_ptr,
              nullptr);
    ASSERT_NE(top_ptr,
              nullptr);
    EXPECT_EQ(level_ptr->view.resource_id,
              top_ptr->view.resource_id);
    EXPECT_EQ(level_ptr->view.mip_level,
              1u);
    EXPECT_FALSE(level_ptr->owns_resource);

    EXPECT_EQ(scene.get_harness().get_model().resource_vec.back().description.mip_level_count,
              7u);
}

TEST(D3D7Frontend, WhatTheApplicationWritesToATextureBeforeADrawMakesItStatic)
{
    Tests::D3D7Scene scene;

    Tests::D3D7CallBuilder::SurfaceSpec spec;

    spec.caps   = Interception::Core::D3D7Utils::DDSCAPS_TEXTURE;
    spec.flags  = Interception::Core::D3D7Utils::DDSD_CAPS | Interception::Core::D3D7Utils::DDSD_HEIGHT | Interception::Core::D3D7Utils::DDSD_WIDTH;
    spec.height = 4;
    spec.width  = 4;

    const uint64_t texture = scene.get_builder().create_surface(spec);

    scene.get_builder().unlock(texture,
                               std::vector<uint8_t>(64,
                                                    0x7F) );
    scene.get_builder().set_texture(0,
                                    texture);
    scene.draw   (1);
    scene.present();
    scene.draw   (1);
    scene.present();

    const auto& model = scene.get_harness().get_model();
    const auto& access = model.resource_access_vec.at(model.resource_vec.size() - 1);

    EXPECT_EQ(access.n_frames_read,
              2u);
    EXPECT_EQ(access.n_frames_written,
              1u);
}

TEST(D3D7Frontend, ASurfaceIsGoneWithItsLastReference)
{
    Tests::D3D7Scene scene;

    scene.get_builder().release(scene.get_depth_surface(),
                                1); /* a reference is left */
    scene.feed();

    EXPECT_FALSE(scene.get_harness().get_model().resource_vec.at(2).is_destroyed);

    scene.get_builder().release(scene.get_depth_surface(),
                                0);
    scene.feed();

    EXPECT_TRUE(scene.get_harness().get_model().resource_vec.at(2).is_destroyed);
    EXPECT_EQ  (scene.get_harness().get_frontend().get_tracker().get_object_ptr(scene.get_depth_surface() ),
                nullptr);
}

TEST(D3D7Frontend, ACallThatFailedChangedNothing)
{
    Tests::D3D7Scene failed_scene;
    Tests::D3D7Scene untouched_scene;
    Tests::D3D7Scene working_scene;

    failed_scene.get_builder().fail_next_device_call();
    failed_scene.get_builder().set_render_state(Interception::Core::D3D7Utils::D3DRENDERSTATE_ALPHABLENDENABLE,
                                                1);
    working_scene.get_builder().set_render_state(Interception::Core::D3D7Utils::D3DRENDERSTATE_ALPHABLENDENABLE,
                                                 1);

    failed_scene.draw   (1);
    untouched_scene.draw(1);
    working_scene.draw  (1);

    failed_scene.present   ();
    untouched_scene.present();
    working_scene.present  ();

    const auto get_state_key_func = [](Tests::D3D7Scene& in_scene)
    {
        return in_scene.get_harness().get_model().draw_structure_map.begin()->second.state_key;
    };

    /* The state that failed to be set is not there; the one that was set is. */
    EXPECT_EQ(get_state_key_func(failed_scene),
              get_state_key_func(untouched_scene) );
    EXPECT_NE(get_state_key_func(working_scene),
              get_state_key_func(untouched_scene) );
}

TEST(D3D7Frontend, ASurfaceThatWasNeverMadeIsCountedAsUnfollowed)
{
    Tests::D3D7Scene scene;

    ASSERT_EQ(scene.get_harness().get_frontend().get_n_unfollowed_calls(),
              0u);

    scene.get_builder().set_texture(0,
                                    0xDEAD0);
    scene.feed();

    EXPECT_EQ(scene.get_harness().get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST(D3D7Frontend, StateBlocksAreCountedAsUnsupported)
{
    Tests::D3D7Scene scene;

    scene.get_builder().begin_state_block();
    scene.feed();

    EXPECT_EQ(scene.get_harness().get_frontend().get_n_unsupported_calls(),
              1u);
}

TEST(D3D7Frontend, VerticesInScreenSpaceAreAResolutionHazard)
{
    Tests::D3D7Scene scene;

    constexpr uint32_t FVF_XYZRHW_DIFFUSE = 0x044;

    scene.get_builder().draw_primitive(Interception::Hooks::D3D7::D3DPT_TRIANGLELIST,
                                       FVF_XYZRHW_DIFFUSE,
                                       3);
    scene.feed();

    const auto& hazard_map = scene.get_harness().get_frontend().get_n_resolution_hazards();

    ASSERT_NE(hazard_map.find(Analysis::APIDumpAnalysis::ResolutionHazard::SCREEN_SPACE_VERTICES),
              hazard_map.end() );
    EXPECT_EQ(hazard_map.at(Analysis::APIDumpAnalysis::ResolutionHazard::SCREEN_SPACE_VERTICES),
              1u);
}

TEST(D3D7Frontend, TheVerticesInTheApplicationsMemoryTellHowDeepADrawIs)
{
    constexpr uint32_t FVF_XYZ = 0x002;

    Tests::D3D7Scene scene;

    /* The camera is 4 from the origin of the world, and the vertices are 6 beyond it. */
    Interception::Hooks::D3D7::D3DMATRIX view = {};

    view.m[0][0] = 1.0f;
    view.m[1][1] = 1.0f;
    view.m[2][2] = 1.0f;
    view.m[3][2] = 4.0f;
    view.m[3][3] = 1.0f;

    const float vertex_array[9] = {0.0f, 0.0f, 6.0f,
                                   1.0f, 0.0f, 6.0f,
                                   0.0f, 1.0f, 6.0f};

    scene.get_builder().set_transform(Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_VIEW,
                                      view);
    scene.get_builder().draw_primitive(Interception::Hooks::D3D7::D3DPT_TRIANGLELIST,
                                       FVF_XYZ,
                                       3,
                                       Tests::D3D7CallBuilder::get_bytes(vertex_array,
                                                                         sizeof(vertex_array) ));
    scene.feed();
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                10.0f,
                1.0f);
}

TEST(D3D7Frontend, TheVerticesOfAnIndexedDrawAreThoseTheIndicesPick)
{
    constexpr uint32_t FVF_XYZ = 0x002;

    Tests::D3D7Scene scene;

    Interception::Hooks::D3D7::D3DMATRIX view = {};

    view.m[0][0] = 1.0f;
    view.m[1][1] = 1.0f;
    view.m[2][2] = 1.0f;
    view.m[3][2] = 4.0f;
    view.m[3][3] = 1.0f;

    /* The first vertex is far away, and no index picks it. */
    const float    vertex_array[9] = {0.0f, 0.0f, 100.0f,
                                      0.0f, 0.0f, 6.0f,
                                      1.0f, 0.0f, 6.0f};
    const uint16_t index_array[3]  = {1, 2, 1};

    scene.get_builder().set_transform(Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_VIEW,
                                      view);
    scene.get_builder().draw_indexed_primitive(Interception::Hooks::D3D7::D3DPT_TRIANGLELIST,
                                               FVF_XYZ,
                                               3,
                                               std::vector<uint16_t>(index_array,
                                                                     index_array + 3),
                                               Tests::D3D7CallBuilder::get_bytes(vertex_array,
                                                                                 sizeof(vertex_array) ));
    scene.feed();
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                10.0f,
                1.0f);
}

TEST(D3D7Frontend, TheVerticesOfAVertexBufferAreWhatWasWrittenToIt)
{
    constexpr uint32_t FVF_XYZ = 0x002;

    Tests::D3D7Scene scene;

    Interception::Hooks::D3D7::D3DMATRIX view = {};

    view.m[0][0] = 1.0f;
    view.m[1][1] = 1.0f;
    view.m[2][2] = 1.0f;
    view.m[3][2] = 4.0f;
    view.m[3][3] = 1.0f;

    const float vertex_array[9] = {0.0f, 0.0f, 6.0f,
                                   1.0f, 0.0f, 6.0f,
                                   0.0f, 1.0f, 6.0f};

    const uint64_t vertex_buffer = scene.get_builder().create_vertex_buffer(FVF_XYZ,
                                                                            3);

    scene.get_builder().unlock_vertex_buffer(vertex_buffer,
                                             Tests::D3D7CallBuilder::get_bytes(vertex_array,
                                                                               sizeof(vertex_array) ));
    scene.get_builder().set_transform       (Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_VIEW,
                                             view);
    scene.get_builder().draw_primitive_vb   (Interception::Hooks::D3D7::D3DPT_TRIANGLELIST,
                                             vertex_buffer,
                                             0,
                                             3);
    scene.feed();
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                10.0f,
                1.0f);
}

TEST(D3D7Frontend, PreTransformedVerticesAreSampledByTheReciprocalOfTheirDepthAndNotMovedByAMatrix)
{
    constexpr uint32_t FVF_XYZRHW = 0x004;

    Tests::D3D7Scene scene;

    Interception::Hooks::D3D7::D3DMATRIX view = {};

    view.m[0][0] = 1.0f;
    view.m[1][1] = 1.0f;
    view.m[2][2] = 1.0f;
    view.m[3][2] = 4.0f;
    view.m[3][3] = 1.0f;

    const float vertex_array[12] = {10.0f, 10.0f, 0.5f, 1.0f,
                                    20.0f, 10.0f, 0.5f, 1.0f,
                                    10.0f, 20.0f, 0.5f, 1.0f};

    scene.get_builder().set_transform(Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_VIEW,
                                      view);
    scene.get_builder().draw_primitive(Interception::Hooks::D3D7::D3DPT_TRIANGLELIST,
                                       FVF_XYZRHW,
                                       3,
                                       Tests::D3D7CallBuilder::get_bytes(vertex_array,
                                                                         sizeof(vertex_array) ));
    scene.feed();
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);

    /* The reciprocal of the depth is 1, so the depth is 1: the view matrix (the camera is 4 away) has nothing to do with it. */
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                1.0f,
                0.2f);
}

TEST(D3D7Frontend, TheVerticesOfAStridedDrawAreThoseOfItsArraysOfPositions)
{
    constexpr uint32_t FVF_XYZ = 0x002;
    constexpr uint32_t STRIDE  = 16; /* the positions are not next to each other */

    Tests::D3D7Scene scene;

    Interception::Hooks::D3D7::D3DMATRIX view = {};

    view.m[0][0] = 1.0f;
    view.m[1][1] = 1.0f;
    view.m[2][2] = 1.0f;
    view.m[3][2] = 4.0f;
    view.m[3][3] = 1.0f;

    const float position_array[12] = {0.0f, 0.0f, 6.0f, 0.0f,
                                      1.0f, 0.0f, 6.0f, 0.0f,
                                      0.0f, 1.0f, 6.0f, 0.0f};

    scene.get_builder().set_transform(Interception::Core::D3D7Utils::D3DTRANSFORMSTATE_VIEW,
                                      view);
    scene.get_builder().draw_primitive_strided(Interception::Hooks::D3D7::D3DPT_TRIANGLELIST,
                                               FVF_XYZ,
                                               3,
                                               STRIDE,
                                               Tests::D3D7CallBuilder::get_bytes(position_array,
                                                                                 sizeof(position_array) ));
    scene.feed();
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.surface_depth_histogram_map.size(),
              1u);
    EXPECT_NEAR(model.surface_depth_histogram_map.begin()->second.get_depth_at_fraction(0.5),
                10.0f,
                1.0f);
}

TEST(D3D7Frontend, AnObjectOfTheApplicationIsFoundByItsAddressAsTheResourceOrThePartOfItThatItStandsFor)
{
    Tests::D3D7Scene scene;

    Tests::D3D7CallBuilder::SurfaceSpec spec;

    spec.caps          = Interception::Core::D3D7Utils::DDSCAPS_COMPLEX | Interception::Core::D3D7Utils::DDSCAPS_MIPMAP | Interception::Core::D3D7Utils::DDSCAPS_TEXTURE;
    spec.flags         = Interception::Core::D3D7Utils::DDSD_CAPS | Interception::Core::D3D7Utils::DDSD_HEIGHT | Interception::Core::D3D7Utils::DDSD_MIPMAPCOUNT | Interception::Core::D3D7Utils::DDSD_WIDTH;
    spec.height        = 64;
    spec.mip_map_count = 7;
    spec.width         = 64;

    const uint64_t texture = scene.get_builder().create_surface(spec);
    const uint64_t level_1 = scene.get_builder().get_attached_surface(texture,
                                                                      Interception::Core::D3D7Utils::DDSCAPS_MIPMAP);
    scene.feed();

    Analysis::APIDumpAnalysis::ResourceView level_view;
    Analysis::APIDumpAnalysis::ResourceView texture_view;

    ASSERT_TRUE(scene.get_harness().get_frontend().find_resource_view(texture,
                                                                     &texture_view) );
    ASSERT_TRUE(scene.get_harness().get_frontend().find_resource_view(level_1,
                                                                     &level_view) );

    EXPECT_EQ(level_view.resource_id,
              texture_view.resource_id);
    EXPECT_EQ(texture_view.mip_level,
              0u);
    EXPECT_EQ(level_view.mip_level,
              1u);

    EXPECT_FALSE(scene.get_harness().get_frontend().find_resource_view(0xDEAD0,
                                                                      &level_view) );
}

TEST(D3D7Frontend, TheTextureTheApplicationMakesCanBeToldByTheIdentityOfItsResourceWhenItIsMade)
{
    Tests::D3D7Scene scene;

    Tests::D3D7CallBuilder::SurfaceSpec spec;

    spec.caps   = Interception::Core::D3D7Utils::DDSCAPS_TEXTURE;
    spec.flags  = Interception::Core::D3D7Utils::DDSD_CAPS | Interception::Core::D3D7Utils::DDSD_HEIGHT | Interception::Core::D3D7Utils::DDSD_WIDTH;
    spec.height = 64;
    spec.width  = 64;

    const uint64_t first_texture  = scene.get_builder().create_surface(spec);
    const uint64_t second_texture = scene.get_builder().create_surface(spec);

    scene.feed();

    Analysis::APIDumpAnalysis::ResourceView     first_view;
    Analysis::APIDumpAnalysis::ResourceView     second_view;
    Analysis::APIDumpAnalysis::ResourceIdentity first_identity;
    Analysis::APIDumpAnalysis::ResourceIdentity second_identity;

    ASSERT_TRUE(scene.get_harness().get_frontend().find_resource_view(first_texture,
                                                                     &first_view) );
    ASSERT_TRUE(scene.get_harness().get_frontend().find_resource_view(second_texture,
                                                                     &second_view) );
    ASSERT_TRUE(Analysis::APIDumpAnalysis::ResourceIdentities::find(scene.get_harness().get_model().resource_vec,
                                                                    first_view.resource_id,
                                                                   &first_identity) );
    ASSERT_TRUE(Analysis::APIDumpAnalysis::ResourceIdentities::find(scene.get_harness().get_model().resource_vec,
                                                                    second_view.resource_id,
                                                                   &second_identity) );

    /* Made in the same way, so told apart by the order. */
    EXPECT_EQ(first_identity.description_key,
              second_identity.description_key);
    EXPECT_EQ(first_identity.ordinal + 1,
              second_identity.ordinal);
}
