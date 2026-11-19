/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_window.h"

Analysis::APIDumpAnalysis::Frontends::GLWindow::GLWindow(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_builder_ptr          (in_builder_ptr),
     m_framebuffer_height   (0),
     m_framebuffer_width    (0),
     m_has_default_framebuffer(false),
     m_render_target_tracker(in_builder_ptr)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::Frontends::GLWindow::ensure_default_framebuffer(const GLContextState& in_state)
{
    if (m_has_default_framebuffer == false)
    {
        Analysis::APIDumpAnalysis::ResourceDescription color_description;
        Analysis::APIDumpAnalysis::ResourceView        color_view;
        Analysis::APIDumpAnalysis::ResourceView        depth_stencil_view;
        Analysis::APIDumpAnalysis::ResourceDescription depth_description;
        const Analysis::APIDumpAnalysis::Viewport      viewport = get_model_viewport(in_state);

        m_framebuffer_height = static_cast<uint32_t>(viewport.height);
        m_framebuffer_width  = static_cast<uint32_t>(viewport.width);

        color_description.format.format_class = Analysis::APIDumpAnalysis::FormatClass::COLOR;
        color_description.height              = m_framebuffer_height;
        color_description.is_render_target    = true;
        color_description.kind                = Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE;
        color_description.width               = m_framebuffer_width;

        depth_description.format.format_class = Analysis::APIDumpAnalysis::FormatClass::DEPTH_STENCIL;
        depth_description.height              = m_framebuffer_height;
        depth_description.is_depth_stencil    = true;
        depth_description.kind                = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D;
        depth_description.width               = m_framebuffer_width;

        color_view.resource_id         = m_builder_ptr->on_create_resource(color_description,
                                                                           "Default framebuffer: color");
        depth_stencil_view.resource_id = m_builder_ptr->on_create_resource(depth_description,
                                                                           "Default framebuffer: depth and stencil");

        m_render_target_tracker.set(color_view,
                                    depth_stencil_view,
                                    true); /* in_tells_unchanged */

        m_builder_ptr->on_set_viewport(viewport);

        m_has_default_framebuffer = true;
    }
}

const Analysis::APIDumpAnalysis::ResourceView& Analysis::APIDumpAnalysis::Frontends::GLWindow::get_color_view() const
{
    return m_render_target_tracker.get_color_view();
}

const Analysis::APIDumpAnalysis::ResourceView& Analysis::APIDumpAnalysis::Frontends::GLWindow::get_depth_stencil_view() const
{
    return m_render_target_tracker.get_depth_stencil_view();
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GLWindow::get_framebuffer_height() const
{
    return m_framebuffer_height;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GLWindow::get_framebuffer_width() const
{
    return m_framebuffer_width;
}

Analysis::APIDumpAnalysis::Viewport Analysis::APIDumpAnalysis::Frontends::GLWindow::get_model_viewport(const GLContextState& in_state)
{
    Analysis::APIDumpAnalysis::Viewport result;

    result.height = static_cast<float>(in_state.attributes.viewport.height);
    result.width  = static_cast<float>(in_state.attributes.viewport.width);
    result.x      = static_cast<float>(in_state.attributes.viewport.x);
    result.y      = static_cast<float>(in_state.attributes.viewport.y);

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GLWindow::has_default_framebuffer() const
{
    return m_has_default_framebuffer;
}
