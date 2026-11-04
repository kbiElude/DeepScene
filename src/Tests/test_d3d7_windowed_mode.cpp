/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what the converter does to a Direct3D 7 application which is to render to a window that is invisible: which calls it makes in the
 * place of the application's, and what it writes where the application is to get the result.
 */
#include <gtest/gtest.h>
#include "Common/callbacks.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "d3d7_windowed_mode.h"
#include "fake_direct_draw.h"

namespace
{
    namespace Caps = Interception::Core::D3D7Utils;

    constexpr uint32_t COOPERATIVE_LEVEL_NORMAL = 0x00000008;

    Interception::Hooks::D3D7::DDSURFACEDESC2 make_description(const uint32_t& in_caps,
                                                               const uint32_t& in_width,
                                                               const uint32_t& in_height)
    {
        Interception::Hooks::D3D7::DDSURFACEDESC2 result = {};

        result.dwSize         = static_cast<DWORD>(sizeof(result) );
        result.dwFlags        = Caps::DDSD_CAPS | Caps::DDSD_HEIGHT | Caps::DDSD_WIDTH;
        result.dwHeight       = in_height;
        result.dwWidth        = in_width;
        result.ddsCaps.dwCaps = in_caps;

        return result;
    }

    /* A windowed mode which is told that the images are shown elsewhere, with the size of an image of an eye if it is given. */
    Stereo::ConverterDLL::D3D7WindowedMode make_enabled_mode(const uint32_t& in_target_width  = 0,
                                                             const uint32_t& in_target_height = 0)
    {
        Stereo::ConverterDLL::D3D7WindowedMode result;

        result.set_is_enabled(true);
        result.set_target_eye_size(in_target_width,
                                   in_target_height);

        return result;
    }

    bool set_display_mode(Stereo::ConverterDLL::D3D7WindowedMode* inout_mode_ptr,
                          Tests::FakeDirectDraw*                  in_direct_draw_ptr,
                          const uint32_t&                         in_width,
                          const uint32_t&                         in_height)
    {
        const APIInterceptor::APIFunctionArgument argument_vec6[6] = {APIInterceptor::APIFunctionArgument::create_void_ptr(in_direct_draw_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(in_width),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(in_height),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(32),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(0),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(0)};

        return inout_mode_ptr->on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETDISPLAYMODE,
                                      6,
                                      argument_vec6);
    }

    /* The call of the application; what it would get is in the variable. Returns whether the call is to be made. */
    bool create_surface(Stereo::ConverterDLL::D3D7WindowedMode* inout_mode_ptr,
                        Tests::FakeDirectDraw*                  in_direct_draw_ptr,
                        const uint32_t&                         in_caps,
                        const uint32_t&                         in_width,
                        const uint32_t&                         in_height,
                        void**                                  out_surface_ptr_ptr)
    {
        const auto description = make_description(in_caps,
                                                  in_width,
                                                  in_height);
        const auto chunk_id    = APIInterceptor::register_data_chunk(&description,
                                                                     static_cast<uint32_t>(sizeof(description) ));
        const APIInterceptor::APIFunctionArgument argument_vec4[4] = {APIInterceptor::APIFunctionArgument::create_void_ptr(in_direct_draw_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_data_chunk_id(chunk_id),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(out_surface_ptr_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr)};

        const bool result = inout_mode_ptr->on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE,
                                                   4,
                                                   argument_vec4);

        APIInterceptor::release_data_chunk(chunk_id);

        return result;
    }

    constexpr uint32_t PRIMARY_WITH_CHAIN_CAPS = Caps::DDSCAPS_PRIMARYSURFACE | Caps::DDSCAPS_FLIP | Caps::DDSCAPS_COMPLEX;
}

TEST(D3D7WindowedMode, WhenItIsNotEnabledNothingIsChanged)
{
    auto                                   direct_draw_ptr = Tests::make_direct_draw();
    Stereo::ConverterDLL::D3D7WindowedMode mode;
    void*                                  surface_ptr     = nullptr;

    EXPECT_TRUE(set_display_mode(&mode,
                                 direct_draw_ptr.get(),
                                 640,
                                 480) );
    EXPECT_TRUE(create_surface(&mode,
                               direct_draw_ptr.get(),
                               PRIMARY_WITH_CHAIN_CAPS,
                               0,
                               0,
                              &surface_ptr) );
    EXPECT_EQ(surface_ptr,
              nullptr);
    EXPECT_EQ(mode.get_back_buffer_ptr(),
              nullptr);
}

