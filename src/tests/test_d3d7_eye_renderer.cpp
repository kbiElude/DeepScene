/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what the converter does to the calls of a Direct3D 7 application to draw the second eye: the device is given the twins of what the
 * application set, the draw is made for each eye with its vertices moved, and what the application set is in the device again. The device and the
 * surfaces are fakes with virtual tables (see fake_direct_draw.h).
 */
#include <gtest/gtest.h>
#include "common/callbacks.h"
#include "interception/core/D3D7Utils/constants.h"
#include "interception/core/D3D7Utils/strided_layout.h"
#include "stereo/core/stereo_core.h"
#include "D3D7/d3d7_eye_renderer.h"
#include "D3D7/d3d7_windowed_mode.h"
#include "fake_direct_draw.h"
#include <algorithm>

namespace
{
    namespace Caps = Interception::Core::D3D7Utils;

    constexpr uint32_t FVF_PRE_TRANSFORMED = 0x1c4; /* positions in pixels, 32 bytes a vertex */
    constexpr size_t   VERTEX_N_BYTES      = 32;

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

    /* A core which has the recipe of an application of pre-transformed vertices, and its eyes 2 apart, converged at 5. */
    class CoreFixture
    {
    public:
        CoreFixture()
        {
            Analysis::Recipe::Recipe recipe;

            recipe.is_baked                              = true;
            recipe.screen_parallax.convergence_distance  = 5.0f;
            recipe.screen_parallax.field_of_view_degrees = 90.0f;
            recipe.screen_parallax.is_enabled            = true;

            core_ptr.reset(new Stereo::Core::StereoCore(recipe,
                                                        [](const std::string&)
                                                        {
                                                            FAIL() << "The core was asked to fail";
                                                        }) );

            core_ptr->set_convergence_distance(5.0f);
            core_ptr->set_eye_separation(2.0f);
            core_ptr->classify_draw(1, /* a signature */
                                    2);/* a cover key  */
        }

        std::unique_ptr<Stereo::Core::StereoCore> core_ptr;
    };

