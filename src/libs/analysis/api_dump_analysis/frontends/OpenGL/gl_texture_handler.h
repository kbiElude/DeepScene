/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Handles the OpenGL texture calls: the textures of the program (shared by all contexts), their levels and contents, the units they are bound to,
 * and what the model builder and the texture sink are told about them (events for uploads, copies and driver-generated levels, and snapshots of
 * rendered content, see TextureEventKind). A call that changes a kind of texture that is not followed ends the process (see
 * is_unsupported_texture_function() and UnsupportedUse). Copies of the window to a texture are done by the front end, which knows the window.
 * It uses the functions of this class that create a level (specify_texture(), record_texture_level(), report_render_write()).
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

                /* OpenGL objects are numbers, not application addresses, so the address of a texture is the name the program uses for it. Returns false
                 * if there is no such texture or if it has no resource yet (it gets one once it is given a size).
                 */
                bool find_resource_view(const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const;

                /* Returns the texture with the given name; nullptr if the program has not created one. */
                const Analysis::APIDumpAnalysis::Frontends::GLTexture* find_texture_ptr(const uint32_t& in_name) const;

                /* A level of the bound texture changed. If the texture has GL_GENERATE_MIPMAP and the changed level is the first, the levels below it are generated. */
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

                /* A glPixelStorei call: the alignment and the row length of uploads are stored. The other values are not followed. */
                void handle_pixel_store(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        GLContextState*                              inout_state_ptr);

                /* A glTexImage2D call: the first level gives the texture its size (and its resource). A level may or may not carry pixels. */
                void handle_tex_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const Capture::APIDumpLoader::Workload&      in_workload,
                                      GLContextState*                              inout_state_ptr);

                /* glTexParameteri and glTexParameterf, and the forms that take an array. Only the three parameters that change what the texture holds are
                 * examined. GL_GENERATE_MIPMAP and GL_TEXTURE_MAX_LEVEL are stored. A base level other than the first is not followed.
                 */
                void handle_tex_parameter(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const GLContextState&                        in_state);

                /* A glTexSubImage2D call: writes a region of a level that the texture has. The call may or may not carry pixels. */
                void handle_tex_sub_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const Capture::APIDumpLoader::Workload&      in_workload,
                                          GLContextState*                              inout_state_ptr);

                /* Returns the level of the texture bound to the active unit, as the call that created it specified it. Returns false if the texture has no size yet. */
                bool record_texture_level(const GLContextState& in_state,
                                          const uint32_t&       in_level,
                                          const GLTextureLevel& in_texture_level);

                /* A region of a level of the bound texture now holds content made by rendering (a copy from the window). It is reported to the builder for the texture sink. */
                void report_render_write(const GLContextState& in_state,
                                         const uint32_t&       in_level,
                                         const uint32_t&       in_x_offset,
                                         const uint32_t&       in_y_offset,
                                         const uint32_t&       in_width,
                                         const uint32_t&       in_height);

                /* A draw is about to read the textures it samples. The levels that rendering changed since they were last read are reported as snapshots
                 * (see TextureEventKind::SNAPSHOT) and are then marked unchanged. A snapshot covers the smallest region that contains the rendered content
                 * (the whole level for levels that the driver generated), and never storage that was allocated but not filled.
                 */
                void report_snapshots(const GLContextState& in_state);

                /* Returns the resource of the texture bound to the active unit. It is created, or created again if the size changed. */
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
                /* Generates the levels of the bound texture below the first (down to one by one texel, or to its GL_TEXTURE_MAX_LEVEL), as the driver does
                 * for glGenerateMipmap. Each level is reported as an event, as a copy of the level above for the dependencies, and as content that the workload does not hold.
                 */
                void generate_mipmap_levels(const GLContextState& in_state);

                /* Builds the texture event of the call, except for the event kind: the destination, the region, and the level as it is after the call,
                 * in the terms of the project (the format is the named one, and the API value is stored beside it). The level is one that the texture has.
                 */
                Analysis::APIDumpTextures::TextureEvent make_texture_event(const GLTexture& in_texture,
                                                                           const uint32_t&  in_level,
                                                                           const uint32_t&  in_x_offset,
                                                                           const uint32_t&  in_y_offset,
                                                                           const uint32_t&  in_width,
                                                                           const uint32_t&  in_height) const;

                /* A region of a level of the bound texture receives the pixels of the call (the ninth argument), or none. It is reported to the builder
                 * for the texture sink, with the bytes, their layout (from the pixel store of the context), and their format and type.
                 */
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

                /* The pixels of a call that fills a level of the texture of the active unit, as the resource receives them: for the whole level, or for a rectangle of it. */
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