TEST(D3D7WindowedMode, TheCooperativeLevelIsThatOfAWindowAndTheDisplayModeIsNotSet)
{
    auto                                   direct_draw_ptr = Tests::make_direct_draw();
    auto                                   mode            = make_enabled_mode();

    const APIInterceptor::APIFunctionArgument argument_vec3[3] = {APIInterceptor::APIFunctionArgument::create_void_ptr(direct_draw_ptr.get() ),
                                                                  APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                                                                  APIInterceptor::APIFunctionArgument::create_u32(0x00000013)}; /* exclusive and full screen */

    EXPECT_FALSE(mode.on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETCOOPERATIVELEVEL,
                             3,
                             argument_vec3) );

    ASSERT_TRUE(direct_draw_ptr->has_cooperative_level);
    EXPECT_EQ(direct_draw_ptr->cooperative_level,
              COOPERATIVE_LEVEL_NORMAL);

    EXPECT_FALSE(set_display_mode(&mode,
                                  direct_draw_ptr.get(),
                                  640,
                                  480) );
    EXPECT_EQ(direct_draw_ptr->n_display_mode_calls,
              0u);
}

TEST(D3D7WindowedMode, ThePrimarySurfaceWithAChainIsMadeWithoutOneAndItsBackBufferIsASurfaceOfTheSizeOfTheDisplayMode)
{
    auto  direct_draw_ptr = Tests::make_direct_draw();
    auto  mode            = make_enabled_mode();
    void* primary_ptr     = nullptr;

    EXPECT_FALSE(set_display_mode(&mode,
                                  direct_draw_ptr.get(),
                                  640,
                                  480) );
    EXPECT_FALSE(create_surface(&mode,
                                direct_draw_ptr.get(),
                                PRIMARY_WITH_CHAIN_CAPS,
                                0,
                                0,
                               &primary_ptr) );

    /* The application got a surface in the place where it asked for it: the primary one, of the window. */
    ASSERT_NE(primary_ptr,
              nullptr);
    ASSERT_EQ(direct_draw_ptr->created_description_vec.size(),
              2u);
    EXPECT_EQ(direct_draw_ptr->created_description_vec.at(0).ddsCaps.dwCaps,
              static_cast<DWORD>(Caps::DDSCAPS_PRIMARYSURFACE) );

    /* The other is the surface that it renders to: not a part of a chain, of the size that it asked the display mode for. */
    const auto& back_buffer_description = direct_draw_ptr->created_description_vec.at(1);

    EXPECT_EQ(back_buffer_description.dwWidth,
              640u);
    EXPECT_EQ(back_buffer_description.dwHeight,
              480u);
    EXPECT_NE(back_buffer_description.ddsCaps.dwCaps & Caps::DDSCAPS_3DDEVICE,
              0u);
    EXPECT_EQ(back_buffer_description.ddsCaps.dwCaps & (Caps::DDSCAPS_FLIP | Caps::DDSCAPS_COMPLEX | Caps::DDSCAPS_PRIMARYSURFACE),
              0u);
    EXPECT_NE(mode.get_back_buffer_ptr(),
              nullptr);
}

TEST(D3D7WindowedMode, TheApplicationGetsTheBackBufferWhereItAsksForTheOneOfTheChainAndAFlipShowsNothing)
{
    auto  direct_draw_ptr = Tests::make_direct_draw();
    auto  mode            = make_enabled_mode();
    void* primary_ptr     = nullptr;

    set_display_mode(&mode,
                     direct_draw_ptr.get(),
                     640,
                     480);
    create_surface(&mode,
                   direct_draw_ptr.get(),
                   PRIMARY_WITH_CHAIN_CAPS,
                   0,
                   0,
                  &primary_ptr);

    Interception::Hooks::D3D7::DDSCAPS2 caps  = {};
    void*                               found = nullptr;

    caps.dwCaps = Caps::DDSCAPS_BACKBUFFER;

    const auto chunk_id = APIInterceptor::register_data_chunk(&caps,
                                                              static_cast<uint32_t>(sizeof(caps) ));
    const APIInterceptor::APIFunctionArgument argument_vec3[3] = {APIInterceptor::APIFunctionArgument::create_void_ptr(primary_ptr),
                                                                  APIInterceptor::APIFunctionArgument::create_data_chunk_id(chunk_id),
                                                                  APIInterceptor::APIFunctionArgument::create_void_ptr(&found)};

    EXPECT_FALSE(mode.on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE,
                             3,
                             argument_vec3) );

    APIInterceptor::release_data_chunk(chunk_id);

    EXPECT_EQ(found,
              mode.get_back_buffer_ptr() );

    /* The application holds the reference that the call gave. */
    EXPECT_EQ(static_cast<Tests::FakeSurface*>(found)->n_add_refs,
              1u);

    /* A flip of the surface that is shown is not made: there is no chain to flip. */
    const APIInterceptor::APIFunctionArgument flip_argument_vec3[3] = {APIInterceptor::APIFunctionArgument::create_void_ptr(primary_ptr),
                                                                       APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                                                                       APIInterceptor::APIFunctionArgument::create_u32(0)};

    EXPECT_FALSE(mode.on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP,
                             3,
                             flip_argument_vec3) );
}