    void post_create_surface(Stereo::ConverterDLL::D3D7EyeRenderer* inout_renderer_ptr,
                             Tests::FakeDirectDraw*                 in_direct_draw_ptr,
                             Tests::FakeSurface*                    in_surface_ptr)
    {
        const APIInterceptor::APIFunctionArgument argument_vec4[4] = {APIInterceptor::APIFunctionArgument::create_void_ptr(in_direct_draw_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_data_chunk_id(0),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(in_surface_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr)};
        const auto                                result            = APIInterceptor::APIFunctionArgument::create_i32(0);

        inout_renderer_ptr->on_post(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE,
                                    4,
                                    argument_vec4,
                                   &result);
    }

    /* The application creates its device for a surface. */
    void post_create_device(Stereo::ConverterDLL::D3D7EyeRenderer* inout_renderer_ptr,
                            Tests::FakeSurface*                    in_surface_ptr,
                            Tests::FakeDevice*                     in_device_ptr)
    {
        const APIInterceptor::APIFunctionArgument argument_vec4[4] = {APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                                                                      APIInterceptor::APIFunctionArgument::create_data_chunk_id(0),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(in_surface_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(in_device_ptr)};
        const auto                                result            = APIInterceptor::APIFunctionArgument::create_i32(0);

        inout_renderer_ptr->on_post(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3D7_CREATEDEVICE,
                                    4,
                                    argument_vec4,
                                   &result);
    }

    /* Three vertices, the first of which is near (the reciprocal of its depth is larger than the one of the distance of convergence, which is 0.2). */
    std::vector<uint8_t> make_vertices()
    {
        const float          vertex_f32vec24[24] = {100.0f, 50.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                    200.0f, 50.0f, 0.5f, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                    300.0f, 50.0f, 0.5f, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f};
        std::vector<uint8_t> result(sizeof(vertex_f32vec24) );

        ::memcpy(result.data(),
                 vertex_f32vec24,
                 sizeof(vertex_f32vec24) );

        return result;
    }

    float get_x(const std::vector<uint8_t>& in_vertices,
                const size_t&               in_n_vertex)
    {
        float result = 0.0f;

        ::memcpy(&result,
                 in_vertices.data() + in_n_vertex * VERTEX_N_BYTES,
                 sizeof(result) );

        return result;
    }

    /* The chunks of the arguments of a draw: where the interceptor has them, as nothing has taken them in the tests. */
    bool get_test_chunk(const uint32_t& in_chunk_id,
                        const void**    out_data_ptr_ptr,
                        uint32_t*       out_n_bytes_ptr)
    {
        return APIInterceptor::get_data_chunk(in_chunk_id,
                                              out_data_ptr_ptr,
                                              out_n_bytes_ptr);
    }

    /* What the renderer is told before a draw which the application makes; returns whether the application's call is to be made. */
    bool pre_draw(Stereo::ConverterDLL::D3D7EyeRenderer* inout_renderer_ptr,
                  Tests::FakeDevice*                     in_device_ptr,
                  std::vector<uint8_t>*                  inout_vertices_ptr,
                  const float&                           in_picture_width)
    {
        const APIInterceptor::APIFunctionArgument argument_vec7[7] = {APIInterceptor::APIFunctionArgument::create_void_ptr(in_device_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(4), /* a list of triangles */
                                                                      APIInterceptor::APIFunctionArgument::create_u32(FVF_PRE_TRANSFORMED),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(inout_vertices_ptr->data() ),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(3),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(0),
                                                                      APIInterceptor::APIFunctionArgument::create_data_chunk_id(0)};

        return inout_renderer_ptr->on_pre_draw(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE,
                                               7,
                                               argument_vec7,
                                               &get_test_chunk,
                                               in_picture_width);
    }

    size_t count_calls(const Tests::FakeDevice& in_device,
                       const std::string&       in_prefix)
    {
        return static_cast<size_t>(std::count_if(in_device.call_vec.begin(),
                                                 in_device.call_vec.end  (),
                                                 [&in_prefix](const std::string& in_call)
                                                 {
                                                     return (in_call.compare(0,
                                                                             in_prefix.size(),
                                                                             in_prefix) == 0);
                                                 }) );
    }
}

TEST(D3D7EyeRenderer, ADrawIsMadeForTheRightEyeToTheTwinsAndForTheLeftEyeWithTheVerticesMovedTheOtherWay)
{
    CoreFixture                            fixture;
    Stereo::ConverterDLL::D3D7WindowedMode windowed_mode;
    Stereo::ConverterDLL::D3D7EyeRenderer renderer(fixture.core_ptr.get(),
                                                   &windowed_mode);
    auto direct_draw_ptr = Tests::make_direct_draw();
    auto device_ptr      = Tests::make_device();

    Tests::FakeSurface* const render_target_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_3DDEVICE,
                                                                                       640,
                                                                                       480) );

    post_create_surface(&renderer,
                        direct_draw_ptr.get(),
                        render_target_ptr);
    post_create_device (&renderer,
                        render_target_ptr,
                        device_ptr.get() );

    auto vertices = make_vertices();

    /* The renderer made the draw for the left eye itself: the application's call is not to be made. */
    EXPECT_FALSE(pre_draw(&renderer,
                          device_ptr.get(),
                         &vertices,
                          640.0f) );

    ASSERT_EQ(device_ptr->vertex_vec_vec.size(),
              2u);

    /* The right eye first, to the twin, then the left eye, to what the application set. */
    const auto twin_text     = "SetRenderTarget " + std::to_string(reinterpret_cast<uintptr_t>(renderer.get_twin_ptr(render_target_ptr) ));
    const auto original_text = "SetRenderTarget " + std::to_string(reinterpret_cast<uintptr_t>(render_target_ptr) );

    ASSERT_GE(device_ptr->call_vec.size(),
              5u);
    EXPECT_NE(std::find(device_ptr->call_vec.begin(),
                        device_ptr->call_vec.end  (),
                        twin_text),
              device_ptr->call_vec.end() );
    EXPECT_EQ(device_ptr->call_vec.back(),
              "DrawPrimitive");
    EXPECT_EQ(count_calls(*device_ptr,
                          original_text),
              1u);

    /* A near vertex is moved to the right in the left eye and to the left in the right one; one at the distance of convergence is not moved. */
    const auto& right_vertices = device_ptr->vertex_vec_vec.at(0);
    const auto& left_vertices  = device_ptr->vertex_vec_vec.at(1);

    EXPECT_LT(get_x(right_vertices, 0),
              100.0f);
    EXPECT_GT(get_x(left_vertices, 0),
              100.0f);
    EXPECT_FLOAT_EQ(get_x(right_vertices, 0) - 100.0f,
                    100.0f - get_x(left_vertices, 0) );
    EXPECT_NEAR(get_x(left_vertices, 1),
                200.0f,
                0.01f);
    EXPECT_NEAR(get_x(right_vertices, 1),
                200.0f,
                0.01f);

    /* What the application holds is not touched. */
    EXPECT_FLOAT_EQ(get_x(vertices, 0),
                    100.0f);
}

TEST(D3D7EyeRenderer, TheTexturesTheApplicationSetAreTheirTwinsForTheRightEyeAndAreBackAfterIt)
{
    CoreFixture                            fixture;
    Stereo::ConverterDLL::D3D7WindowedMode windowed_mode;
    Stereo::ConverterDLL::D3D7EyeRenderer renderer(fixture.core_ptr.get(),
                                                   &windowed_mode);
    auto direct_draw_ptr = Tests::make_direct_draw();
    auto device_ptr      = Tests::make_device();

    Tests::FakeSurface* const render_target_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_3DDEVICE,
                                                                                       640,
                                                                                       480) );
    Tests::FakeSurface* const texture_ptr       = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_3DDEVICE | Caps::DDSCAPS_TEXTURE,
                                                                                       256,
                                                                                       256) );

    post_create_surface(&renderer,
                        direct_draw_ptr.get(),
                        render_target_ptr);
    post_create_surface(&renderer,
                        direct_draw_ptr.get(),
                        texture_ptr);
    post_create_device (&renderer,
                        render_target_ptr,
                        device_ptr.get() );

    /* The application sets the texture in the first stage. */
    const APIInterceptor::APIFunctionArgument set_texture_vec3[3] = {APIInterceptor::APIFunctionArgument::create_void_ptr(device_ptr.get() ),
                                                                     APIInterceptor::APIFunctionArgument::create_u32(0),
                                                                     APIInterceptor::APIFunctionArgument::create_void_ptr(texture_ptr)};

    EXPECT_TRUE(renderer.on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETTEXTURE,
                                3,
                                set_texture_vec3) );

    auto vertices = make_vertices();

    pre_draw(&renderer,
             device_ptr.get(),
            &vertices,
             640.0f);

    const auto twin_text     = "SetTexture 0 " + std::to_string(reinterpret_cast<uintptr_t>(renderer.get_twin_ptr(texture_ptr) ));
    const auto original_text = "SetTexture 0 " + std::to_string(reinterpret_cast<uintptr_t>(texture_ptr) );

    const auto twin_iterator     = std::find(device_ptr->call_vec.begin(), device_ptr->call_vec.end(), twin_text);
    const auto original_iterator = std::find(device_ptr->call_vec.begin(), device_ptr->call_vec.end(), original_text);

    ASSERT_NE(twin_iterator,
              device_ptr->call_vec.end() );
    ASSERT_NE(original_iterator,
              device_ptr->call_vec.end() );

    /* The twin is set before the draw of the right eye, and the application's own texture after it. */
    EXPECT_LT(twin_iterator,
              original_iterator);
}

