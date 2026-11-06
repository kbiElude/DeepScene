/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the twins that the converter makes of the surfaces a Direct3D 7 application renders to: which surfaces get one, what it is like,
 * what is attached to it, and when it goes. The surfaces are fakes with virtual tables (see fake_direct_draw.h).
 */
#include <gtest/gtest.h>
#include "Interception/Core/D3D7Utils/constants.h"
#include "d3d7_surface_twins.h"
#include "fake_direct_draw.h"
#include <string>

namespace
{
    namespace Caps = Interception::Core::D3D7Utils;

    Interception::Hooks::D3D7::DDSURFACEDESC2 make_description(const uint32_t& in_caps,
                                                               const uint32_t& in_width,
                                                               const uint32_t& in_height)
    {
        Interception::Hooks::D3D7::DDSURFACEDESC2 result = {};

        result.dwSize        = static_cast<DWORD>(sizeof(result) );
        result.dwFlags       = Caps::DDSD_CAPS | Caps::DDSD_HEIGHT | Caps::DDSD_WIDTH;
        result.dwHeight      = in_height;
        result.dwWidth       = in_width;
        result.ddsCaps.dwCaps = in_caps;

        return result;
    }

    /* What the application did to the DirectDraw object, as the callback sees it after the call. */
    void post_create_surface(Stereo::ConverterDLL::D3D7SurfaceTwins* inout_twins_ptr,
                             Tests::FakeDirectDraw*                  in_direct_draw_ptr,
                             Tests::FakeSurface*                     in_surface_ptr)
    {
        const APIInterceptor::APIFunctionArgument argument_vec4[4] = {APIInterceptor::APIFunctionArgument::create_void_ptr(in_direct_draw_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_data_chunk_id(0),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(in_surface_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr)};
        const auto                                result            = APIInterceptor::APIFunctionArgument::create_i32(0);

        inout_twins_ptr->on_post(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE,
                                 4,
                                 argument_vec4,
                                &result);
    }

    void post_get_attached_surface(Stereo::ConverterDLL::D3D7SurfaceTwins* inout_twins_ptr,
                                   Tests::FakeSurface*                     in_surface_ptr,
                                   Tests::FakeSurface*                     in_attached_ptr)
    {
        const APIInterceptor::APIFunctionArgument argument_vec3[3] = {APIInterceptor::APIFunctionArgument::create_void_ptr(in_surface_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_data_chunk_id(0),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(in_attached_ptr)};
        const auto                                result            = APIInterceptor::APIFunctionArgument::create_i32(0);

        inout_twins_ptr->on_post(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE,
                                 3,
                                 argument_vec3,
                                &result);
    }

    void post_add_attached_surface(Stereo::ConverterDLL::D3D7SurfaceTwins* inout_twins_ptr,
                                   Tests::FakeSurface*                     in_surface_ptr,
                                   Tests::FakeSurface*                     in_attached_ptr)
    {
        const APIInterceptor::APIFunctionArgument argument_vec2[2] = {APIInterceptor::APIFunctionArgument::create_void_ptr(in_surface_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(in_attached_ptr)};
        const auto                                result            = APIInterceptor::APIFunctionArgument::create_i32(0);

        inout_twins_ptr->on_post(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE,
                                 2,
                                 argument_vec2,
                                &result);
    }

    void post_release(Stereo::ConverterDLL::D3D7SurfaceTwins* inout_twins_ptr,
                      Tests::FakeSurface*                     in_surface_ptr,
                      const uint32_t&                         in_n_references)
    {
        const APIInterceptor::APIFunctionArgument argument            = APIInterceptor::APIFunctionArgument::create_void_ptr(in_surface_ptr);
        const auto                                result              = APIInterceptor::APIFunctionArgument::create_u32(in_n_references);

        inout_twins_ptr->on_post(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_RELEASE,
                                 1,
                                &argument,
                                &result);
    }
}

TEST(D3D7SurfaceTwins, ASurfaceTheApplicationRendersToGetsATwinOfTheSameSizeAndOtherSurfacesDoNot)
{
    auto                                   direct_draw_ptr = Tests::make_direct_draw();
    Stereo::ConverterDLL::D3D7SurfaceTwins twins;

    Tests::FakeSurface* const render_target_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_3DDEVICE | Caps::DDSCAPS_TEXTURE,
                                                                                       1024,
                                                                                       512) );
    Tests::FakeSurface* const texture_ptr       = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_TEXTURE,
                                                                                       256,
                                                                                       256) );

    post_create_surface(&twins,
                        direct_draw_ptr.get(),
                        render_target_ptr);
    post_create_surface(&twins,
                        direct_draw_ptr.get(),
                        texture_ptr);

    ASSERT_NE(twins.get_twin_ptr(render_target_ptr),
              nullptr);
    EXPECT_EQ(twins.get_twin_ptr(texture_ptr),
              nullptr);
    EXPECT_EQ(twins.get_n_twins(),
              1u);

    /* The twin was made again, as large as the surface. */
    const auto* twin_ptr = static_cast<Tests::FakeSurface*>(twins.get_twin_ptr(render_target_ptr) );

    EXPECT_NE(twin_ptr,
              render_target_ptr);
    EXPECT_EQ(twin_ptr->description.dwWidth,
              1024u);
    EXPECT_EQ(twin_ptr->description.dwHeight,
              512u);
    EXPECT_NE(twin_ptr->description.ddsCaps.dwCaps & Caps::DDSCAPS_3DDEVICE,
              0u);
}

