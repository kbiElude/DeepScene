/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Handles the OpenGL calls that set the state of a context: the values that a clear uses, the capabilities that are switched on and off, the pipeline
 * values that distinguish draws (a function, a mode, a mask), the viewport and the scissor box, and the attribute stack (glPushAttrib saves the
 * state, and glPopAttrib restores it). The state belongs to the context that the call is made for. The front end passes that context in.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_STATE_HANDLER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_STATE_HANDLER_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_context_state.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_window.h"
#include "analysis/api_dump_analysis/frontends/shared/resolution_hazard_counter.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLStateHandler
            {
            public:
                /* The builder must outlive the handler. */
                explicit GLStateHandler(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* The number of state calls that are not followed. The front end continues without them. */
                uint64_t get_n_unfollowed_calls() const;

                void handle_clear_color  (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GLContextState*                              inout_state_ptr);
                void handle_clear_depth  (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GLContextState*                              inout_state_ptr);
                void handle_clear_stencil(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GLContextState*                              inout_state_ptr);

                void handle_enable(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const bool&                                  in_is_enabled,
                                   GLContextState*                              inout_state_ptr);

                /* The restored state may contain a different viewport. The builder is told about it only if the window exists (there is no viewport before that). */
                void handle_pop_attributes(const GLWindow& in_window,
                                           GLContextState* inout_state_ptr);

                void handle_pop_client_attributes(GLContextState* inout_state_ptr);

                void handle_push_attributes(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            GLContextState*                              inout_state_ptr);

                /* Saves the unpack state of pixels if the mask has GL_CLIENT_PIXEL_STORE_BIT. The state of the vertex arrays is not kept, so
                 * the bit for it saves nothing.
                 */
                void handle_push_client_attributes(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                   GLContextState*                              inout_state_ptr);

                /* A scissor box depends on the size of the render target, so the counter counts it as a resolution hazard. */
                void handle_scissor(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                    ResolutionHazardCounter*                     inout_counter_ptr,
                                    GLContextState*                              inout_state_ptr);

                /* A call that sets a pipeline value that distinguishes draws (a function, a mode, a mask). */
                void handle_state_value(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        GLContextState*                              inout_state_ptr);

                /* The model has the viewport of the render target. There is none before the window is created, so the builder is told only after that. */
                void handle_viewport(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     const GLWindow&                              in_window,
                                     GLContextState*                              inout_state_ptr);

            private:
                /* Returns true if the two boxes are equal. */
                static bool is_same_box(const GLBox& in_first,
                                        const GLBox& in_second);

                /* Reads a box from the four arguments of glViewport or glScissor. */
                static bool read_box(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     GLBox*                                       out_box_ptr);

                /* Restores what the mask of the matching glPushAttrib selected for saving. */
                static void restore_attributes(const GLAttributeFrame& in_frame,
                                               GLAttributes*           inout_attributes_ptr);

                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
                uint64_t                                 m_n_unfollowed_calls;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_STATE_HANDLER_H */