TEST(D3D7EyeRenderer, ADrawToASurfaceWhichHasNoTwinIsCountedAndMadeOnceForTheLeftEyeAlone)
{
    CoreFixture                            fixture;
    Stereo::ConverterDLL::D3D7WindowedMode windowed_mode;
    Stereo::ConverterDLL::D3D7EyeRenderer renderer(fixture.core_ptr.get(),
                                                   &windowed_mode);
    auto direct_draw_ptr = Tests::make_direct_draw();
    auto device_ptr      = Tests::make_device();

    /* A surface that the application did not make to render to (it is not seen to be one). */
    Tests::FakeSurface* const plain_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                              make_description(Caps::DDSCAPS_TEXTURE,
                                                                               640,
                                                                               480) );

    post_create_surface(&renderer,
                        direct_draw_ptr.get(),
                        plain_ptr);
    post_create_device (&renderer,
                        plain_ptr,
                        device_ptr.get() );

    auto vertices = make_vertices();

    pre_draw(&renderer,
             device_ptr.get(),
            &vertices,
             640.0f);

    EXPECT_EQ(renderer.get_n_unmirrored_calls(),
              1u);
    EXPECT_EQ(device_ptr->vertex_vec_vec.size(),
              1u);
    EXPECT_EQ(count_calls(*device_ptr,
                          "SetRenderTarget"),
              0u);
}

TEST(D3D7EyeRenderer, AClearIsMadeForTheRightEyeToTheTwinAndTheApplicationsOwnIsLeftToIt)
{
    CoreFixture                            fixture;
    Stereo::ConverterDLL::D3D7WindowedMode windowed_mode;
    Stereo::ConverterDLL::D3D7EyeRenderer renderer(fixture.core_ptr.get(),
                                                   &windowed_mode);
    auto direct_draw_ptr = Tests::make_direct_draw();
    auto device_ptr      = Tests::make_device();

    Tests::FakeSurface* const render_target_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                      make_description(Caps::DDSCAPS_3DDEVICE,
                                                                                       640,
                                                                                       480) );

    post_create_surface(&renderer,
                        direct_draw_ptr.get(),
                        render_target_ptr);
    post_create_device (&renderer,
                        render_target_ptr,
                        device_ptr.get() );

    const APIInterceptor::APIFunctionArgument clear_argument_vec7[7] = {APIInterceptor::APIFunctionArgument::create_void_ptr(device_ptr.get() ),
                                                                        APIInterceptor::APIFunctionArgument::create_u32(0),
                                                                        APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr),
                                                                        APIInterceptor::APIFunctionArgument::create_u32(3), /* target and depth */
                                                                        APIInterceptor::APIFunctionArgument::create_u32(0),
                                                                        APIInterceptor::APIFunctionArgument::create_fp32(1.0f),
                                                                        APIInterceptor::APIFunctionArgument::create_u32(0)};

    EXPECT_TRUE(renderer.on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR,
                                7,
                                clear_argument_vec7) );

    EXPECT_EQ(count_calls(*device_ptr,
                          "Clear"),
              1u);
    EXPECT_EQ(count_calls(*device_ptr,
                          "SetRenderTarget"),
              2u);
}

