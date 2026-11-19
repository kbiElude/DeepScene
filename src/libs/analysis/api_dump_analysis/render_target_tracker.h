/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the application renders to now: the view of the color target and the view of the depth and stencil target. The front ends of the APIs
 * work out which resources those are from their own calls (SetRenderTarget, the default framebuffer, the surface a depth buffer is attached to);
 * this class keeps them and tells the model builder when they change, which ends the pass that was open (see ModelBuilder::on_set_attachments()).
 */
#ifndef APIDUMPANALYSIS_RENDER_TARGET_TRACKER_H
#define APIDUMPANALYSIS_RENDER_TARGET_TRACKER_H

#include "analysis/api_dump_analysis/model_builder.h"
#include "analysis/api_dump_analysis/types.h"

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class RenderTargetTracker
        {
        public:
            /* The builder must outlive the tracker. */
            explicit RenderTargetTracker(ModelBuilder* in_builder_ptr);

            /* Forgets the color view, without telling the builder: the next set() sees a new target. */
            void forget_color_view();

            const ResourceView& get_color_view() const;

            /* resource_id is NONE if there is none. */
            const ResourceView& get_depth_stencil_view() const;

            /* Makes the views what the application renders to. The builder is told if either of them changed, or whatever changed if in_tells_unchanged.
             * Returns whether the color view changed: a new target starts with a viewport that covers all of it, until the application sets another one.
             */
            bool set(const ResourceView& in_color_view,
                     const ResourceView& in_depth_stencil_view,
                     const bool&         in_tells_unchanged);

        private:
            ModelBuilder* m_builder_ptr;
            ResourceView  m_color_view;
            ResourceView  m_depth_stencil_view;
        };
    }
}

#endif /* APIDUMPANALYSIS_RENDER_TARGET_TRACKER_H */
