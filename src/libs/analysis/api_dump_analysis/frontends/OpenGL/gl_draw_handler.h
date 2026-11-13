/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the front end of OpenGL does with the calls that draw or clear: the draws of immediate mode (a glBegin, the glVertex calls and the glEnd that makes them one
 * draw), the draws from arrays (glDrawArrays and glDrawElements, from the copy of the arrays that the dumper made) and the clears. Each draw is told to the model
 * builder with the state of the pipeline that was set, the textures that are bound and the call that begins its camera. The window that is rendered to is made when
 * the first of them comes, and the front end gives it, with the texture handler and the counter of the hazards, with the call: the handler keeps no address of the
 * front end.
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

                /* The data that the draws read from the memory of the application, kept for as long as it is worth keeping (see GLBlobStore). */
                const GLBlobStore& get_blob_store() const;

                /* How many of the calls that draw or clear are not followed, and the front end goes on without them. */
                uint64_t get_n_unfollowed_calls() const;

                /* A call of glDrawArrays or glDrawElements: a draw from the arrays that the dumper copied into it. */
                void handle_array_draw(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                       const Capture::APIDumpLoader::Workload&      in_workload,
                                       const bool&                                  in_is_indexed,
                                       GLWindow*                                    inout_window_ptr,
                                       GLTextureHandler*                            inout_texture_handler_ptr,
                                       GLContextState*                              inout_state_ptr);

                void handle_begin(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                  GLContextState*                              inout_state_ptr);

                /* With the scissor test on, a clear reaches the box and no more, unless the box is the whole framebuffer: the counter counts the one that does not. */
                void handle_clear(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                  GLWindow*                                    inout_window_ptr,
                                  ResolutionHazardCounter*                     inout_counter_ptr,
                                  GLContextState*                              inout_state_ptr);

                /* The vertices since the glBegin are a draw. */
                void handle_end(GLWindow*         inout_window_ptr,
                                GLTextureHandler* inout_texture_handler_ptr,
                                GLContextState*   inout_state_ptr);

                /* A glVertex of the block that is open: it is counted, and its position kept if the builder wants the geometry. */
                void handle_vertex(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const Capture::APIDumpLoader::Workload&      in_workload,
                                   GLContextState*                              inout_state_ptr);

                /* A frame is presented: the describer is told, so that it lets go of the data it kept for the frame. */
                void on_frame_ended();

            private:
                /* The call that begins the camera of the draw (see DrawDescription::view_anchor_call_index), from the model-view stack. */
                static void add_view_anchor(const GLContextState&                       in_state,
                                            Analysis::APIDumpAnalysis::DrawDescription* out_description_ptr);

                /* What the mode of a glBegin makes of the vertices; quads are two triangles and a loop of lines is a strip of them. */
                static Analysis::APIDumpAnalysis::PrimitiveTopology get_primitive_topology(const uint32_t& in_mode);

                GLArrayDrawDescriber                     m_array_draw_describer;
                Analysis::APIDumpAnalysis::ModelBuilder* m_builder_ptr;
                uint64_t                                 m_n_unfollowed_calls;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_DRAW_HANDLER_H */
