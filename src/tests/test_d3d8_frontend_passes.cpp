/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the D3D8 front end turning the calls of a frame into passes and draws.
 */
#include <gtest/gtest.h>
#include "d3d8_scene.h"
#include "data_fixtures.h"

TEST(D3D8FrontendPasses, AFrameOfFixedFunctionDrawsIsOnePass)
{
    Tests::D3D8Scene                     scene;
    Interception::Hooks::D3D8::D3DMATRIX projection = {};

    scene.bind_back_buffer();
    scene.get_builder().set_viewport({0, 0, 800, 600, 0.0f, 1.0f});
    scene.get_builder().clear       (Interception::Hooks::D3D8::D3DCLEAR_TARGET | Interception::Hooks::D3D8::D3DCLEAR_ZBUFFER, 0xFF000000, 1.0f, 0);
    scene.set_view                  (1.0f);
    scene.get_builder().set_transform(Interception::Hooks::D3D8::D3DTS_PROJECTION,
                                      projection);
    scene.draw                      (3);
    scene.present                   ();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.frame_record_vec.size(),
              1u);
    EXPECT_EQ(model.frame_record_vec.at(0).n_passes,
              1u);
    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              3u);

    ASSERT_EQ(model.pass_structure_interner.get_n_structures(),
              1u);

    const auto& structure = model.pass_structure_interner.get_structure(0);

    EXPECT_TRUE(structure.clears_color);
    EXPECT_TRUE(structure.clears_depth);
    EXPECT_EQ  (structure.viewport.width,
                800.0f);
    EXPECT_EQ  (structure.viewport.height,
                600.0f);
    EXPECT_EQ  (structure.draw_structure_key_vec.size(),
                1u);

    /* The view and the projection are known to be what they are, because the API says so. */
    ASSERT_EQ(structure.constant_signature_vec.size(),
              2u);
    EXPECT_EQ(structure.constant_signature_vec.at(0).provenance,
              Analysis::APIDumpAnalysis::ConstantProvenance::API_DEFINED);
    EXPECT_EQ(model.constant_series_map.size(),
              2u);

    EXPECT_EQ(scene.get_harness().get_frontend().get_n_unfollowed_calls(),
              0u);
}

TEST(D3D8FrontendPasses, DrawsAreCountedInVertices)
{
    Tests::D3D8Scene scene;

    scene.bind_back_buffer();
    scene.draw            (3); /* 3 draws of 2 triangles: 18 indices */
    scene.get_builder().draw_primitive(Interception::Hooks::D3D8::D3DPT_TRIANGLESTRIP, 0, 4); /* 6 */
    scene.get_builder().draw_primitive(Interception::Hooks::D3D8::D3DPT_TRIANGLEFAN,   0, 3); /* 5 */
    scene.get_builder().draw_primitive(Interception::Hooks::D3D8::D3DPT_LINELIST,      0, 2); /* 4 */
    scene.get_builder().draw_primitive(Interception::Hooks::D3D8::D3DPT_LINESTRIP,     0, 3); /* 4 */
    scene.get_builder().draw_primitive(Interception::Hooks::D3D8::D3DPT_POINTLIST,     0, 5); /* 5 */
    scene.feed                        ();
    scene.present                     ();

    const auto& model         = scene.get_harness().get_model();
    const auto  entry_ptr_vec = model.representative_store.get_entry_ptr_vec(model.frame_record_vec.at(0).shape_id);

    ASSERT_EQ(entry_ptr_vec.size(),
              1u);
    ASSERT_EQ(entry_ptr_vec.at(0)->detail.pass_instance_vec.size(),
              1u);
    EXPECT_EQ(entry_ptr_vec.at(0)->detail.pass_instance_vec.at(0).n_draws,
              8u);
    EXPECT_EQ(entry_ptr_vec.at(0)->detail.pass_instance_vec.at(0).n_vertices,
              18u + 6u + 5u + 4u + 4u + 5u);
}