TEST(D3D7WindowedMode, APrimarySurfaceWithNoChainAndOtherSurfacesAreMadeAsTheApplicationMakesThem)
{
    auto  direct_draw_ptr = Tests::make_direct_draw();
    auto  mode            = make_enabled_mode();
    void* surface_ptr     = nullptr;

    set_display_mode(&mode,
                     direct_draw_ptr.get(),
                     640,
                     480);

    EXPECT_TRUE(create_surface(&mode,
                               direct_draw_ptr.get(),
                               Caps::DDSCAPS_PRIMARYSURFACE,
                               0,
                               0,
                              &surface_ptr) );
    EXPECT_TRUE(create_surface(&mode,
                               direct_draw_ptr.get(),
                               Caps::DDSCAPS_TEXTURE,
                               640,
                               480,
                              &surface_ptr) );
    EXPECT_EQ(surface_ptr,
              nullptr);
    EXPECT_TRUE(direct_draw_ptr->created_description_vec.empty() );
}

TEST(D3D7WindowedMode, WhenTheImagesAreShownLargerTheBackBufferAndWhatIsRenderedWithItAreMadeAsLarge)
{
    auto  direct_draw_ptr = Tests::make_direct_draw();
    auto  mode            = make_enabled_mode(1280,
                                              960);
    void* depth_ptr       = nullptr;
    void* other_ptr       = nullptr;
    void* primary_ptr     = nullptr;
    void* texture_ptr     = nullptr;

    set_display_mode(&mode,
                     direct_draw_ptr.get(),
                     640,
                     480);

    EXPECT_FLOAT_EQ(mode.get_render_scale(),
                    2.0f);

    create_surface(&mode,
                   direct_draw_ptr.get(),
                   PRIMARY_WITH_CHAIN_CAPS,
                   0,
                   0,
                  &primary_ptr);

    ASSERT_EQ(direct_draw_ptr->created_description_vec.size(),
              2u);
    EXPECT_EQ(direct_draw_ptr->created_description_vec.at(1).dwWidth,
              1280u);
    EXPECT_EQ(direct_draw_ptr->created_description_vec.at(1).dwHeight,
              960u);

    /* The depth buffer that the application makes for it is as large as the back buffer is: it must hold the same pixels. */
    EXPECT_FALSE(create_surface(&mode,
                                direct_draw_ptr.get(),
                                Caps::DDSCAPS_ZBUFFER,
                                640,
                                480,
                               &depth_ptr) );

    ASSERT_NE(depth_ptr,
              nullptr);
    EXPECT_EQ(direct_draw_ptr->created_description_vec.back().dwWidth,
              1280u);
    EXPECT_EQ(direct_draw_ptr->created_description_vec.back().dwHeight,
              960u);

    /* A surface of another size (a render target which is a texture of its own) and a texture are not made larger. */
    const size_t n_created = direct_draw_ptr->created_description_vec.size();

    EXPECT_TRUE(create_surface(&mode,
                               direct_draw_ptr.get(),
                               Caps::DDSCAPS_3DDEVICE | Caps::DDSCAPS_TEXTURE,
                               1024,
                               512,
                              &other_ptr) );
    EXPECT_TRUE(create_surface(&mode,
                               direct_draw_ptr.get(),
                               Caps::DDSCAPS_TEXTURE,
                               640,
                               480,
                              &texture_ptr) );
    EXPECT_EQ(direct_draw_ptr->created_description_vec.size(),
              n_created);
}

TEST(D3D7WindowedMode, ABackBufferIsNotMadeLargerThanDirect3D7MakesASurface)
{
    auto  direct_draw_ptr = Tests::make_direct_draw();
    auto  mode            = make_enabled_mode(2064,  /* an eye of a Quest 3: the application stops if a surface is a pixel wider than 2048 */
                                              2208);
    void* primary_ptr     = nullptr;

    set_display_mode(&mode,
                     direct_draw_ptr.get(),
                     640,
                     480);

    EXPECT_FLOAT_EQ(mode.get_render_scale(),
                    3.2f);

    create_surface(&mode,
                   direct_draw_ptr.get(),
                   PRIMARY_WITH_CHAIN_CAPS,
                   0,
                   0,
                  &primary_ptr);

    ASSERT_EQ(direct_draw_ptr->created_description_vec.size(),
              2u);
    EXPECT_EQ(direct_draw_ptr->created_description_vec.at(1).dwWidth,
              2048u);
    EXPECT_EQ(direct_draw_ptr->created_description_vec.at(1).dwHeight,
              1536u);
}

TEST(D3D7WindowedMode, WithNoSizeOfTheImagesToFitNothingIsMadeLarger)
{
    auto  direct_draw_ptr = Tests::make_direct_draw();
    auto  mode            = make_enabled_mode();
    void* depth_ptr       = nullptr;

    set_display_mode(&mode,
                     direct_draw_ptr.get(),
                     640,
                     480);

    EXPECT_FLOAT_EQ(mode.get_render_scale(),
                    1.0f);
    EXPECT_TRUE(create_surface(&mode,
                               direct_draw_ptr.get(),
                               Caps::DDSCAPS_ZBUFFER,
                               640,
                               480,
                              &depth_ptr) );
    EXPECT_EQ(depth_ptr,
              nullptr);
}