TEST(D3D7EyeRenderer, ACopyToASurfaceWhichHasATwinIsMadeToTheTwinFromTheTwinOfItsSource)
{
    CoreFixture                            fixture;
    Stereo::ConverterDLL::D3D7WindowedMode windowed_mode;
    Stereo::ConverterDLL::D3D7EyeRenderer renderer(fixture.core_ptr.get(),
                                                   &windowed_mode);
    auto                                  direct_draw_ptr = Tests::make_direct_draw();

    Tests::FakeSurface* const destination_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                    make_description(Caps::DDSCAPS_3DDEVICE,
                                                                                     64,
                                                                                     64) );
    Tests::FakeSurface* const source_ptr      = Tests::make_surface(direct_draw_ptr.get(),
                                                                    make_description(Caps::DDSCAPS_3DDEVICE,
                                                                                     64,
                                                                                     64) );

    post_create_surface(&renderer,
                        direct_draw_ptr.get(),
                        destination_ptr);
    post_create_surface(&renderer,
                        direct_draw_ptr.get(),
                        source_ptr);

    const APIInterceptor::APIFunctionArgument blt_argument_vec6[6] = {APIInterceptor::APIFunctionArgument::create_void_ptr(destination_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_data_chunk_id(0),
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(source_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_data_chunk_id(0),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(0x01000000),
                                                                      APIInterceptor::APIFunctionArgument::create_data_chunk_id(0)};

    EXPECT_TRUE(renderer.on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_BLT,
                                6,
                                blt_argument_vec6) );

    const auto* destination_twin_ptr = static_cast<Tests::FakeSurface*>(renderer.get_twin_ptr(destination_ptr) );

    ASSERT_EQ(destination_twin_ptr->call_vec.size(),
              1u);
    EXPECT_NE(destination_twin_ptr->call_vec.at(0).find("source " + std::to_string(reinterpret_cast<uintptr_t>(renderer.get_twin_ptr(source_ptr) ))),
              std::string::npos);
    EXPECT_EQ(renderer.get_n_unmirrored_calls(),
              0u);
}