TEST(D3D8FrontendPasses, ABindEveryScenePassMakesThreePassesLikeFr08)
{
    Tests::D3D8Scene scene;

    scene.bind_back_buffer();
    scene.get_builder().set_viewport({0, 45, 800, 510, 0.0f, 1.0f});
    scene.get_builder().clear       (Interception::Hooks::D3D8::D3DCLEAR_TARGET | Interception::Hooks::D3D8::D3DCLEAR_ZBUFFER, 0xFF000000, 1.0f, 0);
    scene.set_view                  (1.0f);
    scene.draw                      (5);

    scene.bind_back_buffer();
    scene.get_builder().set_viewport({0, 45, 800, 510, 0.0f, 1.0f});
    scene.get_builder().clear       (Interception::Hooks::D3D8::D3DCLEAR_ZBUFFER, 0xFF000000, 1.0f, 0);
    scene.set_view                  (2.0f);
    scene.draw                      (2);

    scene.bind_back_buffer();
    scene.get_builder().set_viewport({0, 0, 800, 600, 0.0f, 1.0f});
    scene.set_view                  (3.0f);
    scene.draw                      (4);
    scene.present                   ();

    const auto& model = scene.get_harness().get_model();

    EXPECT_EQ(model.frame_record_vec.at(0).n_passes,
              3u);
    EXPECT_EQ(model.frame_record_vec.at(0).n_draws,
              11u);
    ASSERT_EQ(model.pass_structure_interner.get_n_structures(),
              3u);

    EXPECT_TRUE (model.pass_structure_interner.get_structure(0).clears_color);
    EXPECT_FALSE(model.pass_structure_interner.get_structure(1).clears_color);
    EXPECT_TRUE (model.pass_structure_interner.get_structure(1).clears_depth);
    EXPECT_FALSE(model.pass_structure_interner.get_structure(2).clears_depth);
    EXPECT_EQ   (model.pass_structure_interner.get_structure(1).viewport.y,
                 45.0f);
    EXPECT_EQ   (model.pass_structure_interner.get_structure(2).viewport.height,
                 600.0f);
}

TEST(D3D8FrontendPasses, ARenderTargetThatIsATextureTakesTheViewportOfTheTexture)
{
    Tests::D3D8Scene scene;

    const auto texture       = scene.get_builder().create_texture(512, 512, 1, Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET, Interception::Hooks::D3D8::D3DFMT_X8R8G8B8);
    const auto level         = scene.get_builder().get_surface_level(texture, 0);
    const auto depth_surface = scene.get_builder().create_depth_stencil_surface(512, 512, Interception::Hooks::D3D8::D3DFMT_D24S8);

    scene.get_builder().set_render_target(level,
                                          depth_surface);
    scene.draw(1);
    scene.present();

    const auto& structure = scene.get_harness().get_model().pass_structure_interner.get_structure(0);

    /* D3D8 makes the viewport the whole of a new render target. */
    EXPECT_EQ(structure.viewport.width,
              512.0f);
    EXPECT_EQ(structure.viewport.height,
              512.0f);
}

TEST(D3D8FrontendPasses, APassDependsOnThePassThatRenderedTheTextureItSamples)
{
    const auto run_func = [](const uint32_t& in_first_pass_blend)
    {
        Tests::D3D8Scene scene;

        const auto texture       = scene.get_builder().create_texture(512, 512, 1, Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET, Interception::Hooks::D3D8::D3DFMT_X8R8G8B8);
        const auto level         = scene.get_builder().get_surface_level(texture, 0);
        const auto depth_surface = scene.get_builder().create_depth_stencil_surface(512, 512, Interception::Hooks::D3D8::D3DFMT_D24S8);

        scene.get_builder().set_render_target(level,
                                              depth_surface);
        scene.get_builder().clear            (Interception::Hooks::D3D8::D3DCLEAR_TARGET | Interception::Hooks::D3D8::D3DCLEAR_ZBUFFER, 0xFF000000, 1.0f, 0);
        scene.get_builder().set_render_state (Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE,
                                              in_first_pass_blend);
        scene.draw(2);

        scene.bind_back_buffer();
        scene.get_builder().set_texture(0,
                                        texture);
        scene.draw(1);
        scene.present();

        /* The match keys of the passes, in the order they were made. */
        const auto&           model = scene.get_harness().get_model();
        std::vector<uint64_t> key_vec;

        for (const auto& instance : model.representative_store.get_entry_ptr_vec(0).at(0)->detail.pass_instance_vec)
        {
            key_vec.push_back(model.pass_structure_interner.get_structure(instance.structure_id).key);
        }

        return key_vec;
    };

    const auto plain = run_func(0);
    const auto again = run_func(0);
    const auto blend = run_func(1);

    ASSERT_EQ(plain.size(),
              2u);
    ASSERT_EQ(blend.size(),
              2u);

    EXPECT_EQ(plain,
              again);

    /* The first pass is another kind of pass, and so is the second: it samples what the first has rendered. */
    EXPECT_NE(plain.at(0),
              blend.at(0) );
    EXPECT_NE(plain.at(1),
              blend.at(1) );
}

