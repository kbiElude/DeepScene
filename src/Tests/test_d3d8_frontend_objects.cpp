/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the D3D8 front end following the objects: what is created, what a surface is a part of, and when things go.
 */
#include <gtest/gtest.h>
#include "d3d8_frontend_harness.h"
#include "data_fixtures.h"
#include <string.h>

TEST(D3D8FrontendObjects, DeviceCreationMakesTheBackBufferAndTheDepthBuffer)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    harness.feed(builder);

    const auto& resource_vec = harness.get_model().resource_vec;

    ASSERT_EQ(resource_vec.size(),
              2u);

    EXPECT_EQ(resource_vec.at(0).description.kind,
              Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE);
    EXPECT_EQ(resource_vec.at(0).description.width,
              800u);
    EXPECT_EQ(resource_vec.at(0).description.height,
              600u);
    EXPECT_EQ(resource_vec.at(0).description.format.raw_value,
              static_cast<uint32_t>(Interception::Hooks::D3D8::D3DFMT_X8R8G8B8) );
    EXPECT_EQ(resource_vec.at(0).description.format.format_class,
              Analysis::APIDumpAnalysis::FormatClass::COLOR);
    EXPECT_TRUE(resource_vec.at(0).description.is_render_target);
    EXPECT_EQ(resource_vec.at(0).debug_name,
              "BackBuffer");

    EXPECT_EQ(resource_vec.at(1).description.format.format_class,
              Analysis::APIDumpAnalysis::FormatClass::DEPTH_STENCIL);
    EXPECT_TRUE(resource_vec.at(1).description.is_depth_stencil);
    EXPECT_EQ(resource_vec.at(1).debug_name,
              "AutoDepthStencil");

    EXPECT_EQ(harness.get_frontend().get_n_unfollowed_calls(),
              0u);
}

TEST(D3D8FrontendObjects, ATextureIsDescribedByTheCallThatCreatesIt)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    builder.create_texture(512, 256, 1, Interception::Hooks::D3D8::D3DUSAGE_RENDERTARGET, Interception::Hooks::D3D8::D3DFMT_X8R8G8B8);
    harness.feed(builder);

    const auto& resource = harness.get_model().resource_vec.at(2);

    EXPECT_EQ(resource.description.kind,
              Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D);
    EXPECT_EQ(resource.description.width,
              512u);
    EXPECT_EQ(resource.description.height,
              256u);
    EXPECT_EQ(resource.description.mip_level_count,
              1u);
    EXPECT_TRUE(resource.description.is_render_target);
    EXPECT_FALSE(resource.description.is_depth_stencil);
    EXPECT_EQ(resource.debug_name,
              "Texture#1");
}

TEST(D3D8FrontendObjects, ZeroLevelsMeansTheWholeMipChain)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    builder.create_texture(256, 64, 0, 0, Interception::Hooks::D3D8::D3DFMT_DXT1);
    harness.feed(builder);

    const auto& resource = harness.get_model().resource_vec.at(2);

    EXPECT_EQ(resource.description.mip_level_count,
              9u);
    EXPECT_EQ(resource.description.format.format_class,
              Analysis::APIDumpAnalysis::FormatClass::COMPRESSED);
}

TEST(D3D8FrontendObjects, BuffersAreDescribedByTheirSizeAndUsage)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    builder.create_vertex_buffer(294912, 0x152);
    builder.create_index_buffer (98304);
    harness.feed(builder);

    const auto& resource_vec = harness.get_model().resource_vec;

    EXPECT_EQ(resource_vec.at(2).description.kind,
              Analysis::APIDumpAnalysis::ResourceKind::BUFFER_VERTEX);
    EXPECT_EQ(resource_vec.at(2).description.size_in_bytes,
              294912u);
    EXPECT_TRUE(resource_vec.at(2).description.is_dynamic);
    EXPECT_EQ(resource_vec.at(2).debug_name,
              "VertexBuffer#1");

    EXPECT_EQ(resource_vec.at(3).description.kind,
              Analysis::APIDumpAnalysis::ResourceKind::BUFFER_INDEX);
    EXPECT_EQ(resource_vec.at(3).description.size_in_bytes,
              98304u);
    EXPECT_EQ(resource_vec.at(3).description.format.raw_value,
              static_cast<uint32_t>(Interception::Hooks::D3D8::D3DFMT_INDEX16) );
    EXPECT_EQ(resource_vec.at(3).debug_name,
              "IndexBuffer#1");
}