TEST(D3D7EyeRenderer, WhenTheBackBufferIsLargerTheViewportTheClearAndTheVerticesAreInItsPixels)
{
    CoreFixture                            fixture;
    Stereo::ConverterDLL::D3D7WindowedMode windowed_mode;
    auto                                   direct_draw_ptr = Tests::make_direct_draw();
    auto                                   device_ptr      = Tests::make_device();

    /* The application asks for a display mode of 640 x 480; the images are shown at twice that. */
    windowed_mode.set_is_enabled(true);
    windowed_mode.set_target_eye_size(1280,
                                      960);

    const APIInterceptor::APIFunctionArgument display_mode_vec6[6] = {APIInterceptor::APIFunctionArgument::create_void_ptr(direct_draw_ptr.get() ),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(640),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(480),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(32),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(0),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(0)};

    windowed_mode.on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETDISPLAYMODE,
                         6,
                         display_mode_vec6);

    Stereo::ConverterDLL::D3D7EyeRenderer renderer(fixture.core_ptr.get(),
                                                   &windowed_mode);

    /* The back buffer, as the primary surface gives it. */
    Tests::FakeSurface* const primary_ptr     = Tests::make_surface(direct_draw_ptr.get(),
                                                                    make_description(Caps::DDSCAPS_PRIMARYSURFACE,
                                                                                     0,
                                                                                     0) );
    Tests::FakeSurface* const back_buffer_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                                    make_description(Caps::DDSCAPS_3DDEVICE | Caps::DDSCAPS_OFFSCREENPLAIN,
                                                                                     1280,
                                                                                     960) );

    post_create_surface(&renderer,
                        direct_draw_ptr.get(),
                        primary_ptr);

    const APIInterceptor::APIFunctionArgument attached_argument_vec3[3] = {APIInterceptor::APIFunctionArgument::create_void_ptr(primary_ptr),
                                                                           APIInterceptor::APIFunctionArgument::create_data_chunk_id(0),
                                                                           APIInterceptor::APIFunctionArgument::create_void_ptr(back_buffer_ptr)};
    const auto                                success                   = APIInterceptor::APIFunctionArgument::create_i32(0);

    renderer.on_post(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE,
                     3,
                     attached_argument_vec3,
                    &success);
    post_create_device(&renderer,
                       back_buffer_ptr,
                       device_ptr.get() );

    /* The viewport which the application sets is of its own pixels; the device is given one as large as the back buffer. */
    Interception::Hooks::D3D7::D3DVIEWPORT7 viewport = {};

    viewport.dwHeight = 480;
    viewport.dwWidth  = 640;
    viewport.dvMaxZ   = 1.0f;

    const auto viewport_chunk_id = APIInterceptor::register_data_chunk(&viewport,
                                                                       static_cast<uint32_t>(sizeof(viewport) ));
    const APIInterceptor::APIFunctionArgument viewport_argument_vec2[2] = {APIInterceptor::APIFunctionArgument::create_void_ptr(device_ptr.get() ),
                                                                           APIInterceptor::APIFunctionArgument::create_data_chunk_id(viewport_chunk_id)};

    EXPECT_FALSE(renderer.on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETVIEWPORT,
                                 2,
                                 viewport_argument_vec2) );

    APIInterceptor::release_data_chunk(viewport_chunk_id);

    EXPECT_EQ(device_ptr->viewport.dwWidth,
              1280u);
    EXPECT_EQ(device_ptr->viewport.dwHeight,
              960u);

    /* A clear with a rectangle of the application's pixels is made with the rectangle of the back buffer's, for both eyes. */
    Interception::Hooks::D3D7::D3DRECT rect = {10, 20, 30, 40};

    const APIInterceptor::APIFunctionArgument clear_argument_vec7[7] = {APIInterceptor::APIFunctionArgument::create_void_ptr(device_ptr.get() ),
                                                                        APIInterceptor::APIFunctionArgument::create_u32(1),
                                                                        APIInterceptor::APIFunctionArgument::create_void_ptr(&rect),
                                                                        APIInterceptor::APIFunctionArgument::create_u32(1),
                                                                        APIInterceptor::APIFunctionArgument::create_u32(0),
                                                                        APIInterceptor::APIFunctionArgument::create_fp32(1.0f),
                                                                        APIInterceptor::APIFunctionArgument::create_u32(0)};

    EXPECT_FALSE(renderer.on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_CLEAR,
                                 7,
                                 clear_argument_vec7) );

    ASSERT_EQ(device_ptr->last_clear_rect_vec.size(),
              1u);
    EXPECT_EQ(device_ptr->last_clear_rect_vec.at(0).x1,
              20);
    EXPECT_EQ(device_ptr->last_clear_rect_vec.at(0).y2,
              80);
    EXPECT_EQ(count_calls(*device_ptr,
                          "Clear"),
              2u);

    /* The vertices are in the pixels of the application, which is twice as many in each direction here: the one at the distance of convergence is
     * at twice its position.
     */
    auto vertices = make_vertices();

    pre_draw(&renderer,
             device_ptr.get(),
            &vertices,
             640.0f);

    ASSERT_EQ(device_ptr->vertex_vec_vec.size(),
              2u);
    EXPECT_NEAR(get_x(device_ptr->vertex_vec_vec.at(1), 1),
                400.0f,
                0.01f);
}

namespace
{
    /* What the renderer is told before a draw from a vertex buffer. */
    bool pre_draw_from_vertex_buffer(Stereo::ConverterDLL::D3D7EyeRenderer* inout_renderer_ptr,
                                     Tests::FakeDevice*                     in_device_ptr,
                                     Tests::FakeVertexBuffer*               in_vertex_buffer_ptr,
                                     const float&                           in_picture_width)
    {
        const APIInterceptor::APIFunctionArgument argument_vec6[6] = {APIInterceptor::APIFunctionArgument::create_void_ptr(in_device_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(4), /* a list of triangles */
                                                                      APIInterceptor::APIFunctionArgument::create_void_ptr(in_vertex_buffer_ptr),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(0), /* the first vertex */
                                                                      APIInterceptor::APIFunctionArgument::create_u32(3),
                                                                      APIInterceptor::APIFunctionArgument::create_u32(0)};

        return inout_renderer_ptr->on_pre_draw(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVEVB,
                                               6,
                                               argument_vec6,
                                               &get_test_chunk,
                                               in_picture_width);
    }

    /* A device with a render target which has a twin, for the tests of what is drawn. */
    struct DrawFixture
    {
        DrawFixture()
            :renderer(core_fixture.core_ptr.get(),
                      &windowed_mode),
             direct_draw_ptr(Tests::make_direct_draw() ),
             device_ptr     (Tests::make_device() )
        {
            render_target_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                    make_description(Caps::DDSCAPS_3DDEVICE,
                                                                     640,
                                                                     480) );

            post_create_surface(&renderer,
                                direct_draw_ptr.get(),
                                render_target_ptr);
            post_create_device (&renderer,
                                render_target_ptr,
                                device_ptr.get() );
        }

        CoreFixture                            core_fixture;
        Stereo::ConverterDLL::D3D7WindowedMode windowed_mode;
        Stereo::ConverterDLL::D3D7EyeRenderer  renderer;
        std::unique_ptr<Tests::FakeDirectDraw> direct_draw_ptr;
        std::unique_ptr<Tests::FakeDevice>     device_ptr;
        Tests::FakeSurface*                    render_target_ptr = nullptr;
    };
}

