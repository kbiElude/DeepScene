/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Handles the OpenGL calls that draw or clear: the draws of immediate mode (a glBegin, the glVertex calls and the glEnd that ends the draw), the
 * draws from arrays (glDrawArrays and glDrawElements, using the copy of the arrays that the dumper made) and the clears. Each draw is reported to
 * the model builder together with the pipeline state, the bound textures and the call that begins its camera. The window that is rendered to is
 * created when the first draw or clear arrives. The front end passes the window, the texture handler and the hazard counter in with each call,
 * so the handler holds no pointer to the front end.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_DRAW_HANDLER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_DRAW_HANDLER_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_array_draw_describer.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_context_state.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_texture_handler.h"
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
            class GLDrawHandler
            {
            public:
                /* The builder must outlive the handler. */
                explicit GLDrawHandler(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* The data that draws read from application memory, kept only as long as it is worth keeping (see GLBlobStore). */
                const GLBlobStore& get_blob_store() const;

                /* The number of draw or clear calls that are not followed. The front end continues without them. */
                uint64_t get_n_unfollowed_calls() const;

                /* A glDrawArrays or glDrawElements call: a draw from the arrays that the dumper copied into the call. */
                void handle_array_draw(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                       const Capture::APIDumpLoader::Workload&      in_workload,
                                       const bool&                                  in_is_indexed,
                                       GLWindow*                                    inout_window_ptr,
                                       GLTextureHandler*                            inout_texture_handler_ptr,
                                       GLContextState*                              inout_state_ptr);

                void handle_begin(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                  GLContextState*                              inout_state_ptr);

                /* With the scissor test on, a clear affects only the scissor box, unless the box covers the whole framebuffer. The counter counts the clears that do not. */
                void handle_clear(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                  GLWindow*                                    inout_window_ptr,
                                  ResolutionHazardCounter*                     inout_counter_ptr,
                                  GLContextState*                              inout_state_ptr);

                /* The vertices since the glBegin form a draw. */
                void handle_end(GLWindow*         inout_window_ptr,
                                GLTextureHandler* inout_texture_handler_ptr,
                                GLContextState*   inout_state_ptr);

                /* A glVertex inside the open block. It is counted, and its position is kept if the builder wants the geometry. */
                void handle_vertex(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const Capture::APIDumpLoader::Workload&      in_workload,
                                   GLContextState*                              inout_state_ptr);

                /* A frame is presented. The describer is notified so that it releases the data it kept for the frame. */
                void on_frame_ended();

            private:
                /* Returns the call that begins the camera of the draw (see DrawDescription::view_anchor_call_index), taken from the model-view stack. */
                static void add_view_anchor(const GLContextState&                       in_state,
                                            Analysis::APIDumpAnalysis::DrawDescription* out_description_ptr);

                /* Maps the mode of a glBegin to the primitives built from the vertices: quads become two triangles and a line loop becomes a line strip. */
                static Analysis::APIDumpAnalysis::PrimitiveTopology get_primitive_topology(const uint32_t& in_mode);

                GLArrayDrawDescriber                     m_array_draw_describer;
                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
                uint64_t                                 m_n_unfollowed_calls;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_DRAW_HANDLER_H */
