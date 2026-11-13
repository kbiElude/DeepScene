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
 * is_unsupported_texture_function()), and the state that decides what a draw writes (see GlFunctionPolicy::changes_what_a_draw_writes()).
 * A read of the framebuffer (glReadPixels) is only counted, as a hazard of pixel readbacks.
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
                                              GlListEyeDependence* out_dependence_ptr) const;

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
                uint64_t                                                               get_n_unfollowed_calls  () const override;
                uint64_t                                                               get_n_unsupported_calls () const override;
                void                                                                   on_call                 (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                                                const Capture::APIDumpLoader::Workload&      in_workload) override;

            private:
                /* The texture of the program that the draw samples with each unit, as bindings. */
                void add_texture_bindings(const GlContextState&                       in_state,
                                          Analysis::APIDumpAnalysis::DrawDescription* out_description_ptr) const;

                /* The call that begins the camera of the draw (see DrawDescription::view_anchor_call_index), from the model-view stack. */
                void add_view_anchor(const GlContextState&                       in_state,
                                     Analysis::APIDumpAnalysis::DrawDescription* out_description_ptr) const;

                /* The pixels of the window are copied to a region of a level of the texture that the active unit has bound (the depth of the window, for a texture of
                 * depth); false if the program has not given the texture a size, or the level.
                 */
                bool copy_to_bound_texture(const GlContextState& in_state,
                                           const uint32_t&       in_level,
                                           const uint32_t&       in_x_offset,
                                           const uint32_t&       in_y_offset,
                                           const uint32_t&       in_width,
                                           const uint32_t&       in_height);

                /* A call that depends on the size of what is rendered to in pixels. */
                void count_hazard(const Analysis::APIDumpAnalysis::ResolutionHazard& in_hazard);

                /* Makes the resources of the window, and tells the builder what is rendered to and where, if that has not been done. */
                void ensure_default_framebuffer(const GlContextState& in_state);

                /* Makes the levels of the bound texture below the first (down to a size of one by one, or to its GL_TEXTURE_MAX_LEVEL), as the driver does for
                 * glGenerateMipmap: each is told as an event, as a copy of the level above for the dependencies, and as content that the workload does not have.
                 */
                void generate_mipmap_levels(const GlContextState& in_state);

                /* A level of the bound texture has been changed: if the texture has GL_GENERATE_MIPMAP and it is the first level, the levels below it are made. */
                void generate_mipmaps_if_automatic(const GlContextState& in_state,
                                                   const uint32_t&       in_level);

                /* The state of the context that the thread has current; nullptr if it has none. */
                GlContextState* find_current_context_ptr(const uint32_t& in_thread_id);

                /* What a call that a list keeps does to what an eye has of its own: that of the list, for the call of one. */
                GlListEyeDependence get_call_eye_dependence(const APIInterceptor::APIFunction&                      in_function,
                                                            const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec) const;

                /* A call of glActiveTexture: the unit that the calls of textures are for from now on. */
                void handle_active_texture(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           GlContextState*                              inout_state_ptr);

                /* A call of glDrawArrays or glDrawElements: a draw from the arrays that the dumper copied into it. */
                void handle_array_draw(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                       const Capture::APIDumpLoader::Workload&      in_workload,
                                       const bool&                                  in_is_indexed,
                                       GlContextState*                              inout_state_ptr);

                void handle_begin(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                  GlContextState*                              inout_state_ptr);

                /* A call of glBindTexture: of a two dimensional texture, the one that the active unit has from now on. */
                /* glBindBuffer: a buffer that pixels are packed into (glReadPixels then reads to the buffer, and what is read is in no call) or unpacked from (the
                 * pixels of an upload are then in the buffer, not in the call) is not followed: the process is ended. So is any other buffer object (of vertices, of
                 * indices, of constants): buffers are not followed. Unbinding one (the buffer 0) is not that.
                 */
                void handle_bind_buffer(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                void handle_bind_texture(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         GlContextState*                              inout_state_ptr);

                /* A call of glCallList: the calls of the list are made, as the list has them. */
                void handle_call_list(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                void handle_clear        (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GlContextState*                              inout_state_ptr);
                void handle_clear_color  (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GlContextState*                              inout_state_ptr);
                void handle_clear_depth  (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GlContextState*                              inout_state_ptr);
                void handle_clear_stencil(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          GlContextState*                              inout_state_ptr);

                /* A call of glCopyTexImage2D: the pixels of the window give the texture its size, and are copied to it. */
                void handle_copy_tex_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           GlContextState*                              inout_state_ptr);

                /* A call of glCopyTexSubImage2D: the pixels of the window are copied to a part of the texture. */
                void handle_copy_tex_sub_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                               GlContextState*                              inout_state_ptr);

                /* The lists of glDeleteLists are gone. */
                void handle_delete_lists(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                /* The textures of glDeleteTextures are gone, and their resources. */
                void handle_delete_textures(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            const Capture::APIDumpLoader::Workload&      in_workload);

                void handle_enable(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const bool&                                  in_is_enabled,
                                   GlContextState*                              inout_state_ptr);

                /* The vertices since the glBegin are a draw. */
                void handle_end(GlContextState* inout_state_ptr);

                /* A list which was being made is made, and its calls are made when it is called. */
                void handle_end_list(GlContextState* inout_state_ptr);

                /* glGenerateMipmap of the target that is followed. */
                void handle_generate_mipmap(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            const GlContextState&                        in_state);

                /* A call of GL: kept in the list that its context is making, or made, or both. It is also how the calls of a called list are made. */
                void handle_gl_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                    const Capture::APIDumpLoader::Workload&      in_workload);

                void handle_make_current(const Capture::APIDumpLoader::DumpedAPICall& in_call);

                /* A call that loads or multiplies the current matrix: the matrix is what its arguments (or its data) say. */
                void handle_matrix(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const Capture::APIDumpLoader::Workload&      in_workload,
                                   GlContextState*                              inout_state_ptr);

                void handle_matrix_mode    (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            GlContextState*                              inout_state_ptr);
                void handle_new_list       (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            GlContextState*                              inout_state_ptr);
                /* glPixelStorei: the alignment and the length of the rows of what is uploaded are kept; the other values are not followed. */
                void handle_pixel_store(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        GlContextState*                              inout_state_ptr);

                void handle_pop_attributes (GlContextState*                              inout_state_ptr);
                void handle_pop_matrix     (GlContextState*                              inout_state_ptr);
                void handle_push_attributes(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            GlContextState*                              inout_state_ptr);
                void handle_push_matrix    (GlContextState*                              inout_state_ptr);
                void handle_scissor        (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            GlContextState*                              inout_state_ptr);

                /* A call which sets a value of the pipeline that a draw is told apart by (a function, a mode, a mask). */
                void handle_state_value(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        GlContextState*                              inout_state_ptr);

                /* A call of glTexImage2D: the first level gives the texture its size (and resource), a level has pixels or not. */
                void handle_tex_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const Capture::APIDumpLoader::Workload&      in_workload,
                                      GlContextState*                              inout_state_ptr);

                /* glTexParameteri and glTexParameterf (and the forms that take an array, of which the three values that change what the texture holds are the only
                 * ones looked at): GL_GENERATE_MIPMAP and GL_TEXTURE_MAX_LEVEL are kept, and a first level other than the first is not followed.
                 */
                void handle_tex_parameter(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const GlContextState&                        in_state);

                /* A call of glTexSubImage2D: a region of a level that the texture has, with pixels or not. */
                void handle_tex_sub_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const Capture::APIDumpLoader::Workload&      in_workload,
                                          GlContextState*                              inout_state_ptr);

                /* A glVertex of the block that is open: it is counted, and its position kept if the builder wants the geometry. */
                void handle_vertex(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const Capture::APIDumpLoader::Workload&      in_workload,
                                   GlContextState*                              inout_state_ptr);

                void handle_viewport(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     GlContextState*                              inout_state_ptr);

                /* The event of a texture that the call makes, but for what is of the kind of it: the destination, the region, and the level as it is after the call, in
                 * the words of the project (the format is the named one, and the API's value is kept beside it). The level is one that the texture has.
                 */
                Analysis::APIDumpTextures::TextureEvent make_texture_event(const GlTexture& in_texture,
                                                                           const uint32_t&  in_level,
                                                                           const uint32_t&  in_x_offset,
                                                                           const uint32_t&  in_y_offset,
                                                                           const uint32_t&  in_width,
                                                                           const uint32_t&  in_height) const;

                /* The level of the texture that the active unit has bound, as the call that made it said; false if the texture has no size yet. */
                bool record_texture_level(const GlContextState& in_state,
                                          const uint32_t&       in_level,
                                          const GlTextureLevel& in_texture_level);

                /* Keeps the call in the list that the context is making, with the data that it refers to. */
                void record_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                 const Capture::APIDumpLoader::Workload&      in_workload,
                                 GlContextState*                              inout_state_ptr);

                /* Tells the model builder the model-view and the projection matrix of the context, those of them which are not what it was told last. */
                void report_matrices(const GlContextState& in_state);

                void report_matrix(const GlMatrix&                                    in_matrix_f32mat,
                                   const Analysis::APIDumpAnalysis::ConstantSemantic& in_semantic,
                                   const uint64_t&                                    in_byte_offset,
                                   GlReportedMatrix*                                  inout_reported_ptr);

                /* A region of a level of the bound texture is the content that rendering made (a copy from the window): told to the builder for the sink of the textures. */
                void report_render_write(const GlContextState& in_state,
                                         const uint32_t&       in_level,
                                         const uint32_t&       in_x_offset,
                                         const uint32_t&       in_y_offset,
                                         const uint32_t&       in_width,
                                         const uint32_t&       in_height);

                /* A draw is about to read the textures it samples: the levels that rendering has changed since they were last read are told as snapshots (see
                 * TextureEventKind::SNAPSHOT), and are no longer changed. A snapshot is of the smallest region that holds what rendering made of the level (all of
                 * it for the levels that the driver made), and not of storage that was given and not filled.
                 */
                void report_snapshots(const GlContextState& in_state);

                /* What the program does is not followed (a target, a kind of texture, a buffer that pixels go to or come from, a call): what the analysis finds of it
                 * would be wrong or left out, and so the process is ended, saying what it was (AI_ASSERT_ALWAYS). Nothing is left out without being said.
                 */
                static void report_unsupported_use(const APIInterceptor::APIFunction& in_function,
                                                   const char*                        in_reason_ptr,
                                                   const uint64_t&                    in_value);

                /* A region of a level of the bound texture is given the pixels of the call (the ninth argument), or none: told to the builder for the sink of the
                 * textures, with the bytes, the way they are laid out (the pixel store of the context), and the format and the type that they are of.
                 */
                void report_upload(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const Capture::APIDumpLoader::Workload&      in_workload,
                                   const GlContextState&                        in_state,
                                   const uint32_t&                              in_level,
                                   const uint32_t&                              in_x_offset,
                                   const uint32_t&                              in_y_offset,
                                   const uint32_t&                              in_width,
                                   const uint32_t&                              in_height,
                                   const uint32_t&                              in_format,
                                   const uint32_t&                              in_type);

                /* The resource of the texture that is bound to the active unit: made, or made again if the size is not what it was. */
                void specify_texture(const GlContextState& in_state,
                                     const uint32_t&       in_internal_format,
                                     const uint32_t&       in_width,
                                     const uint32_t&       in_height);

                /* The pixels of a call that gives a level of the texture of the active unit, as the resource gets them: to all of the level, or to the rectangle of it. */
                void upload_texture_level(const Capture::APIDumpLoader::DumpedAPICall&              in_call,
                                          const Capture::APIDumpLoader::Workload&                   in_workload,
                                          const GlContextState&                                     in_state,
                                          const uint32_t&                                           in_level,
                                          const Analysis::APIDumpAnalysis::AccessCoverage&          in_coverage,
                                          const Analysis::APIDumpAnalysis::AccessRectangle&         in_rectangle);

                /* The name of the texture that the active unit has bound (0 if it has none). */
                static uint32_t get_bound_texture_name(const GlContextState& in_state);

                /* The stack of the matrices that glMatrixMode has chosen. */
                static std::vector<GlMatrix>* get_current_stack_ptr(GlContextState* inout_state_ptr);

                /* What an image of the internal format holds: color, or depth (or depth and stencil). */
                static Analysis::APIDumpAnalysis::FormatClass get_format_class(const uint32_t& in_internal_format);

                /* A context as it starts: what the specification says, and a stack of the identity for each kind of matrix. */
                static GlContextState get_initial_context_state();

                /* The viewport of the model, from the box of the context. */
                static Analysis::APIDumpAnalysis::Viewport get_model_viewport(const GlContextState& in_state);

                /* What the mode of a glBegin makes of the vertices; quads are two triangles and a loop of lines is a strip of them. */
                static Analysis::APIDumpAnalysis::PrimitiveTopology get_primitive_topology(const uint32_t& in_mode);

                /* True if the two boxes are the same. */
                static bool is_same_box(const GlBox& in_first,
                                        const GlBox& in_second);

                /* True for the calls whose effects the front end does not follow, and that are not about textures (they write the framebuffer from somewhere else, or
                 * call lists that are named by an array): one of them ends the process (see report_unsupported_use()).
                 */
                static bool is_unsupported_function(const APIInterceptor::APIFunction& in_function);

                /* A call that makes or changes a texture of a kind that is not followed (see report_unsupported_use()). */
                static bool is_unsupported_texture_function(const APIInterceptor::APIFunction& in_function);

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
                GlContextState*                                                 m_cached_context_state_ptr;      /* what find_current_context_ptr() found last, for m_cached_thread_id; nullptr: nothing */
                uint32_t                                                        m_cached_thread_id;
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
                std::map<uint32_t, GlTexture>                                   m_texture_map;                   /* by the name of the texture; the contexts share them */
                std::map<uint32_t, GlTextureParameters>                         m_texture_parameter_map;         /* by the name of the texture, which need not have a size yet */
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_FRONTEND_H */
