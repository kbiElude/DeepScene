/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Handles the OpenGL matrix calls: the matrix stacks of each context (model-view, projection, and the others, which are only tracked to keep them
 * correct), the calls that load or multiply the current matrix, and the reports to the model builder. Only two matrices are reported, each when it
 * changes: MODEL_VIEW_MATRIX and PROJECTION_MATRIX. OpenGL has a single matrix for model and view together, so the model-view matrix is reported
 * as it is. A separate model matrix is never derived from it.
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

                /* The number of matrix calls that are not followed. The front end continues without them. */
                uint64_t get_n_unfollowed_calls() const;

                /* A call that loads or multiplies the current matrix. The matrix operand comes from the call arguments or from the data stored with the call. */
                void handle_matrix(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const Capture::APIDumpLoader::Workload&      in_workload,
                                   GLContextState*                              inout_state_ptr);

                void handle_matrix_mode(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        GLContextState*                              inout_state_ptr);
                void handle_pop_matrix (GLContextState*                              inout_state_ptr);
                void handle_push_matrix(GLContextState*                              inout_state_ptr);

                /* Reports the model-view and projection matrices of the context to the model builder, but only those that differ from the last report. */
                void report_matrices(const GLContextState& in_state);

            private:
                void report_matrix(const GLMatrix&                                    in_matrix_f32mat,
                                   const Analysis::APIDumpAnalysis::ConstantSemantic& in_semantic,
                                   const uint64_t&                                    in_byte_offset,
                                   GLReportedMatrix*                                  inout_reported_ptr);

                /* The matrix stack that glMatrixMode selected. */
                static std::vector<GLMatrix>* get_current_stack_ptr(GLContextState* inout_state_ptr);

                /* Reads the matrix that a glLoadMatrix or glMultMatrix call stored as its argument, as floats or as doubles. */
                static bool read_matrix_chunk(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                              const Capture::APIDumpLoader::Workload&      in_workload,
                                              const bool&                                  in_is_double,
                                              GLMatrix*                                    out_matrix_f32mat_ptr);

                /* Reads the matrix that the call loads into the current matrix or multiplies it by. Returns false if the arguments cannot be read. */
                static bool read_matrix_operand(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                const Capture::APIDumpLoader::Workload&      in_workload,
                                                GLMatrix*                                    out_matrix_f32mat_ptr);

                /* The value of the GL_MODELVIEW matrix mode. */
                static const uint32_t MODELVIEW_MODE;

                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
                uint64_t                                 m_n_unfollowed_calls;
                GLReportedMatrix                         m_reported_modelview;
                GLReportedMatrix                         m_reported_projection;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_MATRIX_HANDLER_H */