TEST(D3D7EyeRenderer, ADrawFromAVertexBufferIsMadeFromItsVerticesMovedForEachEye)
{
    DrawFixture fixture;
    auto        vertex_buffer_ptr = Tests::make_vertex_buffer(FVF_PRE_TRANSFORMED,
                                                              make_vertices(),
                                                              3);

    /* The renderer made the draw for the left eye itself, from the vertices it read out of the buffer. */
    EXPECT_FALSE(pre_draw_from_vertex_buffer(&fixture.renderer,
                                             fixture.device_ptr.get(),
                                             vertex_buffer_ptr.get(),
                                             640.0f) );

    ASSERT_EQ(fixture.device_ptr->vertex_vec_vec.size(),
              2u);

    const auto& right_vertices = fixture.device_ptr->vertex_vec_vec.at(0);
    const auto& left_vertices  = fixture.device_ptr->vertex_vec_vec.at(1);

    EXPECT_LT(get_x(right_vertices, 0),
              100.0f);
    EXPECT_GT(get_x(left_vertices, 0),
              100.0f);
    EXPECT_FALSE(vertex_buffer_ptr->is_locked); /* it is let go of */
}

TEST(D3D7EyeRenderer, ADrawFromAVertexBufferWhichCannotBeReadIsCountedAndMadeOnceForTheLeftEyeAlone)
{
    DrawFixture fixture;
    auto        vertex_buffer_ptr = Tests::make_vertex_buffer(FVF_PRE_TRANSFORMED,
                                                              make_vertices(),
                                                              3);

    vertex_buffer_ptr->fails_to_lock = true;

    /* The call of the application is made. */
    EXPECT_TRUE(pre_draw_from_vertex_buffer(&fixture.renderer,
                                            fixture.device_ptr.get(),
                                            vertex_buffer_ptr.get(),
                                            640.0f) );

    EXPECT_TRUE(fixture.device_ptr->vertex_vec_vec.empty() );
    EXPECT_EQ(fixture.renderer.get_n_unmirrored_calls(),
              1u);
}

