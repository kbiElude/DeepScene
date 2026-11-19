/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/render_target_tracker.h"

Analysis::APIDumpAnalysis::RenderTargetTracker::RenderTargetTracker(ModelBuilder* in_builder_ptr)
    :m_builder_ptr(in_builder_ptr)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::RenderTargetTracker::forget_color_view()
{
    m_color_view = ResourceView();
}

const Analysis::APIDumpAnalysis::ResourceView& Analysis::APIDumpAnalysis::RenderTargetTracker::get_color_view() const
{
    return m_color_view;
}

const Analysis::APIDumpAnalysis::ResourceView& Analysis::APIDumpAnalysis::RenderTargetTracker::get_depth_stencil_view() const
{
    return m_depth_stencil_view;
}

bool Analysis::APIDumpAnalysis::RenderTargetTracker::set(const ResourceView& in_color_view,
                                                         const ResourceView& in_depth_stencil_view,
                                                         const bool&         in_tells_unchanged)
{
    const bool is_color_changed = ( (m_color_view == in_color_view) == false);
    const bool is_changed       = (is_color_changed                                           ||
                                   (m_depth_stencil_view == in_depth_stencil_view) == false);

    m_color_view         = in_color_view;
    m_depth_stencil_view = in_depth_stencil_view;

    if (is_changed ||
        in_tells_unchanged)
    {
        AttachmentSet attachments;

        attachments.color_view_vec.push_back(m_color_view);

        attachments.depth_stencil_view = m_depth_stencil_view;

        m_builder_ptr->on_set_attachments(attachments);
    }

    return is_color_changed;
}
