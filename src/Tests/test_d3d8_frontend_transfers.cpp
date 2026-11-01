/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the D3D8 front end following what is written to buffers and textures, and what is copied.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/hasher.h"
#include "d3d8_scene.h"
#include "data_fixtures.h"

TEST(D3D8FrontendTransfers, WhatIsWrittenToABufferIsAnUploadOfIt)
{
    Tests::D3D8CallBuilder     builder;
    Tests::D3D8FrontendHarness harness;
    const std::vector<uint8_t> data_u8_vec = {1, 2, 3, 4};

    builder.create_device();

    const auto vertex_buffer = builder.create_vertex_buffer(64, 0x152);

    builder.unlock_vertex_buffer(vertex_buffer,
                                 data_u8_vec);
    builder.present();
    harness.feed(builder);

    const auto& model         = harness.get_model();
    const auto  entry_ptr_vec = model.representative_store.get_entry_ptr_vec(model.frame_record_vec.at(0).shape_id);

    ASSERT_EQ(entry_ptr_vec.size(),
              1u);
    ASSERT_EQ(entry_ptr_vec.at(0)->detail.transfer_record_vec.size(),
              1u);

    const auto& transfer = entry_ptr_vec.at(0)->detail.transfer_record_vec.at(0);

    EXPECT_EQ(transfer.kind,
              Analysis::APIDumpAnalysis::TransferKind::UPLOAD);
    EXPECT_EQ(transfer.destination.resource_id,
              Analysis::APIDumpAnalysis::make_resource_id(3) );
    EXPECT_EQ(transfer.data.n_bytes,
              4u);
    EXPECT_EQ(transfer.data.content_hash,
              Analysis::APIDumpAnalysis::Hasher::hash_bytes(data_u8_vec.data(),
                                                            data_u8_vec.size() ));
}

TEST(D3D8FrontendTransfers, WhatIsWrittenToATextureLevelIsAnUploadOfThatLevel)
{
    Tests::D3D8CallBuilder     builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const auto texture = builder.create_texture(64, 64, 3, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);

    builder.unlock_texture_rect(texture,
                                2,
                                {1, 2});
    builder.present();
    harness.feed(builder);

    const auto& model         = harness.get_model();
    const auto  entry_ptr_vec = model.representative_store.get_entry_ptr_vec(model.frame_record_vec.at(0).shape_id);

    ASSERT_EQ(entry_ptr_vec.at(0)->detail.transfer_record_vec.size(),
              1u);
    EXPECT_EQ(entry_ptr_vec.at(0)->detail.transfer_record_vec.at(0).destination.mip_level,
              2u);
    EXPECT_EQ(entry_ptr_vec.at(0)->detail.transfer_record_vec.at(0).destination.resource_id,
              Analysis::APIDumpAnalysis::make_resource_id(3) );
}

TEST(D3D8FrontendTransfers, WritingToABufferBetweenDrawsDoesNotEndThePass)
{
    Tests::D3D8Scene scene;

    scene.bind_back_buffer();
    scene.draw(1);
    scene.get_builder().unlock_vertex_buffer(scene.get_vertex_buffer(),
                                             {1, 2, 3});
    scene.get_builder().unlock_index_buffer (scene.get_index_buffer(),
                                             {4, 5});
    scene.draw(1);
    scene.present();

    EXPECT_EQ(scene.get_harness().get_model().frame_record_vec.at(0).n_passes,
              1u);
    EXPECT_EQ(scene.get_harness().get_model().frame_record_vec.at(0).n_draws,
              2u);
}