TEST(D3D7EyeRenderer, AStridedDrawIsMadeForTheRightEyeAsItIsAndTheCallOfTheApplicationIsMadeForTheLeft)
{
    DrawFixture fixture;

    /* The arrays of the application (the position of 3 vertices, 12 bytes each), as the dump and the hook have them: the structure, and the chunk. */
    const float                                            position_f32vec9[9] = {0.1f, 0.9f, 0.5f, 0.9f, 0.9f, 0.5f, 0.5f, 0.45f, 0.5f};
    Interception::Hooks::D3D7::D3DDRAWPRIMITIVESTRIDEDDATA strided          = {};
    std::vector<uint8_t>                                   arrays_u8_vec;
    const uint32_t                                         n_arrays         = Interception::Core::D3D7Utils::StridedLayout::N_ARRAYS;
    const uint32_t                                         position_n_bytes = sizeof(position_f32vec9);

    strided.position.lpvData  = const_cast<float*>(position_f32vec9);
    strided.position.dwStride = 12;

    arrays_u8_vec.insert(arrays_u8_vec.end(),
                         reinterpret_cast<const uint8_t*>(&n_arrays),
                         reinterpret_cast<const uint8_t*>(&n_arrays) + sizeof(n_arrays) );
    arrays_u8_vec.insert(arrays_u8_vec.end(),
                         reinterpret_cast<const uint8_t*>(&position_n_bytes),
                         reinterpret_cast<const uint8_t*>(&position_n_bytes) + sizeof(position_n_bytes) );
    arrays_u8_vec.resize(arrays_u8_vec.size() + sizeof(uint32_t) * (Interception::Core::D3D7Utils::StridedLayout::N_ARRAYS - 1),
                         0);
    arrays_u8_vec.insert(arrays_u8_vec.end(),
                         reinterpret_cast<const uint8_t*>(position_f32vec9),
                         reinterpret_cast<const uint8_t*>(position_f32vec9) + sizeof(position_f32vec9) );

    const auto structure_chunk_id = APIInterceptor::register_data_chunk(&strided,
                                                                        sizeof(strided) );
    const auto arrays_chunk_id    = APIInterceptor::register_data_chunk(arrays_u8_vec.data(),
                                                                        static_cast<uint32_t>(arrays_u8_vec.size() ));

    const APIInterceptor::APIFunctionArgument argument_vec7[7] = {APIInterceptor::APIFunctionArgument::create_void_ptr(fixture.device_ptr.get() ),
                                                                  APIInterceptor::APIFunctionArgument::create_u32(4), /* a list of triangles */
                                                                  APIInterceptor::APIFunctionArgument::create_u32(0x002), /* positions which are not transformed */
                                                                  APIInterceptor::APIFunctionArgument::create_data_chunk_id(structure_chunk_id),
                                                                  APIInterceptor::APIFunctionArgument::create_u32(3),
                                                                  APIInterceptor::APIFunctionArgument::create_u32(0),
                                                                  APIInterceptor::APIFunctionArgument::create_data_chunk_id(arrays_chunk_id)};

    const bool is_call_made = fixture.renderer.on_pre_draw(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVESTRIDED,
                                                           7,
                                                           argument_vec7,
                                                           &get_test_chunk,
                                                           640.0f);

    APIInterceptor::release_data_chunk(structure_chunk_id);
    APIInterceptor::release_data_chunk(arrays_chunk_id);

    /* The left eye is the call of the application; the right eye is made by the renderer, with the arrays and the distance that the application gave. */
    EXPECT_TRUE(is_call_made);
    EXPECT_EQ(count_calls(*fixture.device_ptr,
                          "DrawPrimitiveStrided "),
              1u);
    EXPECT_NE(std::find(fixture.device_ptr->call_vec.begin(),
                        fixture.device_ptr->call_vec.end  (),
                        "DrawPrimitiveStrided " + std::to_string(0.1f) + " 12"),
              fixture.device_ptr->call_vec.end() );
}

namespace
{
    /* An application which asked for a display mode of 640 x 480, with the images shown at twice that: its back buffer is 1280 x 960, and has a twin. */
    class LargerBackBufferFixture
    {
    public:
        LargerBackBufferFixture()
            :direct_draw_ptr(Tests::make_direct_draw() )
        {
            windowed_mode.set_is_enabled(true);
            windowed_mode.set_target_eye_size(1280,
                                              960);

            const APIInterceptor::APIFunctionArgument display_mode_vec6[6] = {APIInterceptor::APIFunctionArgument::create_void_ptr(direct_draw_ptr.get() ),
                                                                              APIInterceptor::APIFunctionArgument::create_u32(640),
                                                                              APIInterceptor::APIFunctionArgument::create_u32(480),
                                                                              APIInterceptor::APIFunctionArgument::create_u32(32),
                                                                              APIInterceptor::APIFunctionArgument::create_u32(0),
                                                                              APIInterceptor::APIFunctionArgument::create_u32(0)};

            windowed_mode.on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETDISPLAYMODE,
                                 6,
                                 display_mode_vec6);

            renderer_ptr.reset(new Stereo::ConverterDLL::D3D7EyeRenderer(core_fixture.core_ptr.get(),
                                                                         &windowed_mode) );

            primary_ptr     = Tests::make_surface(direct_draw_ptr.get(),
                                                  make_description(Caps::DDSCAPS_PRIMARYSURFACE,
                                                                   0,
                                                                   0) );
            back_buffer_ptr = Tests::make_surface(direct_draw_ptr.get(),
                                                  make_description(Caps::DDSCAPS_3DDEVICE | Caps::DDSCAPS_OFFSCREENPLAIN,
                                                                   1280,
                                                                   960) );

            post_create_surface(renderer_ptr.get(),
                                direct_draw_ptr.get(),
                                primary_ptr);

            const APIInterceptor::APIFunctionArgument attached_argument_vec3[3] = {APIInterceptor::APIFunctionArgument::create_void_ptr(primary_ptr),
                                                                                   APIInterceptor::APIFunctionArgument::create_data_chunk_id(0),
                                                                                   APIInterceptor::APIFunctionArgument::create_void_ptr(back_buffer_ptr)};
            const auto                                success                   = APIInterceptor::APIFunctionArgument::create_i32(0);

