/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the front end of OpenGL does with the calls that are about textures: the textures of the program (shared by the contexts), the levels of each and what
 * they hold, the units that they are bound to, and what the model builder and the sink of the textures are told of them (the events of the uploads, the copies
 * and the levels that the driver makes, and the snapshots of what rendering made, see TextureEventKind). A call about a texture that is of a kind that is not
 * followed ends the process (see is_unsupported_texture_function() and UnsupportedUse). The copies of the window to a texture are made by the front end, which
 * knows the window, with the parts of this class that make a level (specify_texture(), record_texture_level(), report_render_write()).
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

                /* The texture of the program that the draw samples with each unit, as bindings. */
                void add_texture_bindings(const GLContextState&                       in_state,
                                          Analysis::APIDumpAnalysis::DrawDescription* out_description_ptr) const;

                /* The objects of OpenGL are numbers, not addresses of the application: the address is the name of a texture, as the program has it. False if there is no
                 * such texture, or it has no resource yet (it has one once it is given a size).
                 */
                bool find_resource_view(const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const;

                /* The texture of the name; nullptr if the program has not made one. */
                const Analysis::APIDumpAnalysis::Frontends::GLTexture* find_texture_ptr(const uint32_t& in_name) const;

                /* A level of the bound texture has been changed: if the texture has GL_GENERATE_MIPMAP and it is the first level, the levels below it are made. */
                void generate_mipmaps_if_automatic(const GLContextState& in_state,
                                                   const uint32_t&       in_level);

                /* How many of the calls about textures are not followed, and the front end goes on without them. */
                uint64_t get_n_unfollowed_calls() const;

                /* A call of glActiveTexture: the unit that the calls of textures are for from now on. */
                void handle_active_texture(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                           GLContextState*                              inout_state_ptr);

                /* A call of glBindTexture: of a two dimensional texture, the one that the active unit has from now on. */
                void handle_bind_texture(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         GLContextState*                              inout_state_ptr);

                /* The textures of glDeleteTextures are gone, and their resources. */
                void handle_delete_textures(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            const Capture::APIDumpLoader::Workload&      in_workload);

                /* glGenerateMipmap of the target that is followed. */
                void handle_generate_mipmap(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            const GLContextState&                        in_state);

                /* glPixelStorei: the alignment and the length of the rows of what is uploaded are kept; the other values are not followed. */
                void handle_pixel_store(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                        GLContextState*                              inout_state_ptr);

                /* A call of glTexImage2D: the first level gives the texture its size (and resource), a level has pixels or not. */
                void handle_tex_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      const Capture::APIDumpLoader::Workload&      in_workload,
                                      GLContextState*                              inout_state_ptr);

                /* glTexParameteri and glTexParameterf (and the forms that take an array, of which the three values that change what the texture holds are the only
                 * ones looked at): GL_GENERATE_MIPMAP and GL_TEXTURE_MAX_LEVEL are kept, and a first level other than the first is not followed.
                 */
                void handle_tex_parameter(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const GLContextState&                        in_state);

                /* A call of glTexSubImage2D: a region of a level that the texture has, with pixels or not. */
                void handle_tex_sub_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                          const Capture::APIDumpLoader::Workload&      in_workload,
                                          GLContextState*                              inout_state_ptr);

                /* The level of the texture that the active unit has bound, as the call that made it said; false if the texture has no size yet. */
                bool record_texture_level(const GLContextState& in_state,
                                          const uint32_t&       in_level,
                                          const GLTextureLevel& in_texture_level);

                /* A region of a level of the bound texture is the content that rendering made (a copy from the window): told to the builder for the sink of the textures. */
                void report_render_write(const GLContextState& in_state,
                                         const uint32_t&       in_level,
                                         const uint32_t&       in_x_offset,
                                         const uint32_t&       in_y_offset,
                                         const uint32_t&       in_width,
                                         const uint32_t&       in_height);

                /* A draw is about to read the textures it samples: the levels that rendering has changed since they were last read are told as snapshots (see
                 * TextureEventKind::SNAPSHOT), and are no longer changed. A snapshot is of the smallest region that holds what rendering made of the level (all of
                 * it for the levels that the driver made), and not of storage that was given and not filled.
                 */
                void report_snapshots(const GLContextState& in_state);

                /* The resource of the texture that is bound to the active unit: made, or made again if the size is not what it was. */
                void specify_texture(const GLContextState& in_state,
                                     const uint32_t&       in_internal_format,
                                     const uint32_t&       in_width,
                                     const uint32_t&       in_height);

                /* The name of the texture that the active unit has bound (0 if it has none). */
                static uint32_t get_bound_texture_name(const GLContextState& in_state);

                /* What an image of the internal format holds: color, or depth (or depth and stencil). */
                static Analysis::APIDumpAnalysis::FormatClass get_format_class(const uint32_t& in_internal_format);

                /* A call that makes or changes a texture of a kind that is not followed (the process is ended, see UnsupportedUse). */
                static bool is_unsupported_texture_function(const APIInterceptor::APIFunction& in_function);

            private:
                /* Makes the levels of the bound texture below the first (down to a size of one by one, or to its GL_TEXTURE_MAX_LEVEL), as the driver does for
                 * glGenerateMipmap: each is told as an event, as a copy of the level above for the dependencies, and as content that the workload does not have.
                 */
                void generate_mipmap_levels(const GLContextState& in_state);

                /* The event of a texture that the call makes, but for what is of the kind of it: the destination, the region, and the level as it is after the call, in
                 * the words of the project (the format is the named one, and the API's value is kept beside it). The level is one that the texture has.
                 */
                Analysis::APIDumpTextures::TextureEvent make_texture_event(const GLTexture& in_texture,
                                                                           const uint32_t&  in_level,
                                                                           const uint32_t&  in_x_offset,
                                                                           const uint32_t&  in_y_offset,
                                                                           const uint32_t&  in_width,
                                                                           const uint32_t&  in_height) const;

                /* A region of a level of the bound texture is given the pixels of the call (the ninth argument), or none: told to the builder for the sink of the
                 * textures, with the bytes, the way they are laid out (the pixel store of the context), and the format and the type that they are of.
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

                /* The pixels of a call that gives a level of the texture of the active unit, as the resource gets them: to all of the level, or to the rectangle of it. */
                void upload_texture_level(const Capture::APIDumpLoader::DumpedAPICall&              in_call,
                                          const Capture::APIDumpLoader::Workload&                   in_workload,
                                          const GLContextState&                                     in_state,
                                          const uint32_t&                                           in_level,
                                          const Analysis::APIDumpAnalysis::AccessCoverage&          in_coverage,
                                          const Analysis::APIDumpAnalysis::AccessRectangle&         in_rectangle);

                Analysis::APIDumpAnalysis::ModelBuilder*        m_builder_ptr;
                uint64_t                                        m_n_unfollowed_calls;
                std::map<uint32_t, GLTexture>                   m_texture_map;           /* by the name of the texture; the contexts share them */
                std::map<uint32_t, GLTextureParameters>         m_texture_parameter_map; /* by the name of the texture, which need not have a size yet */
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_TEXTURE_HANDLER_H */
