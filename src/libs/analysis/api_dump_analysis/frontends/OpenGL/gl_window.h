/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The window of the program as the default framebuffer: what is rendered to when no framebuffer object is bound. OpenGL does not make it, so it is made for the
 * model when it is first needed (the first clear or draw), with the size that the viewport has by then, and the builder is told what is rendered to and where.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_WINDOW_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_WINDOW_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_context_state.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLWindow
            {
            public:
                /* The builder must outlive the window. */
                explicit GLWindow(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* Makes the resources of the window, and tells the builder what is rendered to and where, if that has not been done. */
                void ensure_default_framebuffer(const GLContextState& in_state);

                const Analysis::APIDumpAnalysis::ResourceView& get_color_view() const;

                /* resource_id is NONE until the window is made. */
                const Analysis::APIDumpAnalysis::ResourceView& get_depth_stencil_view() const;

                /* The size that the window was made with; 0 until it is made. */
                uint32_t get_framebuffer_height() const;
                uint32_t get_framebuffer_width () const;

                bool has_default_framebuffer() const;

                /* The viewport of the model, from the box of the context. */
                static Analysis::APIDumpAnalysis::Viewport get_model_viewport(const GLContextState& in_state);

            private:
                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
                Analysis::APIDumpAnalysis::ResourceView  m_color_view;
                Analysis::APIDumpAnalysis::ResourceView  m_depth_stencil_view;
                uint32_t                                 m_framebuffer_height;
                uint32_t                                 m_framebuffer_width;
                bool                                     m_has_default_framebuffer;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_WINDOW_H */
