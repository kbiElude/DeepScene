/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Handles the OpenGL texture calls: textures (shared by all contexts), levels, unit bindings, and the events and snapshots reported
 * to the model builder and texture sink (see TextureEventKind). Unfollowed texture kinds end the process (see UnsupportedUse).
 * Window-to-texture copies are done by the front end, which knows the window.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_TEXTURE_HANDLER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_TEXTURE_HANDLER_H

#include "analysis/api_dump_analysis/frontends/OpenGL/gl_context_state.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include <map>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GLTextureHandler
            {
            public:
                /* The builder must outlive the handler. */
                explicit GLTextureHandler(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* The texture of the program that the draw samples through each unit, returned as bindings. */
                void add_texture_bindings(const GLContextState&                       in_state,
                                          Analysis::APIDumpAnalysis::DrawDescription* out_description_ptr) const;

                /* A texture's address is its name. Returns false if it does not exist or has no resource yet (no size given). */
                bool find_resource_view(const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const;

                /* Returns the texture with the given name; nullptr if the program has not created one. */
                const Analysis::APIDumpAnalysis::Frontends::GLTexture* find_texture_ptr(const uint32_t& in_name) const;

                /* A level of the bound texture changed; with GL_GENERATE_MIPMAP and the first level changed, the lower levels are generated. */
                void generate_mipmaps_if_automatic(const GLContextState& in_state,
                                                   const uint32_t&       in_level);

                /* The number of texture calls that are not followed. The front end continues without them. */
                uint64_t get_n_unfollowed_calls() const;

                /* A glActiveTexture call: selects the unit that the following texture calls apply to. */
                void handle_active_texture(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           GLContextState*                              inout_state_ptr);

                /* A glBindTexture call: binds a two dimensional texture to the active unit. */
                void handle_bind_texture(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         GLContextState*                              inout_state_ptr);

                /* A glDeleteTextures call: the textures and their resources are deleted. */
                void handle_delete_textures(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            const Capture::APIDumpLoader::Workload&      in_workload);

                /* A glGenerateMipmap call for the target that is followed. */
                void handle_generate_mipmap(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            const GLContextState&                        in_state);

                /* A glPixelStorei call: alignment and row length are stored; other values are not followed. */
                void handle_pixel_store(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        GLContextState*                              inout_state_ptr);

                /* A glTexImage2D call: the first level gives the texture its size (and its resource). A level may or may not carry pixels. */
                void handle_tex_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const Capture::APIDumpLoader::Workload&      in_workload,
                                      GLContextState*                              inout_state_ptr);

                /* glTexParameter*: only the parameters that change the content are examined; GL_GENERATE_MIPMAP and GL_TEXTURE_MAX_LEVEL are stored. */
                void handle_tex_parameter(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const GLContextState&                        in_state);

                /* A glTexSubImage2D call: writes a region of a level that the texture has. The call may or may not carry pixels. */
                void handle_tex_sub_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const Capture::APIDumpLoader::Workload&      in_workload,
                                          GLContextState*                              inout_state_ptr);

                /* Returns the level of the active unit's texture as specified by its creating call; false if the texture has no size. */
                bool record_texture_level(const GLContextState& in_state,
                                          const uint32_t&       in_level,
                                          const GLTextureLevel& in_texture_level);

                /* A level region now holds rendered content (a window copy); reported to the builder for the texture sink. */
                void report_render_write(const GLContextState& in_state,
                                         const uint32_t&       in_level,
                                         const uint32_t&       in_x_offset,
                                         const uint32_t&       in_y_offset,
                                         const uint32_t&       in_width,
                                         const uint32_t&       in_height);

                /* A draw is about to sample its textures: levels rendered since the last read are reported as snapshots (TextureEventKind::SNAPSHOT)
                 * and marked unchanged. A snapshot covers the smallest rendered region (the whole level if driver-generated).
                 */
                void report_snapshots(const GLContextState& in_state);

                /* Returns the active unit's texture resource, created again if the size changed. */
                void specify_texture(const GLContextState& in_state,
                                     const uint32_t&       in_internal_format,
                                     const uint32_t&       in_width,
                                     const uint32_t&       in_height);

                /* Returns the name of the texture bound to the active unit (0 if none is bound). */
                static uint32_t get_bound_texture_name(const GLContextState& in_state);

                /* Returns what an image of the internal format holds: color, or depth (or depth and stencil). */
                static Analysis::APIDumpAnalysis::FormatClass get_format_class(const uint32_t& in_internal_format);

                /* A call that creates or changes a kind of texture that is not followed. The process ends (see UnsupportedUse). */
                static bool is_unsupported_texture_function(const APIInterceptor::APIFunction& in_function);

            private:
                /* Generates the levels below the first (down to 1x1 or GL_TEXTURE_MAX_LEVEL) like the driver; each is reported as an event and
                 * as a copy of the level above, with content the workload does not hold.
                 */
                void generate_mipmap_levels(const GLContextState& in_state);

                /* Builds the call's texture event except its kind: destination, region and the level after the call. The level must exist. */
                Analysis::APIDumpTextures::TextureEvent make_texture_event(const GLTexture& in_texture,
                                                                           const uint32_t&  in_level,
                                                                           const uint32_t&  in_x_offset,
                                                                           const uint32_t&  in_y_offset,
                                                                           const uint32_t&  in_width,
                                                                           const uint32_t&  in_height) const;

                /* A level region receives the call's pixels (ninth argument) or none; reported for the texture sink with bytes, layout and format. */
                void report_upload(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                   const Capture::APIDumpLoader::Workload&      in_workload,
                                   const GLContextState&                        in_state,
                                   const uint32_t&                              in_level,
                                   const uint32_t&                              in_x_offset,
                                   const uint32_t&                              in_y_offset,
                                   const uint32_t&                              in_width,
                                   const uint32_t&                              in_height,
                                   const uint32_t&                              in_format,
                                   const uint32_t&                              in_type);

                /* The pixels a call writes into the active unit's texture, as the resource receives them (whole level or rectangle). */
                void upload_texture_level(const Capture::APIDumpLoader::DumpedAPICall&              in_call,
                                          const Capture::APIDumpLoader::Workload&                   in_workload,
                                          const GLContextState&                                     in_state,
                                          const uint32_t&                                           in_level,
                                          const Analysis::APIDumpAnalysis::AccessCoverage&          in_coverage,
                                          const Analysis::APIDumpAnalysis::AccessRectangle&         in_rectangle);

                Analysis::APIDumpAnalysis::ModelBuilder*        m_builder_ptr;
                uint64_t                                        m_n_unfollowed_calls;
                std::map<uint32_t, GLTexture>                   m_texture_map;           /* by texture name; all contexts share them */
                std::map<uint32_t, GLTextureParameters>         m_texture_parameter_map; /* by texture name; the texture may not have a size yet */
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_TEXTURE_HANDLER_H */
