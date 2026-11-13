/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Follows the calls of an OpenGL application (GL, WGL and SwapBuffers) and tells the model builder what they come to.
 *
 * What it follows: the contexts and which one each thread has current; of each, the viewport, the scissor box and test, the values that a
 * clear uses, the stack of attributes (glPushAttrib puts back what the program set after it) and the stacks of matrices (a model-view, a
 * projection, and the others, which are only kept right); the clears; the draws of immediate mode (a glBegin and its glEnd) and of
 * arrays (glDrawArrays and glDrawElements, from the copy of the arrays that the dumper made), each with the state of the pipeline that
 * was set; and the end of the frames. A display list is followed: the calls made while it is made are kept in it (with the data that they refer
 * to), and made when it is called (glCallList; the lists of the contexts are shared), and at once too for GL_COMPILE_AND_EXECUTE. What it does
 * not follow ends the process, saying the function and why (see UnsupportedUse): what writes the framebuffer and is not a draw (glBitmap,
 * glDrawPixels, glCopyPixels), glCallLists, a texture that is made or changed in a way that is not followed (see
 * GLTextureHandler::is_unsupported_texture_function()), and the state that decides what a draw writes (see GLFunctionPolicy::changes_what_a_draw_writes()).
 * A read of the framebuffer (glReadPixels) is only counted, as a hazard of pixel readbacks.
 *
 * The front end keeps the contexts and dispatches the calls (handle_gl_call); what a kind of call does is in a class of its own, which keeps no
 * address of the front end (what it needs of the others is given with the call): GLTextureHandler (textures), GLMatrixHandler (matrices),
 * GLListHandler (display lists), GLStateHandler (state values, viewport, scissor box, attributes) and GLDrawHandler (draws and clears).
 *
 * The matrices: OpenGL has one matrix for the model and the view together, and a projection of its own. The model builder is told the two as
 * they are, each when it changes (MODEL_VIEW_MATRIX and PROJECTION_MATRIX, in the state of the fixed-function pipeline, at the offsets 0 and
 * 64), and does not tell a model from a view, which the calls do not.
 *
 * The window (GLWindow) is the default framebuffer: one of them for the whole workload, made at the first clear with the size of the viewport
 * that the context has by then (the size of the window is not in the calls; a context starts with a viewport of the whole window, and the
 * application says what it is in its first glViewport).
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_draw_handler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_context_state.h"
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

                /* What the list does to what an eye has of its own; false if there is no such list. */
                bool find_list_eye_dependence(const uint32_t&      in_list,
                                              GLListEyeDependence* out_dependence_ptr) const;

                bool find_resource_view(const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const override;

                /* The data that the draws read from the memory of the application, kept for as long as it is worth keeping (see GLBlobStore). */
                const GLBlobStore& get_blob_store() const;

                /* The current model-view and projection matrices of the context that the thread has current (for the checks of the tests);
                 * false if it has none.
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

                /* The pixels of the window are copied to a region of a level of the texture that the active unit has bound (the depth of the window, for a texture of
                 * depth); false if the program has not given the texture a size, or the level.
                 */
                bool copy_to_bound_texture(const GLContextState& in_state,
                                           const uint32_t&       in_level,
                                           const uint32_t&       in_x_offset,
                                           const uint32_t&       in_y_offset,
                                           const uint32_t&       in_width,
                                           const uint32_t&       in_height);

                /* The state of the context that the thread has current; nullptr if it has none. */
                GLContextState* find_current_context_ptr(const uint32_t& in_thread_id);

                /* glBindBuffer: a buffer that pixels are packed into (glReadPixels then reads to the buffer, and what is read is in no call) or unpacked from (the
                 * pixels of an upload are then in the buffer, not in the call) is not followed: the process is ended. So is any other buffer object (of vertices, of
                 * indices, of constants): buffers are not followed. Unbinding one (the buffer 0) is not that.
                 */
                void handle_bind_buffer(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                /* A call of glCopyTexImage2D: the pixels of the window give the texture its size, and are copied to it. */
                void handle_copy_tex_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           GLContextState*                              inout_state_ptr);

                /* A call of glCopyTexSubImage2D: the pixels of the window are copied to a part of the texture. */
                void handle_copy_tex_sub_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                               GLContextState*                              inout_state_ptr);

                /* A call of GL: kept in the list that its context is making, or made, or both. It is also how the calls of a called list are made. */
                void handle_gl_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                    const Capture::APIDumpLoader::Workload&      in_workload);

                void handle_make_current(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                /* What the program does is not followed (a target, a kind of texture, a buffer that pixels go to or come from, a call): what the analysis finds of it
                 * would be wrong or left out, and so the process is ended, saying what it was (AI_ASSERT_ALWAYS). Nothing is left out without being said.
                 */
                static void report_unsupported_use(const APIInterceptor::APIFunction& in_function,
                                                   const char*                        in_reason_ptr,
                                                   const uint64_t&                    in_value);

                /* A context as it starts: what the specification says, and a stack of the identity for each kind of matrix. */
                static GLContextState get_initial_context_state();

                /* True for the calls whose effects the front end does not follow, and that are not about textures (they write the framebuffer from somewhere else, or
                 * call lists that are named by an array): one of them ends the process (see report_unsupported_use()).
                 */
                static bool is_unsupported_function(const APIInterceptor::APIFunction& in_function);

                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
                GLContextState*                          m_cached_context_state_ptr;      /* what find_current_context_ptr() found last, for m_cached_thread_id; nullptr: nothing */
                uint32_t                                 m_cached_thread_id;
                std::map<uint64_t, GLContextState>       m_context_map;                   /* by the address of the context */
                std::map<uint32_t, uint64_t>             m_current_context_by_thread_map; /* the address, by the thread    */
                GLDrawHandler                            m_draw_handler;
                GLListHandler                            m_list_handler;
                GLMatrixHandler                          m_matrix_handler;
                uint64_t                                 m_n_unfollowed_calls;
                uint64_t                                 m_n_unsupported_calls;
                ResolutionHazardCounter                  m_resolution_hazard_counter;
                GLStateHandler                           m_state_handler;
                GLTextureHandler                         m_texture_handler;
                GLWindow                                 m_window;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H */
