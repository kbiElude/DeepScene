/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d8_scene.h"

Tests::D3D8Scene::D3D8Scene()
    :m_back_buffer_surface(0),
     m_depth_surface      (0),
     m_index_buffer       (0),
     m_vertex_buffer      (0)
{
    constexpr uint32_t FVF_XYZ_NORMAL_DIFFUSE_TEX1 = 0x152;
    constexpr uint32_t INDEX_BUFFER_N_BYTES        = 12;
    constexpr uint32_t STRIDE                      = 36;
    constexpr uint32_t VERTEX_BUFFER_N_BYTES       = 144;

    m_builder.create_device();

    m_index_buffer  = m_builder.create_index_buffer (INDEX_BUFFER_N_BYTES);
    m_vertex_buffer = m_builder.create_vertex_buffer(VERTEX_BUFFER_N_BYTES,
                                                    FVF_XYZ_NORMAL_DIFFUSE_TEX1);

    m_back_buffer_surface = m_builder.get_render_target();
    m_depth_surface       = m_builder.get_depth_stencil_surface();

    m_builder.set_vertex_shader(FVF_XYZ_NORMAL_DIFFUSE_TEX1);
    m_builder.set_stream_source(0,
                                m_vertex_buffer,
                                STRIDE);
    m_builder.set_indices      (m_index_buffer,
                                0);

    feed();
}

void Tests::D3D8Scene::bind_back_buffer()
{
    m_builder.set_render_target(m_back_buffer_surface,
                                m_depth_surface);

    feed();
}

void Tests::D3D8Scene::draw(const uint32_t& in_n_draws)
{
    constexpr uint32_t N_PRIMITIVES = 2;
    constexpr uint32_t N_VERTICES   = 4;

    for (uint32_t n_draw = 0;
                  n_draw < in_n_draws;
                ++n_draw)
    {
        m_builder.draw_indexed_primitive(Interception::Hooks::D3D8::D3DPT_TRIANGLELIST,
                                         0,
                                         N_VERTICES,
                                         0,
                                         N_PRIMITIVES);
    }

    feed();
}

void Tests::D3D8Scene::feed()
{
    m_harness.feed(m_builder);
}

uint64_t Tests::D3D8Scene::get_back_buffer_surface() const
{
    return m_back_buffer_surface;
}

Tests::D3D8CallBuilder& Tests::D3D8Scene::get_builder()
{
    return m_builder;
}

uint64_t Tests::D3D8Scene::get_depth_surface() const
{
    return m_depth_surface;
}

Tests::D3D8FrontendHarness& Tests::D3D8Scene::get_harness()
{
    return m_harness;
}

uint64_t Tests::D3D8Scene::get_index_buffer() const
{
    return m_index_buffer;
}

uint64_t Tests::D3D8Scene::get_vertex_buffer() const
{
    return m_vertex_buffer;
}

void Tests::D3D8Scene::present()
{
    m_builder.present();

    feed();
}

void Tests::D3D8Scene::set_view(const float& in_marker)
{
    Interception::Hooks::D3D8::D3DMATRIX matrix = {};

    matrix.m[0][0] = in_marker;

    m_builder.set_transform(Interception::Hooks::D3D8::D3DTS_VIEW,
                            matrix);

    feed();
}
