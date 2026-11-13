/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the front end of OpenGL does with the calls about matrices: the stacks of each context (a model-view, a projection, and the others, which are only kept
 * right), what loads or multiplies the current matrix, and what the model builder is told of the two that it is told of (MODEL_VIEW_MATRIX and PROJECTION_MATRIX,
 * each when it changes). OpenGL has one matrix for the model and the view together, so the two are told as they are, and a model is not told from a view.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_MATRIX_HANDLER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_MATRIX_HANDLER_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_context_state.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include <stdint.h>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLMatrixHandler
            {
            public:
                /* The builder must outlive the handler. */
                explicit GLMatrixHandler(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* How many of the calls about matrices are not followed, and the front end goes on without them. */
                uint64_t get_n_unfollowed_calls() const;

                /* A call that loads or multiplies the current matrix: the matrix is what its arguments (or its data) say. */
                void handle_matrix(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const Capture::APIDumpLoader::Workload&      in_workload,
                                   GLContextState*                              inout_state_ptr);

                void handle_matrix_mode(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        GLContextState*                              inout_state_ptr);
                void handle_pop_matrix (GLContextState*                              inout_state_ptr);
                void handle_push_matrix(GLContextState*                              inout_state_ptr);

                /* Tells the model builder the model-view and the projection matrix of the context, those of them which are not what it was told last. */
                void report_matrices(const GLContextState& in_state);

            private:
                void report_matrix(const GLMatrix&                                    in_matrix_f32mat,
                                   const Analysis::APIDumpAnalysis::ConstantSemantic& in_semantic,
                                   const uint64_t&                                    in_byte_offset,
                                   GLReportedMatrix*                                  inout_reported_ptr);

                /* The stack of the matrices that glMatrixMode has chosen. */
                static std::vector<GLMatrix>* get_current_stack_ptr(GLContextState* inout_state_ptr);

                /* The matrix that a call of glLoadMatrix or glMultMatrix has stored as its argument, of floats or of doubles. */
                static bool read_matrix_chunk(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                              const Capture::APIDumpLoader::Workload&      in_workload,
                                              const bool&                                  in_is_double,
                                              GLMatrix*                                    out_matrix_f32mat_ptr);

                /* The matrix that the call has to load into the current one, or to multiply it by; false if its arguments cannot be read. */
                static bool read_matrix_operand(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                const Capture::APIDumpLoader::Workload&      in_workload,
                                                GLMatrix*                                    out_matrix_f32mat_ptr);

                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
                uint64_t                                 m_n_unfollowed_calls;
                GLReportedMatrix                         m_reported_modelview;
                GLReportedMatrix                         m_reported_projection;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_MATRIX_HANDLER_H */
