/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d7_scene.h"
#include "Interception/Core/D3D7Utils/constants.h"

Tests::D3D7Scene::D3D7Scene()
    :m_back_buffer_surface(0),
     m_depth_surface      (0),
     m_primary_surface    (0)
{
    constexpr uint32_t N_BACK_BUFFERS = 1;
    constexpr uint32_t SCREEN_HEIGHT  = 480;
    constexpr uint32_t SCREEN_WIDTH   = 640;

    m_builder.set_display_mode(SCREEN_WIDTH,
                               SCREEN_HEIGHT);

    m_primary_surface     = m_builder.create_primary_surface(N_BACK_BUFFERS);
    m_back_buffer_surface = m_builder.get_attached_surface  (m_primary_surface,
                                                             Interception::Core::D3D7Utils::DDSCAPS_BACKBUFFER);
    m_depth_surface       = m_builder.create_depth_buffer   ();

    m_builder.add_attached_surface(m_back_buffer_surface,
                                   m_depth_surface);
    m_builder.create_device       (m_back_buffer_surface);

    feed();
}

void Tests::D3D7Scene::draw(const uint32_t& in_n_draws)
{
    constexpr uint32_t FVF_XYZ_DIFFUSE = 0x042;
    constexpr uint32_t N_VERTICES      = 3;

    for (uint32_t n_draw = 0;
                  n_draw < in_n_draws;
                ++n_draw)
    {
        m_builder.draw_primitive(Interception::Hooks::D3D7::D3DPT_TRIANGLELIST,
                                 FVF_XYZ_DIFFUSE,
                                 N_VERTICES);
    }

    feed();
}

void Tests::D3D7Scene::feed()
{
    m_harness.feed(m_builder);
}

uint64_t Tests::D3D7Scene::get_back_buffer_surface() const
{
    return m_back_buffer_surface;
}

Tests::D3D7CallBuilder& Tests::D3D7Scene::get_builder()
{
    return m_builder;
}

uint64_t Tests::D3D7Scene::get_depth_surface() const
{
    return m_depth_surface;
}

Tests::D3D7FrontendHarness& Tests::D3D7Scene::get_harness()
{
    return m_harness;
}

uint64_t Tests::D3D7Scene::get_primary_surface() const
{
    return m_primary_surface;
}

void Tests::D3D7Scene::present()
{
    m_builder.flip(m_primary_surface);

    feed();
}