TEST(D3D8FrontendObjects, ADepthStencilSurfaceIsADepthResource)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    builder.create_depth_stencil_surface(512, 512, Interception::Hooks::D3D8::D3DFMT_D24S8);
    harness.feed(builder);

    const auto& resource = harness.get_model().resource_vec.at(2);

    EXPECT_TRUE(resource.description.is_depth_stencil);
    EXPECT_EQ(resource.description.format.format_class,
              Analysis::APIDumpAnalysis::FormatClass::DEPTH_STENCIL);
    EXPECT_EQ(resource.debug_name,
              "Surface#1");
}

TEST(D3D8FrontendObjects, TheSameKindOfThingsMadeTheSameWayHaveTheSameKey)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    builder.create_texture(64, 64, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);
    builder.create_texture(64, 64, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);
    builder.create_texture(64, 64, 1, 0, Interception::Hooks::D3D8::D3DFMT_X8R8G8B8);
    harness.feed(builder);

    const auto& resource_vec = harness.get_model().resource_vec;

    EXPECT_EQ(resource_vec.at(2).match_key,
              resource_vec.at(3).match_key);
    EXPECT_NE(resource_vec.at(2).match_key,
              resource_vec.at(4).match_key);
}

TEST(D3D8FrontendObjects, ASurfaceLevelIsAPartOfItsTexture)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const auto texture = builder.create_texture(64, 64, 3, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);
    const auto level_0 = builder.get_surface_level(texture, 0);
    const auto level_2 = builder.get_surface_level(texture, 2);

    harness.feed(builder);

    const auto* level_0_ptr = harness.get_frontend().get_tracker().get_object_ptr(level_0);
    const auto* level_2_ptr = harness.get_frontend().get_tracker().get_object_ptr(level_2);

    ASSERT_NE(level_0_ptr,
              nullptr);
    ASSERT_NE(level_2_ptr,
              nullptr);

    EXPECT_FALSE(level_0_ptr->owns_resource);
    EXPECT_EQ   (level_0_ptr->view.resource_id,
                 Analysis::APIDumpAnalysis::make_resource_id(3) );
    EXPECT_EQ   (level_0_ptr->view.mip_level,
                 0u);
    EXPECT_EQ   (level_2_ptr->view.resource_id,
                 Analysis::APIDumpAnalysis::make_resource_id(3) );
    EXPECT_EQ   (level_2_ptr->view.mip_level,
                 2u);

    /* A surface is not a resource of its own. */
    EXPECT_EQ(harness.get_model().resource_vec.size(),
              3u);
}

TEST(D3D8FrontendObjects, TheRenderTargetAndTheDepthBufferOfTheDeviceAreTheBackBufferAndTheDepthBuffer)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const auto render_target = builder.get_render_target();
    const auto depth_stencil = builder.get_depth_stencil_surface();
    const auto back_buffer   = builder.get_back_buffer(0);

    harness.feed(builder);

    const auto& tracker = harness.get_frontend().get_tracker();

    ASSERT_NE(tracker.get_object_ptr(render_target),
              nullptr);
    ASSERT_NE(tracker.get_object_ptr(depth_stencil),
              nullptr);
    ASSERT_NE(tracker.get_object_ptr(back_buffer),
              nullptr);

    EXPECT_EQ(tracker.get_object_ptr(render_target)->view.resource_id,
              Analysis::APIDumpAnalysis::make_resource_id(1) );
    EXPECT_EQ(tracker.get_object_ptr(back_buffer)->view.resource_id,
              Analysis::APIDumpAnalysis::make_resource_id(1) );
    EXPECT_EQ(tracker.get_object_ptr(depth_stencil)->view.resource_id,
              Analysis::APIDumpAnalysis::make_resource_id(2) );
}

TEST(D3D8FrontendObjects, TheLastReleaseDestroysTheResource)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const auto texture = builder.create_texture(64, 64, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);

    builder.release_texture(texture,
                            1); /* in_n_references_left */
    harness.feed(builder);

    EXPECT_FALSE(harness.get_model().resource_vec.at(2).is_destroyed);
    EXPECT_NE   (harness.get_frontend().get_tracker().get_object_ptr(texture),
                 nullptr);

    builder.release_texture(texture);
    harness.feed(builder);

    EXPECT_TRUE(harness.get_model().resource_vec.at(2).is_destroyed);
    EXPECT_EQ  (harness.get_frontend().get_tracker().get_object_ptr(texture),
                nullptr);
}