TEST(D3D7SurfaceTwins, TheBackBufferThatThePrimarySurfaceGivesGetsATwinWithNoChainInIt)
{
    auto                                   direct_draw_ptr = Tests::make_direct_draw();
    Stereo::ConverterDLL::D3D7SurfaceTwins twins;

    Tests::FakeSurface* const primary_ptr     = Tests::make_surface(direct_draw_ptr.get(),
                                                                    make_description(Caps::DDSCAPS_PRIMARYSURFACE | Caps::DDSCAPS_FLIP | Caps::DDSCAPS_COMPLEX,
                                                                                     0,
                                                                                     0) );
    Tests::FakeSurface* const back_buffer_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                    make_description(Caps::DDSCAPS_BACKBUFFER | Caps::DDSCAPS_FLIP | Caps::DDSCAPS_COMPLEX | Caps::DDSCAPS_3DDEVICE,
                                                                                     640,
                                                                                     480) );

    post_create_surface(&twins,
                        direct_draw_ptr.get(),
                        primary_ptr);

    /* The surface that is shown has no twin: the right eye is not shown on the screen. */
    EXPECT_EQ(twins.get_twin_ptr(primary_ptr),
              nullptr);

    post_get_attached_surface(&twins,
                              primary_ptr,
                              back_buffer_ptr);

    auto* const twin_ptr = static_cast<Tests::FakeSurface*>(twins.get_twin_ptr(back_buffer_ptr) );

    ASSERT_NE(twin_ptr,
              nullptr);
    EXPECT_TRUE(twins.is_back_buffer(back_buffer_ptr) );
    EXPECT_FALSE(twins.is_back_buffer(twin_ptr) );
    EXPECT_EQ(twin_ptr->description.dwWidth,
              640u);

    /* What makes a surface part of a chain is not for its copy to have; it is a plain surface which is rendered to. */
    EXPECT_EQ(twin_ptr->description.ddsCaps.dwCaps & (Caps::DDSCAPS_BACKBUFFER | Caps::DDSCAPS_FLIP | Caps::DDSCAPS_COMPLEX | Caps::DDSCAPS_PRIMARYSURFACE),
              0u);
    EXPECT_NE(twin_ptr->description.ddsCaps.dwCaps & Caps::DDSCAPS_3DDEVICE,
              0u);
    EXPECT_NE(twin_ptr->description.ddsCaps.dwCaps & Caps::DDSCAPS_OFFSCREENPLAIN,
              0u);
}

