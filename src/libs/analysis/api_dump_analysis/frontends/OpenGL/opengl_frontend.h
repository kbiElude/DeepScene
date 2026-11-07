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
 * not follow yet (glCallLists, copies and reads of the framebuffer) is counted as unsupported, so that a model made of it says that its
 * passes are not right.
 *
 * The matrices: OpenGL has one matrix for the model and the view together, and a projection of its own. The front end tells the model builder
 * the two as they are, each when it changes (MODEL_VIEW_MATRIX and PROJECTION_MATRIX, in the state of the fixed-function pipeline, at
 * the offsets 0 and 64), and does not tell a model from a view, which the calls do not.
 *
 * The window is the default framebuffer: one of them for the whole workload, made at the first clear with the size of the viewport that
 * the context has by then (the size of the window is not in the calls; a context starts with a viewport of the whole window, and the
 * application says what it is in its first glViewport).
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_array_draw_describer.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_context_state.h"
#include "analysis/api_dump_analysis/frontends/shared/i_api_frontend.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "common/argument_reader.h"
#include <array>
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

                bool find_resource_view(const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const override;

                /* The data that the draws read from the memory of the application, kept for as long as it is worth keeping (see GlBlobStore). */
                const GlBlobStore& get_blob_store() const;

                /* The current model-view and projection matrices of the context that the thread has current (for the checks of the tests);
                 * false if it has none.
                 */
                bool get_current_matrices(const uint32_t& in_thread_id,
                                          GlMatrix*       out_modelview_f32mat_ptr,
                                          GlMatrix*       out_projection_f32mat_ptr) const;

                const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& get_n_resolution_hazards() const override;

                uint64_t get_n_unfollowed_calls() const override;

                uint64_t get_n_unsupported_calls() const override;

                void on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                             const Capture::APIDumpLoader::Workload&      in_workload) override;

            private:
                /* Makes the resources of the window, and tells the builder what is rendered to and where, if that has not been done. */
                void ensure_default_framebuffer(const GlContextState& in_state);

                /* The state of the context that the thread has current; nullptr if it has none. */
                GlContextState* find_current_context_ptr(const uint32_t& in_thread_id);

                /* A call of glDrawArrays or glDrawElements: a draw from the arrays that the dumper copied into it. */
                void handle_array_draw      (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             const Capture::APIDumpLoader::Workload&      in_workload,
                                             const bool&                                  in_is_indexed,
                                             GlContextState*                              inout_state_ptr);
                void handle_begin           (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);
                /* A call of glCallList: the calls of the list are made, as the list has them. */
                void handle_call_list       (const Capture::APIDumpLoader::DumpedAPICall& in_call);
                void handle_clear           (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);
                void handle_clear_color     (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);
                void handle_clear_depth     (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);
                void handle_clear_stencil   (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);
                /* The lists of glDeleteLists are gone. */
                void handle_delete_lists    (const Capture::APIDumpLoader::DumpedAPICall& in_call);
                void handle_enable          (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             const bool&                                  in_is_enabled,
                                             GlContextState*                              inout_state_ptr);
                /* The vertices since the glBegin are a draw. */
                void handle_end             (GlContextState* inout_state_ptr);
                /* A list which was being made is made, and its calls are made when it is called. */
                void handle_end_list        (GlContextState* inout_state_ptr);
                /* A call of GL: kept in the list that its context is making, or made, or both. It is also how the calls of a called list are made. */
                void handle_gl_call         (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             const Capture::APIDumpLoader::Workload&      in_workload);
                void handle_make_current    (const Capture::APIDumpLoader::DumpedAPICall& in_call);
                /* A call that loads or multiplies the current matrix: the matrix is what its arguments (or its data) say. */
                void handle_matrix          (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             const Capture::APIDumpLoader::Workload&      in_workload,
                                             GlContextState*                              inout_state_ptr);
                void handle_matrix_mode     (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);
                void handle_new_list        (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);
                void handle_pop_attributes  (GlContextState* inout_state_ptr);
                void handle_pop_matrix      (GlContextState* inout_state_ptr);
                void handle_push_attributes (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);
                void handle_push_matrix     (GlContextState* inout_state_ptr);
                void handle_scissor         (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);
                /* A call which sets a value of the pipeline that a draw is told apart by (a function, a mode, a mask). */
                void handle_state_value     (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);
                /* A glVertex of the block that is open: it is counted, and its position kept if the builder wants the geometry. */
                void handle_vertex          (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             const Capture::APIDumpLoader::Workload&      in_workload,
                                             GlContextState*                              inout_state_ptr);
                void handle_viewport        (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             GlContextState*                              inout_state_ptr);

                /* Keeps the call in the list that the context is making, with the data that it refers to. */
                void record_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                 const Capture::APIDumpLoader::Workload&      in_workload,
                                 GlContextState*                              inout_state_ptr);

                /* Tells the model builder the model-view and the projection matrix of the context, those of them which are not what it was told last. */
                void report_matrices(const GlContextState& in_state);

                void report_matrix(const GlMatrix&                                   in_matrix_f32mat,
                                   const Analysis::APIDumpAnalysis::ConstantSemantic& in_semantic,
                                   const uint64_t&                                   in_byte_offset,
                                   GlReportedMatrix*                                 inout_reported_ptr);

                /* The stack of the matrices that glMatrixMode has chosen. */
                static std::vector<GlMatrix>* get_current_stack_ptr(GlContextState* inout_state_ptr);

                /* A context as it starts: what the specification says, and a stack of the identity for each kind of matrix. */
                static GlContextState get_initial_context_state();

                /* The viewport of the model, from the box of the context. */
                static Analysis::APIDumpAnalysis::Viewport get_model_viewport(const GlContextState& in_state);

                /* What the mode of a glBegin makes of the vertices; quads are two triangles and a loop of lines is a strip of them. */
                static Analysis::APIDumpAnalysis::PrimitiveTopology get_primitive_topology(const uint32_t& in_mode);

                /* True for the calls whose effects the front end does not follow yet. */
                static bool is_unsupported_function(const APIInterceptor::APIFunction& in_function);

                /* True if the two boxes are the same. */
                static bool is_same_box(const GlBox& in_first,
                                        const GlBox& in_second);

                /* Reads the first N arguments of the call as numbers; false if it has fewer, or one that is not a number. */
                template<typename T, size_t N>
                static bool read_arguments(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           std::array<T, N>*                            out_value_array_ptr)
                {
                    bool result = (in_call.arg_in_vec.size() >= N);

                    for (size_t n_value = 0;
                                result   &&
                                n_value < N;
                              ++n_value)
                    {
                        result = APIInterceptor::read_number(in_call.arg_in_vec.at(n_value),
                                                             &out_value_array_ptr->at(n_value) );
                    }

                    return result;
                }

                /* A box from the four arguments of glViewport or glScissor. */
                static bool read_box(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     GlBox*                                       out_box_ptr);

                /* The matrix that a call of glLoadMatrix or glMultMatrix has stored as its argument, of floats or of doubles. */
                static bool read_matrix_chunk(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                              const Capture::APIDumpLoader::Workload&      in_workload,
                                              const bool&                                  in_is_double,
                                              GlMatrix*                                    out_matrix_f32mat_ptr);

                /* The matrix that the call has to load into the current one, or to multiply it by; false if its arguments cannot be read. */
                static bool read_matrix_operand(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                const Capture::APIDumpLoader::Workload&      in_workload,
                                                GlMatrix*                                    out_matrix_f32mat_ptr);

                /* Puts back what the mask of a glPushAttrib asked to be saved. */
                static void restore_attributes(const GlAttributeFrame& in_frame,
                                               GlAttributes*           inout_attributes_ptr);

                GlArrayDrawDescriber                                            m_array_draw_describer;
                Analysis::APIDumpAnalysis::ModelBuilder*                        m_builder_ptr;
                Analysis::APIDumpAnalysis::ResourceView                         m_color_view;
                std::map<uint64_t, GlContextState>                              m_context_map;                   /* by the address of the context */
                std::map<uint32_t, uint64_t>                                    m_current_context_by_thread_map; /* the address, by the thread    */
                Analysis::APIDumpAnalysis::ResourceView                         m_depth_stencil_view;
                std::map<uint32_t, std::shared_ptr<GlDisplayList> >             m_display_list_map;              /* by the number of the list; the contexts share them */
                uint32_t                                                        m_framebuffer_height;
                uint32_t                                                        m_framebuffer_width;
                bool                                                            m_has_default_framebuffer;
                uint32_t                                                        m_list_depth;                    /* of the lists that are being called */
                std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t> m_n_resolution_hazard_map;
                uint64_t                                                        m_n_unfollowed_calls;
                uint64_t                                                        m_n_unsupported_calls;
                GlReportedMatrix                                                m_reported_modelview;
                GlReportedMatrix                                                m_reported_projection;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H */
