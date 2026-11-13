/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the front end of OpenGL does with the calls that set the state of a context: the values that a clear uses, the capabilities that are switched on and off,
 * the values of the pipeline that a draw is told apart by (a function, a mode, a mask), the viewport and the scissor box, and the stack of attributes (glPushAttrib
 * keeps what the program sets after it, and glPopAttrib puts it back). The state is that of the context that the call is made for, which the front end gives.
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

                /* How many of the calls that set the state are not followed, and the front end goes on without them. */
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

                /* What was put back may be another viewport, which the builder is told of if the window is made (it has none before). */
                void handle_pop_attributes(const GLWindow& in_window,
                                           GLContextState* inout_state_ptr);

                void handle_push_attributes(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            GLContextState*                              inout_state_ptr);

                /* A scissor box is a call that depends on the size of what is rendered to: the counter counts it. */
                void handle_scissor(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                    ResolutionHazardCounter*                     inout_counter_ptr,
                                    GLContextState*                              inout_state_ptr);

                /* A call which sets a value of the pipeline that a draw is told apart by (a function, a mode, a mask). */
                void handle_state_value(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        GLContextState*                              inout_state_ptr);

                /* The model has the viewport of what is rendered to, and there is none before the window is made: the builder is told only after. */
                void handle_viewport(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     const GLWindow&                              in_window,
                                     GLContextState*                              inout_state_ptr);

            private:
                /* True if the two boxes are the same. */
                static bool is_same_box(const GLBox& in_first,
                                        const GLBox& in_second);

                /* A box from the four arguments of glViewport or glScissor. */
                static bool read_box(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     GLBox*                                       out_box_ptr);

                /* Puts back what the mask of a glPushAttrib asked to be saved. */
                static void restore_attributes(const GLAttributeFrame& in_frame,
                                               GLAttributes*           inout_attributes_ptr);

                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
                uint64_t                                 m_n_unfollowed_calls;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_STATE_HANDLER_H */