TEST(D3D7SurfaceTwins, ADepthBufferIsAttachedToTheTwinOfWhatItIsAttachedTo)
{
    auto                                   direct_draw_ptr = Tests::make_direct_draw();
    Stereo::ConverterDLL::D3D7SurfaceTwins twins;

    Tests::FakeSurface* const render_target_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_3DDEVICE,
                                                                                       512,
                                                                                       512) );
    Tests::FakeSurface* const depth_ptr         = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_ZBUFFER,
                                                                                       512,
                                                                                       512) );

    post_create_surface(&twins,
                        direct_draw_ptr.get(),
                        render_target_ptr);
    post_create_surface(&twins,
                        direct_draw_ptr.get(),
                        depth_ptr);

    ASSERT_NE(twins.get_twin_ptr(depth_ptr),
              nullptr);

    post_add_attached_surface(&twins,
                              render_target_ptr,
                              depth_ptr);

    const auto* render_target_twin_ptr = static_cast<Tests::FakeSurface*>(twins.get_twin_ptr(render_target_ptr) );

    ASSERT_EQ(render_target_twin_ptr->attached_ptr_vec.size(),
              1u);
    EXPECT_EQ(static_cast<void*>(render_target_twin_ptr->attached_ptr_vec.at(0) ),
              twins.get_twin_ptr(depth_ptr) );
}

TEST(D3D7SurfaceTwins, ATwinGoesWhenTheSurfaceDoesAndNotBefore)
{
    auto                                   direct_draw_ptr = Tests::make_direct_draw();
    Stereo::ConverterDLL::D3D7SurfaceTwins twins;

    Tests::FakeSurface* const render_target_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_3DDEVICE,
                                                                                       64,
                                                                                       64) );

    post_create_surface(&twins,
                        direct_draw_ptr.get(),
                        render_target_ptr);

    auto* const twin_ptr = static_cast<Tests::FakeSurface*>(twins.get_twin_ptr(render_target_ptr) );

    ASSERT_NE(twin_ptr,
              nullptr);

    /* The application let go of one of its references: the surface is still there. */
    post_release(&twins,
                 render_target_ptr,
                 1);

    EXPECT_NE(twins.get_twin_ptr(render_target_ptr),
              nullptr);
    EXPECT_EQ(twin_ptr->n_references,
              1u);

    /* The last one: the twin is let go of with it. */
    post_release(&twins,
                 render_target_ptr,
                 0);

    EXPECT_EQ(twins.get_twin_ptr(render_target_ptr),
              nullptr);
    EXPECT_EQ(twin_ptr->n_references,
              0u);
}

TEST(D3D7SurfaceTwins, ATwinWhichCannotBeMadeIsCountedAndTheApplicationGoesOn)
{
    auto                                   direct_draw_ptr = Tests::make_direct_draw();
    Stereo::ConverterDLL::D3D7SurfaceTwins twins;

    Tests::FakeSurface* const render_target_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_3DDEVICE,
                                                                                       64,
                                                                                       64) );

    direct_draw_ptr->fails_to_create = true;

    post_create_surface(&twins,
                        direct_draw_ptr.get(),
                        render_target_ptr);

    EXPECT_EQ(twins.get_twin_ptr(render_target_ptr),
              nullptr);
    EXPECT_EQ(twins.get_n_failed_twins(),
              1u);
    EXPECT_EQ(twins.get_n_twins(),
              0u);
}

TEST(D3D7SurfaceTwins, ACallThatFailedMadeNoTwin)
{
    auto                                   direct_draw_ptr = Tests::make_direct_draw();
    Stereo::ConverterDLL::D3D7SurfaceTwins twins;

    Tests::FakeSurface* const render_target_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_3DDEVICE,
                                                                                       64,
                                                                                       64) );

    const APIInterceptor::APIFunctionArgument argument_vec4[4] = {APIInterceptor::APIFunctionArgument::create_void_ptr(direct_draw_ptr.get() ),
                                                                  APIInterceptor::APIFunctionArgument::create_data_chunk_id(0),
                                                                  APIInterceptor::APIFunctionArgument::create_void_ptr(render_target_ptr),
                                                                  APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr)};
    const auto                                failure           = APIInterceptor::APIFunctionArgument::create_i32(static_cast<int>(0x88760218) );

    twins.on_post(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE,
                  4,
                  argument_vec4,
                 &failure);

    EXPECT_EQ(twins.get_n_twins(),
              0u);
    EXPECT_EQ(twins.get_n_failed_twins(),
              0u);
}
