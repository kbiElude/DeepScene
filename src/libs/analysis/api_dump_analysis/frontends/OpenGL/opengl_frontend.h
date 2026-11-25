/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Follows the OpenGL calls (GL, WGL, SwapBuffers) and tells the model builder what they amount to.
 * Unfollowed calls end the process naming function and reason (see UnsupportedUse). glReadPixels is only counted as a readback hazard.
 * Per-call handling lives in GLTextureHandler, GLMatrixHandler, GLListHandler, GLStateHandler and GLDrawHandler; they keep no front end pointer.
 * The model builder gets the model-view and projection matrices unchanged (offsets 0 and 64), as GL does not separate model from view.
 * The window (GLWindow, one per workload) is created at the first clear, sized by the viewport the context has then.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_draw_handler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_context_state.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_window_extents.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_list_handler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_matrix_handler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_state_handler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_window.h"
#include "analysis/api_dump_analysis/frontends/shared/resolution_hazard_counter.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_texture_handler.h"
#include "analysis/api_dump_analysis/frontends/shared/i_api_frontend.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include <map>
#include <memory>
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class OpenGLFrontend : public IAPIFrontend
            {
            public:
                /* The builder must outlive the front end. */
                explicit OpenGLFrontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* Counts vertices in the open block of the thread's current context that were not given as calls (see GLDrawHandler::add_block_vertices). */
                void add_block_vertices(const uint32_t& in_thread_id,
                                        const uint32_t& in_n_vertices);

                /* Finds how the display list depends on the per-eye state (matrices, viewport, scissor box); false if the list does not exist. */
                bool find_list_eye_dependence(const uint32_t&      in_list,
                                              GLListEyeDependence* out_dependence_ptr) const;

                bool find_resource_view(const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const override;

                /* The current model-view and projection matrices of the thread's current context, and the matrix mode; false if none. */
                bool get_current_matrices(const uint32_t& in_thread_id,
                                          GLMatrix*       out_modelview_f32mat_ptr,
                                          GLMatrix*       out_projection_f32mat_ptr,
                                          uint32_t*       out_matrix_mode_ptr) const;

                const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& get_n_resolution_hazards() const override;
                uint64_t                                                               get_n_unfollowed_calls  () const override;
                uint64_t                                                               get_n_unsupported_calls () const override;
                void                                                                   on_call                 (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                                                const Capture::APIDumpLoader::Workload&      in_workload) override;

            private:

                /* Copies window pixels (depth for a depth texture) into a level region of the active unit's texture; false if it has no size. */
                bool copy_to_bound_texture(const GLContextState& in_state,
                                           const uint32_t&       in_level,
                                           const uint32_t&       in_x_offset,
                                           const uint32_t&       in_y_offset,
                                           const uint32_t&       in_width,
                                           const uint32_t&       in_height);

                /* The state of the context that the thread has current; nullptr if the thread has none. */
                GLContextState* find_current_context_ptr(const uint32_t& in_thread_id);

                /* Handles glBindBuffer. Binding any buffer object ends the process (buffers are not followed); unbinding (0) is allowed. */
                void handle_bind_buffer(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                /* Handles glCopyTexImage2D: the pixels of the window give the texture its size and are copied into it. */
                void handle_copy_tex_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           GLContextState*                              inout_state_ptr);

                /* Handles glCopyTexSubImage2D: the pixels of the window are copied into a part of the texture. */
                void handle_copy_tex_sub_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                               GLContextState*                              inout_state_ptr);

                /* Handles a GL call: stores it in the list that its context is making, runs it, or both. Also used to run the calls of a called list. */
                void handle_gl_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                    const Capture::APIDumpLoader::Workload&      in_workload);

                void handle_make_current(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                /* Ends the process for a function that the front end has no handler for, unless it is one that is left out by design (see GLFunctionClassifier): the
                 * reason is the one of the group that the function is in. Nothing is dropped silently.
                 */
                static void report_unfollowed_function(const APIInterceptor::APIFunction& in_function);

                /* Ends the process (AI_ASSERT_ALWAYS), naming the unsupported function or value and the reason; nothing is dropped silently. */
                static void report_unsupported_use(const APIInterceptor::APIFunction& in_function,
                                                   const char*                        in_reason_ptr,
                                                   const uint64_t&                    in_value);

                /* A new context: the defaults from the specification, and one identity matrix on each matrix stack. */
                static GLContextState get_initial_context_state();

                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
                GLContextState*                          m_cached_context_state_ptr;      /* last find_current_context_ptr() result, for m_cached_thread_id; nullptr: none */
                uint32_t                                 m_cached_thread_id;
                std::map<uint64_t, GLContextState>       m_context_map;                   /* by context address            */
                std::map<uint32_t, uint64_t>             m_current_context_by_thread_map; /* context address, by thread id */
                GLDrawHandler                            m_draw_handler;
                GLListHandler                            m_list_handler;
                GLMatrixHandler                          m_matrix_handler;
                uint64_t                                 m_n_unfollowed_calls;
                uint64_t                                 m_n_unsupported_calls;
                ResolutionHazardCounter                  m_resolution_hazard_counter;
                GLStateHandler                           m_state_handler;
                GLTextureHandler                         m_texture_handler;
                GLWindow                                 m_window;
                GLWindowExtents                          m_window_extents;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H */