            renderer_ptr->on_post(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE,
                                  3,
                                  attached_argument_vec3,
                                 &success);
        }

        /* The application locks the back buffer (with these flags and a rectangle, or the whole of it), and unlocks it. */
        void lock_and_unlock(const uint32_t& in_flags,
                             const bool&     in_is_whole_surface)
        {
            const RECT rect          = {0, 0, 10, 10};
            const auto rect_chunk_id = (in_is_whole_surface) ? 0u
                                                             : APIInterceptor::register_data_chunk(&rect,
                                                                                                   static_cast<uint32_t>(sizeof(rect) ));
            const APIInterceptor::APIFunctionArgument lock_argument_vec5[5] = {APIInterceptor::APIFunctionArgument::create_void_ptr(back_buffer_ptr),
                                                                               APIInterceptor::APIFunctionArgument::create_data_chunk_id(rect_chunk_id),
                                                                               APIInterceptor::APIFunctionArgument::create_data_chunk_id(0),
                                                                               APIInterceptor::APIFunctionArgument::create_u32(in_flags),
                                                                               APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr)};
            const APIInterceptor::APIFunctionArgument unlock_argument_vec2[2] = {APIInterceptor::APIFunctionArgument::create_void_ptr(back_buffer_ptr),
                                                                                 APIInterceptor::APIFunctionArgument::create_data_chunk_id(0)};
            const auto                                success                 = APIInterceptor::APIFunctionArgument::create_i32(0);

            EXPECT_TRUE(renderer_ptr->on_pre(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_LOCK,
                                             5,
                                             lock_argument_vec5) );
            renderer_ptr->on_post(APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_UNLOCK,
                                  2,
                                  unlock_argument_vec2,
                                 &success);

            if (in_is_whole_surface == false)
            {
                APIInterceptor::release_data_chunk(rect_chunk_id);
            }
        }

        Tests::FakeSurface*                                    back_buffer_ptr = nullptr;
        CoreFixture                                            core_fixture;
        std::unique_ptr<Tests::FakeDirectDraw>                 direct_draw_ptr;
        Tests::FakeSurface*                                    primary_ptr     = nullptr;
        std::unique_ptr<Stereo::ConverterDLL::D3D7EyeRenderer> renderer_ptr;
        Stereo::ConverterDLL::D3D7WindowedMode                 windowed_mode;
    };
}

TEST(D3D7EyeRenderer, APictureTheApplicationWritesToTheWholeBackBufferIsStretchedAndMadeForTheRightEyeToo)
{
    LargerBackBufferFixture fixture;

    fixture.lock_and_unlock(0, /* in_flags */
                            true); /* in_is_whole_surface */

    /* The 640 x 480 corner goes to the twin as large as the whole, and what is in the twin goes back: both eyes show it. */
    const auto* const twin_ptr = static_cast<Tests::FakeSurface*>(fixture.renderer_ptr->get_twin_ptr(fixture.back_buffer_ptr) );

    ASSERT_NE(twin_ptr,
              nullptr);
    ASSERT_EQ(twin_ptr->call_vec.size(),
              1u);
    EXPECT_NE(twin_ptr->call_vec.at(0).find("source " + std::to_string(reinterpret_cast<uintptr_t>(fixture.back_buffer_ptr) ) ),
              std::string::npos);
    EXPECT_NE(twin_ptr->call_vec.at(0).find("from 640 x 480"),
              std::string::npos);
    ASSERT_EQ(fixture.back_buffer_ptr->call_vec.size(),
              1u);
    EXPECT_NE(fixture.back_buffer_ptr->call_vec.at(0).find("source " + std::to_string(reinterpret_cast<uintptr_t>(twin_ptr) ) ),
              std::string::npos);
    EXPECT_EQ(fixture.renderer_ptr->get_n_pixel_calls(),
              0u);
}

TEST(D3D7EyeRenderer, ALockToReadOrOfPartOfTheBackBufferChangesNothingAndIsCounted)
{
    LargerBackBufferFixture fixture;

    fixture.lock_and_unlock(0x10, /* DDLOCK_READONLY */
                            true); /* in_is_whole_surface */
    fixture.lock_and_unlock(0, /* in_flags */
                            false); /* in_is_whole_surface */

    const auto* const twin_ptr = static_cast<Tests::FakeSurface*>(fixture.renderer_ptr->get_twin_ptr(fixture.back_buffer_ptr) );

    ASSERT_NE(twin_ptr,
              nullptr);
    EXPECT_TRUE(twin_ptr->call_vec.empty() );
    EXPECT_TRUE(fixture.back_buffer_ptr->call_vec.empty() );
    EXPECT_EQ(fixture.renderer_ptr->get_n_pixel_calls(),
              2u);
}