TEST(D3D8FrontendTransfers, UpdatingATextureIsACopy)
{
    Tests::D3D8CallBuilder     builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const auto source      = builder.create_texture(64, 64, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);
    const auto destination = builder.create_texture(64, 64, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);

    builder.update_texture(source,
                           destination);
    builder.present();
    harness.feed(builder);

    const auto& model         = harness.get_model();
    const auto  entry_ptr_vec = model.representative_store.get_entry_ptr_vec(model.frame_record_vec.at(0).shape_id);

    ASSERT_EQ(entry_ptr_vec.at(0)->detail.transfer_record_vec.size(),
              1u);

    const auto& transfer = entry_ptr_vec.at(0)->detail.transfer_record_vec.at(0);

    EXPECT_EQ(transfer.kind,
              Analysis::APIDumpAnalysis::TransferKind::COPY);
    EXPECT_EQ(transfer.source.resource_id,
              Analysis::APIDumpAnalysis::make_resource_id(3) );
    EXPECT_EQ(transfer.destination.resource_id,
              Analysis::APIDumpAnalysis::make_resource_id(4) );
}

TEST(D3D8FrontendTransfers, CopyingBetweenSurfacesIsACopyOfTheirTextures)
{
    Tests::D3D8CallBuilder     builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const auto source      = builder.create_texture(64, 64, 2, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);
    const auto destination = builder.create_texture(32, 32, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);
    const auto source_level      = builder.get_surface_level(source, 1);
    const auto destination_level = builder.get_surface_level(destination, 0);

    builder.copy_rects(source_level,
                       destination_level);
    builder.present();
    harness.feed(builder);

    const auto& model         = harness.get_model();
    const auto  entry_ptr_vec = model.representative_store.get_entry_ptr_vec(model.frame_record_vec.at(0).shape_id);

    ASSERT_EQ(entry_ptr_vec.at(0)->detail.transfer_record_vec.size(),
              1u);
    EXPECT_EQ(entry_ptr_vec.at(0)->detail.transfer_record_vec.at(0).source.mip_level,
              1u);
    EXPECT_EQ(entry_ptr_vec.at(0)->detail.transfer_record_vec.at(0).destination.resource_id,
              Analysis::APIDumpAnalysis::make_resource_id(4) );
}

TEST(D3D8FrontendTransfers, CopyingFromWhatThePassRendersToEndsThePass)
{
    Tests::D3D8Scene scene;

    const auto texture       = scene.get_builder().create_texture(512, 512, 1, Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET, Interception::Hooks::D3D8::D3DFMT_X8R8G8B8);
    const auto level         = scene.get_builder().get_surface_level(texture, 0);
    const auto depth_surface = scene.get_builder().create_depth_stencil_surface(512, 512, Interception::Hooks::D3D8::D3DFMT_D24S8);
    const auto copy          = scene.get_builder().create_texture(512, 512, 1, 0, Interception::Hooks::D3D8::D3DFMT_X8R8G8B8);
    const auto copy_level    = scene.get_builder().get_surface_level(copy, 0);

    scene.get_builder().set_render_target(level,
                                          depth_surface);
    scene.draw(1);
    scene.get_builder().copy_rects(level,
                                   copy_level);
    scene.draw(1);
    scene.present();

    EXPECT_EQ(scene.get_harness().get_model().frame_record_vec.at(0).n_passes,
              2u);
}

TEST(D3D8FrontendTransfers, Fr08FramesHaveTheUploadsOfTheirBuffers)
{
    Tests::D3D8FrontendHarness harness;

    ASSERT_TRUE(harness.feed_file(Tests::DataFixtures::get_fr08_slice_filename().c_str() ));

    const auto& model     = harness.get_model();
    const auto  entry_ptr = model.representative_store.get_entry_ptr_vec(model.frame_record_vec.at(6).shape_id).at(0);

    /* The vertex and index buffers are written to before the draws of the frame. */
    EXPECT_GT(entry_ptr->detail.transfer_record_vec.size(),
              0u);

    for (const auto& transfer : entry_ptr->detail.transfer_record_vec)
    {
        EXPECT_EQ(transfer.kind,
                  Analysis::APIDumpAnalysis::TransferKind::UPLOAD);
    }

    EXPECT_EQ(harness.get_frontend().get_n_unsupported_calls(),
              0u);
    EXPECT_EQ(harness.get_frontend().get_n_unfollowed_calls(),
              0u);
}
