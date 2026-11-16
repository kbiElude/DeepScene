/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Follows the calls of an OpenGL application (GL, WGL and SwapBuffers) and tells the model builder what they amount to.
 *
 * What it follows:
 *  - the contexts, and which context each thread has current;
 *  - per context: the viewport, the scissor box and scissor test, the values that a clear uses, the attribute stack (glPopAttrib restores what
 *    the program set after glPushAttrib), and the matrix stacks (model-view and projection are used, the other stacks are only kept consistent);
 *  - the clears;
 *  - the draws of immediate mode (a glBegin and its glEnd) and of arrays (glDrawArrays and glDrawElements, using the copy of the arrays made
 *    by the dumper), each with the pipeline state that was set;
 *  - the ends of frames;
 *  - display lists: the calls made while a list is being made are stored in the list together with the data they refer to. They run when the
 *    list is called (glCallList; the lists of all contexts are shared), and also at once for GL_COMPILE_AND_EXECUTE.
 *
 * What it does not follow ends the process with a message that names the function and the reason (see UnsupportedUse): calls that write the
 * framebuffer without being a draw (glBitmap, glDrawPixels, glCopyPixels), glCallLists, a texture that is created or changed in an unsupported
 * way (see GLTextureHandler::is_unsupported_texture_function()), and state that decides what a draw writes
 * (see GLFunctionPolicy::changes_what_a_draw_writes()). A read of the framebuffer (glReadPixels) is only counted as a pixel readback hazard.
 *
 * The front end owns the contexts and dispatches the calls (handle_gl_call). The handling of each kind of call is in a class of its own. These
 * classes keep no pointer to the front end; whatever they need from the other classes is passed with the call: GLTextureHandler (textures),
 * GLMatrixHandler (matrices), GLListHandler (display lists), GLStateHandler (state values, viewport, scissor box, attributes) and
 * GLDrawHandler (draws and clears).
 *
 * Matrices: OpenGL has one matrix for model and view together, and a separate projection matrix. The model builder receives both unchanged,
 * each when it changes (MODEL_VIEW_MATRIX and PROJECTION_MATRIX in the fixed-function pipeline state, at the offsets 0 and 64). It cannot
 * separate model from view, because the calls do not either.
 *
 * The window (GLWindow) is the default framebuffer. There is one for the whole workload. It is created at the first clear, with the size of the
 * viewport that the context has at that moment. The window size is not in the calls: a context starts with a viewport that covers the whole
 * window, and the application changes it in its first glViewport.
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

                /* Finds how the display list depends on the per-eye state (matrices, viewport, scissor box); false if the list does not exist. */
                bool find_list_eye_dependence(const uint32_t&      in_list,
                                              GLListEyeDependence* out_dependence_ptr) const;

                bool find_resource_view(const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const override;

                /* The data that the draws read from the memory of the application, kept while it is worth keeping (see GLBlobStore). */
                const GLBlobStore& get_blob_store() const;

                /* The current model-view and projection matrices of the context that the thread has current (used by the tests);
                 * false if the thread has no current context.
                 */
                bool get_current_matrices(const uint32_t& in_thread_id,
                                          GLMatrix*       out_modelview_f32mat_ptr,
                                          GLMatrix*       out_projection_f32mat_ptr) const;

                const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& get_n_resolution_hazards() const override;
                uint64_t                                                               get_n_unfollowed_calls  () const override;
                uint64_t                                                               get_n_unsupported_calls () const override;
                void                                                                   on_call                 (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                                                const Capture::APIDumpLoader::Workload&      in_workload) override;

            private:

                /* Copies the pixels of the window into a region of a level of the texture bound to the active unit (the depth of the window for a depth
                 * texture). Returns false if the program has not given the texture or the level a size.
                 */
                bool copy_to_bound_texture(const GLContextState& in_state,
                                           const uint32_t&       in_level,
                                           const uint32_t&       in_x_offset,
                                           const uint32_t&       in_y_offset,
                                           const uint32_t&       in_width,
                                           const uint32_t&       in_height);

                /* The state of the context that the thread has current; nullptr if the thread has none. */
                GLContextState* find_current_context_ptr(const uint32_t& in_thread_id);

                /* Handles glBindBuffer. Binding a pixel-pack buffer ends the process: glReadPixels would write to the buffer, and the data would be in no
                 * call. Binding a pixel-unpack buffer ends it too: the pixels of an upload would be in the buffer instead of the call. Any other buffer
                 * object (vertices, indices, constants) ends it as well, because buffers are not followed. Unbinding (buffer 0) is allowed.
                 */
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

                /* Ends the process with a message that names the unsupported function or value and the reason (AI_ASSERT_ALWAYS). The analysis would
                 * be wrong or incomplete otherwise, and nothing is dropped silently.
                 */
                static void report_unsupported_use(const APIInterceptor::APIFunction& in_function,
                                                   const char*                        in_reason_ptr,
                                                   const uint64_t&                    in_value);

                /* A new context: the defaults from the specification, and one identity matrix on each matrix stack. */
                static GLContextState get_initial_context_state();

                /* True for the calls whose effects the front end does not follow and that are not texture calls (they write the framebuffer from
                 * somewhere other than a draw, or call lists named by an array). Such a call ends the process (see report_unsupported_use()).
                 */
                static bool is_unsupported_function(const APIInterceptor::APIFunction& in_function);

                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
                GLContextState*                          m_cached_context_state_ptr;      /* the state that find_current_context_ptr() found last, for m_cached_thread_id; nullptr: none */
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
