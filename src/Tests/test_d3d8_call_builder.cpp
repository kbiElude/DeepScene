/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks that the calls built for the tests look like the calls which the dumper stores.
 */
#include <gtest/gtest.h>
#include "Capture/APIDumpLoader/api_info.h"
#include "Interception/Core/D3D8Utils/argument_info.h"
#include "Interception/Core/D3D8Utils/methods.h"
#include "d3d8_call_builder.h"

TEST(D3D8CallBuilder, CallsHaveTheArgumentCountsOfTheirMethods)
{
    Tests::D3D8CallBuilder                  builder;
    Interception::Hooks::D3D8::D3DMATRIX    matrix   = {};
    Interception::Hooks::D3D8::D3DVIEWPORT8 viewport = {0, 0, 800, 600, 0.0f, 1.0f};

    builder.create_device();

    const auto depth_stencil = builder.create_depth_stencil_surface(512, 512, Interception::Hooks::D3D8::D3DFMT_D24S8);
    const auto index_buffer  = builder.create_index_buffer         (64);
    const auto texture       = builder.create_texture              (512, 512, 1, 1, Interception::Hooks::D3D8::D3DFMT_X8R8G8B8);
    const auto vertex_buffer = builder.create_vertex_buffer        (128, 0x152);
    const auto surface       = builder.get_surface_level           (texture, 0);

    builder.get_depth_stencil_surface();
    builder.get_render_target       ();
    builder.set_render_target       (surface, depth_stencil);
    builder.set_viewport            (viewport);
    builder.clear                   (3, 0xFF000000, 1.0f, 0);
    builder.begin_scene             ();
    builder.set_transform           (Interception::Hooks::D3D8::D3DTS_VIEW, matrix);
    builder.set_render_state        (Interception::Hooks::D3D8::D3DRS_ZENABLE, 1);
    builder.set_vertex_shader       (0x152);
    builder.unlock_vertex_buffer    (vertex_buffer, {1, 2, 3});
    builder.unlock_index_buffer     (index_buffer,  {4, 5});
    builder.set_stream_source       (0, vertex_buffer, 36);
    builder.set_indices             (index_buffer, 0);
    builder.set_texture             (0, texture);
    builder.draw_indexed_primitive  (Interception::Hooks::D3D8::D3DPT_TRIANGLELIST, 0, 4, 0, 2);
    builder.end_scene               ();
    builder.present                 ();
    builder.release_surface         (surface);
    builder.release_texture         (texture);

    for (const auto& call : *builder.get_workload().dumped_api_call_vec_ptr)
    {
        /* Data that the dumper stores after the arguments of the method. */
        const size_t n_extra_arguments = (call.func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CLEAR)        ? 1
                                       : (call.func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT)      ? 3
                                       : (call.func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DINDEXBUFFER8_UNLOCK)  ? 1
                                       : (call.func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DVERTEXBUFFER8_UNLOCK) ? 1
                                                                                                                       : 0;

        if (call.func == APIInterceptor::APIFUNCTION_D3D8_DIRECT3DCREATE8)
        {
            continue;
        }

        EXPECT_EQ(call.arg_in_vec.size(),
                  Interception::Core::D3D8Utils::get_n_arguments(call.func) + n_extra_arguments) << Interception::Core::D3D8Utils::get_function_name_ptr(call.func);
        EXPECT_EQ(call.arg_out_vec.size(),
                  call.arg_in_vec.size() )                                     << Interception::Core::D3D8Utils::get_function_name_ptr(call.func);
    }
}

TEST(D3D8CallBuilder, CreatedObjectsAreInTheOutArguments)
{
    Tests::D3D8CallBuilder builder;

    builder.create_device();

    const auto texture = builder.create_texture(64, 64, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);
    const auto& call   = builder.get_workload().dumped_api_call_vec_ptr->back();

    EXPECT_EQ(call.func,
              APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATETEXTURE);
    EXPECT_EQ(call.arg_out_vec.back().get_ptr_as_u64(),
              texture);
    EXPECT_NE(call.arg_in_vec.back().get_ptr_as_u64(),
              texture);
}

TEST(D3D8CallBuilder, ObjectsGetDistinctAddressesUnlessOneIsAsked)
{
    Tests::D3D8CallBuilder builder;

    builder.create_device();

    const auto first  = builder.create_texture(8, 8, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);
    const auto second = builder.create_texture(8, 8, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8);
    const auto reused = builder.create_texture(8, 8, 1, 0, Interception::Hooks::D3D8::D3DFMT_A8R8G8B8, first);

    EXPECT_NE(first,
              second);
    EXPECT_EQ(reused,
              first);
}

TEST(D3D8CallBuilder, ChunksAreFoundInTheWorkload)
{
    Tests::D3D8CallBuilder               builder;
    Interception::Hooks::D3D8::D3DMATRIX matrix = {};

    builder.create_device();
    builder.set_transform(Interception::Hooks::D3D8::D3DTS_PROJECTION, matrix);

    const auto& call  = builder.get_workload().dumped_api_call_vec_ptr->back();
    const auto  chunk = Capture::APIDumpLoader::get_data_chunk_ptr(builder.get_workload(),
                                                                   call.arg_in_vec.at(2).get_data_chunk_id() );

    ASSERT_NE(chunk,
              nullptr);
    EXPECT_EQ(chunk->size(),
              sizeof(Interception::Hooks::D3D8::D3DMATRIX) );
}

TEST(D3D8CallBuilder, PresentEndsAFrame)
{
    Tests::D3D8CallBuilder builder;

    builder.create_device();
    builder.present();

    EXPECT_TRUE(Capture::APIDumpLoader::is_frame_boundary(builder.get_workload().dumped_api_call_vec_ptr->back() ) );
    EXPECT_FALSE(Capture::APIDumpLoader::is_frame_boundary(builder.get_workload().dumped_api_call_vec_ptr->front() ) );
}