TEST(D3D8FrontendPasses, ADrawThatIsMadeAnotherWayIsAnotherKindOfDraw)
{
    Tests::D3D8Scene scene;

    scene.bind_back_buffer();
    scene.draw(2);
    scene.get_builder().set_render_state(Interception::Hooks::D3D8::D3DRS_ALPHABLENDENABLE,
                                         1);
    scene.draw(1);
    scene.present();

    const auto& model = scene.get_harness().get_model();

    ASSERT_EQ(model.pass_structure_interner.get_n_structures(),
              1u);
    EXPECT_EQ(model.pass_structure_interner.get_structure(0).draw_structure_key_vec.size(),
              2u);
}

TEST(D3D8FrontendPasses, TheValuesOfNumberStatesAndTheLightsDoNotChangeTheKinds)
{
    Tests::D3D8Scene scene;

    for (uint32_t n_frame = 0;
                  n_frame < 3;
                ++n_frame)
    {
        scene.bind_back_buffer();
        scene.get_builder().set_render_state(Interception::Hooks::D3D8::D3DRS_TEXTUREFACTOR,
                                             0xFF000000 + n_frame);
        scene.get_builder().set_light       (0,
                                             3);
        scene.get_builder().light_enable    (0,
                                             true); /* in_is_enabled */
        scene.draw(2);
        scene.present();
    }

    const auto& model = scene.get_harness().get_model();

    EXPECT_EQ(model.frame_shape_interner.get_n_structures(),
              1u);
    EXPECT_EQ(model.pass_structure_interner.get_n_structures(),
              1u);
}

TEST(D3D8FrontendPasses, ALightChangesTheKind)
{
    Tests::D3D8Scene scene;

    scene.bind_back_buffer();
    scene.draw(1);
    scene.present();

    scene.bind_back_buffer();
    scene.get_builder().light_enable(0,
                                     true); /* in_is_enabled */
    scene.draw(1);
    scene.present();

    EXPECT_EQ(scene.get_harness().get_model().frame_shape_interner.get_n_structures(),
              2u);
}

TEST(D3D8FrontendPasses, ASurfaceThatIsNotKnownIsCountedAndTheTargetStays)
{
    Tests::D3D8Scene scene;

    scene.bind_back_buffer();
    scene.get_builder().set_render_target(0xDEAD0,
                                          scene.get_depth_surface() );
    scene.draw(1);
    scene.present();

    EXPECT_EQ(scene.get_harness().get_frontend().get_n_unfollowed_calls(),
              1u);
    EXPECT_EQ(scene.get_harness().get_model().frame_record_vec.at(0).n_passes,
              1u);
}

TEST(D3D8FrontendPasses, ANullDepthSurfaceLeavesTheDepthBufferAlone)
{
    Tests::D3D8Scene scene;

    scene.bind_back_buffer();
    scene.get_builder().set_render_target(scene.get_back_buffer_surface(),
                                          0);
    scene.draw(1);
    scene.present();

    EXPECT_NE(scene.get_harness().get_model().pass_structure_interner.get_structure(0).depth_stencil_view_key,
              0u);
}

TEST(D3D8FrontendPasses, EveryPresentIsAFrame)
{
    Tests::D3D8Scene scene;

    for (uint32_t n_frame = 0;
                  n_frame < 4;
                ++n_frame)
    {
        scene.bind_back_buffer();
        scene.draw(n_frame + 1);
        scene.present();
    }

    const auto& frame_record_vec = scene.get_harness().get_model().frame_record_vec;

    ASSERT_EQ(frame_record_vec.size(),
              4u);
    EXPECT_EQ(frame_record_vec.at(3).n_draws,
              4u);
    EXPECT_EQ(frame_record_vec.at(0).shape_id,
              frame_record_vec.at(3).shape_id);
}