TEST(D3D8FrontendObjects, ReleasingASurfaceDoesNotDestroyItsTexture)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const auto texture = builder.create_texture(64, 64, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);
    const auto surface = builder.get_surface_level(texture, 0);

    builder.release_surface(surface);
    harness.feed(builder);

    EXPECT_FALSE(harness.get_model().resource_vec.at(2).is_destroyed);
    EXPECT_EQ   (harness.get_frontend().get_tracker().get_object_ptr(surface),
                 nullptr);
    EXPECT_NE   (harness.get_frontend().get_tracker().get_object_ptr(texture),
                 nullptr);
}

TEST(D3D8FrontendObjects, AnAddressThatIsReusedIsANewObject)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const auto first = builder.create_texture(64, 64, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);

    builder.release_texture(first);

    const auto second = builder.create_texture(32, 32, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8, first);

    harness.feed(builder);

    ASSERT_EQ(second,
              first);

    const auto& resource_vec = harness.get_model().resource_vec;
    const auto* object_ptr   = harness.get_frontend().get_tracker().get_object_ptr(first);

    ASSERT_EQ(resource_vec.size(),
              4u);
    ASSERT_NE(object_ptr,
              nullptr);

    EXPECT_TRUE(resource_vec.at(2).is_destroyed);
    EXPECT_FALSE(resource_vec.at(3).is_destroyed);
    EXPECT_EQ  (object_ptr->view.resource_id,
                resource_vec.at(3).id);
    EXPECT_EQ  (resource_vec.at(3).description.width,
                32u);
    EXPECT_EQ  (resource_vec.at(3).debug_name,
                "Texture#2");
}

TEST(D3D8FrontendObjects, AFailedCreationMakesNothing)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();

    const auto address = builder.allocate_address();
    const auto device  = APIInterceptor::APIFunctionArgument::create_void_ptr(reinterpret_cast<void*>(static_cast<uintptr_t>(builder.get_device_address() )) );
    const auto slot    = APIInterceptor::APIFunctionArgument::create_void_ptr(reinterpret_cast<void*>(static_cast<uintptr_t>(address) ));

    builder.add_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATETEXTURE,
                     {device,
                      APIInterceptor::APIFunctionArgument::create_u32(64),
                      APIInterceptor::APIFunctionArgument::create_u32(64),
                      APIInterceptor::APIFunctionArgument::create_u32(1),
                      APIInterceptor::APIFunctionArgument::create_u32(0),
                      APIInterceptor::APIFunctionArgument::create_u32(Interception::Hooks::D3D8::D3DFMT_A8R8G8B8),
                      APIInterceptor::APIFunctionArgument::create_u32(Interception::Hooks::D3D8::D3DPOOL_DEFAULT),
                      slot},
                     {},
                     APIInterceptor::APIFunctionArgument::create_i32(-2005530516) ); /* D3DERR_INVALIDCALL */
    harness.feed(builder);

    EXPECT_EQ(harness.get_model().resource_vec.size(),
              2u);
}

TEST(D3D8FrontendObjects, AUnknownTextureIsCountedNotFollowed)
{
    Tests::D3D8CallBuilder builder;
    Tests::D3D8FrontendHarness harness;

    builder.create_device();
    builder.get_surface_level(0xDEAD0, 0);
    harness.feed(builder);

    EXPECT_EQ(harness.get_frontend().get_n_unfollowed_calls(),
              1u);
}

TEST(D3D8FrontendObjects, TheObjectsOfFr08AreAllFollowedAndNamedLikeThePrinterDoes)
{
    Tests::D3D8FrontendHarness harness;

    ASSERT_TRUE(harness.feed_file(Tests::DataFixtures::get_fr08_slice_filename().c_str() ));

    const auto& resource_vec = harness.get_model().resource_vec;
    uint32_t    n_textures   = 0;

    /* The back buffer and the depth buffer, 68 textures, 16 index buffers, 13 vertex buffers, and a depth surface. */
    EXPECT_EQ(resource_vec.size(),
              100u);

    for (const auto& resource : resource_vec)
    {
        if (resource.description.kind == Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D &&
            resource.debug_name.compare(0, strlen("Texture#"), "Texture#") == 0)
        {
            n_textures++;

            EXPECT_EQ(resource.debug_name,
                      "Texture#" + std::to_string(n_textures) );
        }
    }

    EXPECT_EQ(n_textures,
              68u);
    EXPECT_EQ(harness.get_frontend().get_n_unfollowed_calls(),
              0u);
}