TEST(D3D8FrontendPasses, Fr08FramesAreTheKindsOfPassesTheyWereRead)
{
    Tests::D3D8FrontendHarness harness;

    ASSERT_TRUE(harness.feed_file(Tests::DataFixtures::get_fr08_slice_filename().c_str() ));

    const auto& model = harness.get_model();

    /* Frames 0-3 of the capture, 100, 101, 14364, 14365, 219679 and 219680. */
    const uint32_t n_passes_array[] = {1, 1, 1, 1, 1, 1, 3, 3, 8, 8};
    const uint32_t n_draws_array [] = {3, 3, 1, 1, 1, 1, 81, 81, 153, 153};

    ASSERT_EQ(model.frame_record_vec.size(),
              10u);

    for (size_t n_frame = 0;
                n_frame < model.frame_record_vec.size();
              ++n_frame)
    {
        EXPECT_EQ(model.frame_record_vec.at(n_frame).n_passes,
                  n_passes_array[n_frame]) << "frame " << n_frame;
        EXPECT_EQ(model.frame_record_vec.at(n_frame).n_draws,
                  n_draws_array[n_frame])  << "frame " << n_frame;
    }

    /* Frames of one kind are of one shape, whatever else differs. */
    EXPECT_EQ(model.frame_record_vec.at(0).shape_id,
              model.frame_record_vec.at(1).shape_id);
    EXPECT_EQ(model.frame_record_vec.at(2).shape_id,
              model.frame_record_vec.at(5).shape_id);
    EXPECT_EQ(model.frame_record_vec.at(6).shape_id,
              model.frame_record_vec.at(7).shape_id);
    EXPECT_EQ(model.frame_record_vec.at(8).shape_id,
              model.frame_record_vec.at(9).shape_id);
    EXPECT_EQ(model.frame_shape_interner.get_n_structures(),
              4u);
    EXPECT_EQ(model.pass_structure_interner.get_n_structures(),
              13u);

    EXPECT_EQ(harness.get_frontend().get_n_unfollowed_calls(),
              0u);
    EXPECT_EQ(model.n_key_collisions,
              0u);
}

TEST(D3D8FrontendPasses, Fr08SceneRenderedToATextureIsFollowedByPostProcessingPasses)
{
    Tests::D3D8FrontendHarness harness;

    ASSERT_TRUE(harness.feed_file(Tests::DataFixtures::get_fr08_slice_filename().c_str() ));

    const auto& model     = harness.get_model();
    const auto  entry_ptr = model.representative_store.get_entry_ptr_vec(model.frame_record_vec.at(8).shape_id).at(0);

    ASSERT_EQ(entry_ptr->detail.pass_instance_vec.size(),
              8u);

    /* The scene is drawn to a 512x512 texture with a clear; six passes of one kind of draw follow; the last one
     * renders to the back buffer.
     */
    const auto& scene_pass = model.pass_structure_interner.get_structure(entry_ptr->detail.pass_instance_vec.at(0).structure_id);
    const auto& last_pass  = model.pass_structure_interner.get_structure(entry_ptr->detail.pass_instance_vec.at(7).structure_id);

    EXPECT_TRUE(scene_pass.clears_color);
    EXPECT_EQ  (scene_pass.viewport.width,
                512.0f);
    EXPECT_GT  (scene_pass.draw_structure_key_vec.size(),
                1u);
    EXPECT_EQ  (last_pass.viewport.width,
                800.0f);

    for (size_t n_pass = 1;
                n_pass < 7;
              ++n_pass)
    {
        const auto& post_pass = model.pass_structure_interner.get_structure(entry_ptr->detail.pass_instance_vec.at(n_pass).structure_id);

        EXPECT_EQ(post_pass.viewport.width,
                  512.0f) << "pass " << n_pass;
        EXPECT_EQ(post_pass.draw_structure_key_vec.size(),
                  1u)     << "pass " << n_pass;
    }
}
