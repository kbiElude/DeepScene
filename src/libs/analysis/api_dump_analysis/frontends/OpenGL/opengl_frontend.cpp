/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/opengl_frontend.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_argument_reader.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_matrix_math.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_geometry_sampler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_list_classifier.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_function_policy.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_texture_handler.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_vertex_reader.h"
#include "analysis/api_dump_analysis/frontends/shared/call_view.h"
#include "analysis/api_dump_analysis/frontends/shared/resolution_hazard_counter.h"
#include "analysis/api_dump_analysis/frontends/shared/unsupported_use.h"
#include "analysis/api_dump_index/gl_function_classifier.h"
#include "capture/api_dump_loader/api_info.h"
#include "capture/api_dump_names/function_names.h"
#include "common/macros.h"
#include <stdio.h>
#include <string.h>

Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::OpenGLFrontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_builder_ptr             (in_builder_ptr),
     m_cached_context_state_ptr(nullptr),
     m_cached_thread_id        (0),
     m_draw_handler            (in_builder_ptr),
     m_matrix_handler          (in_builder_ptr),
     m_n_unfollowed_calls      (0),
     m_n_unsupported_calls     (0),
     m_state_handler           (in_builder_ptr),
     m_texture_handler         (in_builder_ptr),
     m_window                  (in_builder_ptr)
{
    /* Stub */
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::copy_to_bound_texture(const GLContextState& in_state,
                                                                                 const uint32_t&       in_level,
                                                                                 const uint32_t&       in_x_offset,
                                                                                 const uint32_t&       in_y_offset,
                                                                                 const uint32_t&       in_width,
                                                                                 const uint32_t&       in_height)
{
    const uint32_t name        = GLTextureHandler::get_bound_texture_name(in_state);
    const auto*    texture_ptr = m_texture_handler.find_texture_ptr(name);
    const bool     result      = (texture_ptr != nullptr);

    if (result)
    {
        m_window.ensure_default_framebuffer(in_state);

        /* The copy reads the color of the window, or its depth if the texture has a depth format. */
        const auto format_class = GLTextureHandler::get_format_class(texture_ptr->internal_format);
        const auto source       = (format_class == Analysis::APIDumpAnalysis::FormatClass::COLOR) ? m_window.get_color_view()
                                                                                                   : m_window.get_depth_stencil_view();

        Analysis::APIDumpAnalysis::AccessRectangle destination_rectangle;
        Analysis::APIDumpAnalysis::ResourceView    destination;

        destination.mip_level   = in_level;
        destination.resource_id = texture_ptr->resource_id;

        /* The rectangle of the level that the copy writes. The dependency analysis uses it to find frames that are not needed: a texture that the
         * frames fill one strip at a time is complete again after a few of them.
         */
        destination_rectangle.height   = in_height;
        destination_rectangle.is_known = true;
        destination_rectangle.width    = in_width;
        destination_rectangle.x        = in_x_offset;
        destination_rectangle.y        = in_y_offset;

        m_builder_ptr->on_copy(source,
                               destination,
                               destination_rectangle);

        m_texture_handler.report_render_write(in_state,
                                              in_level,
                                              in_x_offset,
                                              in_y_offset,
                                              in_width,
                                              in_height);

        m_texture_handler.generate_mipmaps_if_automatic(in_state,
                                                        in_level);

        m_resolution_hazard_counter.count(Analysis::APIDumpAnalysis::ResolutionHazard::COPIES_WITH_RECTANGLES);
    }

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GLContextState* Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::find_current_context_ptr(const uint32_t& in_thread_id)
{
    GLContextState* result_ptr = nullptr;

    /* A thread nearly always makes the call that follows its previous call in the same context. A state is never removed from the map, so a cached
     * state stays valid until handle_make_current() runs.
     */
    if (m_cached_context_state_ptr != nullptr &&
        m_cached_thread_id         == in_thread_id)
    {
        result_ptr = m_cached_context_state_ptr;
    }
    else
    {
        const auto thread_iterator = m_current_context_by_thread_map.find(in_thread_id);
        const auto thread_end      = m_current_context_by_thread_map.end();

        if (thread_iterator != thread_end)
        {
            result_ptr = &m_context_map.at(thread_iterator->second);

            m_cached_context_state_ptr = result_ptr;
            m_cached_thread_id         = in_thread_id;
        }
    }

    return result_ptr;
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::add_block_vertices(const uint32_t& in_thread_id,
                                                                              const uint32_t& in_n_vertices)
{
    GLContextState* state_ptr = find_current_context_ptr(in_thread_id);

    if (state_ptr != nullptr)
    {
        m_draw_handler.add_block_vertices(in_n_vertices,
                                          state_ptr);
    }
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::find_list_eye_dependence(const uint32_t&      in_list,
                                                                                    GLListEyeDependence* out_dependence_ptr) const
{
    return m_list_handler.find_list_eye_dependence(in_list,
                                                   out_dependence_ptr);
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::find_resource_view(const uint64_t&                          in_address,
                                                                              Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const
{
    return m_texture_handler.find_resource_view(in_address,
                                                out_view_ptr);
}

const Analysis::APIDumpAnalysis::Frontends::GLBlobStore& Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_blob_store() const
{
    return m_draw_handler.get_blob_store();
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_current_matrices(const uint32_t& in_thread_id,
                                                                                GLMatrix*       out_modelview_f32mat_ptr,
                                                                                GLMatrix*       out_projection_f32mat_ptr,
                                                                                uint32_t*       out_matrix_mode_ptr) const
{
    const auto thread_iterator = m_current_context_by_thread_map.find(in_thread_id);
    const auto thread_end      = m_current_context_by_thread_map.end();
    bool       result          = false;

    if (thread_iterator != thread_end)
    {
        const GLContextState& state = m_context_map.at(thread_iterator->second);

        *out_modelview_f32mat_ptr  = state.modelview_f32mat_vec.back();
        *out_projection_f32mat_ptr = state.projection_f32mat_vec.back();
        *out_matrix_mode_ptr       = state.attributes.matrix_mode;

        result = true;
    }

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GLContextState Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_initial_context_state()
{
    const GLMatrix identity = GLMatrixMath::get_identity();
    GLContextState result;

    result.modelview_anchor_call_index_vec.assign(1,
                                                  UINT64_MAX);
    result.modelview_f32mat_vec.assign (1,
                                        identity);
    result.other_f32mat_vec.assign     (1,
                                        identity);
    result.projection_f32mat_vec.assign(1,
                                        identity);

    return result;
}

const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_n_resolution_hazards() const
{
    return m_resolution_hazard_counter.get_counts();
}

uint64_t Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_n_unfollowed_calls() const
{
    return (m_n_unfollowed_calls                       +
            m_draw_handler.get_n_unfollowed_calls()    +
            m_list_handler.get_n_unfollowed_calls()    +
            m_matrix_handler.get_n_unfollowed_calls()  +
            m_state_handler.get_n_unfollowed_calls()   +
            m_texture_handler.get_n_unfollowed_calls() );
}

uint64_t Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_n_unsupported_calls() const
{
    return m_n_unsupported_calls;
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_bind_buffer(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    constexpr uint32_t PIXEL_PACK_BUFFER   = 0x88EB;
    constexpr uint32_t PIXEL_UNPACK_BUFFER = 0x88EC;

    std::array<uint32_t, 2> bind_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                         &bind_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (bind_array.at(0) == PIXEL_PACK_BUFFER &&
        bind_array.at(1) != 0)
    {
        /* glReadPixels then writes to the buffer instead of the memory of the program. The bytes are in no call of the workload, and the program
         * may create a texture from the buffer.
         */
        report_unsupported_use(in_call.func,
                               "pixels are packed into a buffer, so that what is read back never reaches the memory of the program and is in no call (the target)",
                               bind_array.at(0) );
    }
    else
    if (bind_array.at(0) == PIXEL_UNPACK_BUFFER &&
        bind_array.at(1) != 0)
    {
        /* From now on the pixels of an upload are in the buffer instead of the call, so the bytes of the upload would be missing. */
        report_unsupported_use(in_call.func,
                               "pixels are unpacked from a buffer, so that the pixels of an upload are not in the call (the target)",
                               bind_array.at(0) );
    }
    else
    if (bind_array.at(1) != 0)
    {
        /* The calls that fill a buffer (vertices, indices, constants) are not followed. A draw that reads from the buffer has no snapshot of the
         * data, so the geometry of the draw would be missing.
         */
        report_unsupported_use(in_call.func,
                               "a buffer object is bound, and buffer objects are not followed, so that what a draw reads from one is in no snapshot (the target)",
                               bind_array.at(0) );
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_copy_tex_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                 GLContextState*                              inout_state_ptr)
{
    /* glCopyTexImage2D(target, level, internalformat, x, y, width, height, border). */
    constexpr uint32_t TEXTURE_2D = 0x0DE1;

    std::array<int32_t, 7> image_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                         &image_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (static_cast<uint32_t>(image_array.at(0) ) != TEXTURE_2D)
    {
        report_unsupported_use(in_call.func,
                               "pixels of the window are copied to a level of a texture of a target other than GL_TEXTURE_2D (the target)",
                               static_cast<uint32_t>(image_array.at(0) ) );
    }
    else
    {
        const uint32_t level = static_cast<uint32_t>(image_array.at(1) );

        GLTextureLevel texture_level;

        texture_level.height          = static_cast<uint32_t>(image_array.at(6) );
        texture_level.internal_format = static_cast<uint32_t>(image_array.at(2) );
        texture_level.width           = static_cast<uint32_t>(image_array.at(5) );

        /* The copy gives the first level of the texture its size, as glTexImage2D does. */
        if (level == 0)
        {
            m_texture_handler.specify_texture(*inout_state_ptr,
                                              texture_level.internal_format,
                                              texture_level.width,
                                              texture_level.height);
        }

        m_texture_handler.record_texture_level(*inout_state_ptr,
                                               level,
                                               texture_level);

        if (copy_to_bound_texture(*inout_state_ptr,
                                  level,
                                  0,
                                  0,
                                  texture_level.width,
                                  texture_level.height) == false)
        {
            m_n_unfollowed_calls++;
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_copy_tex_sub_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                     GLContextState*                              inout_state_ptr)
{
    /* glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height). */
    constexpr uint32_t TEXTURE_2D = 0x0DE1;

    std::array<uint32_t, 8> image_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                         &image_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (image_array.at(0) != TEXTURE_2D)
    {
        report_unsupported_use(in_call.func,
                               "pixels of the window are copied to a part of a level of a texture of a target other than GL_TEXTURE_2D (the target)",
                               image_array.at(0) );
    }
    else
    if (copy_to_bound_texture(*inout_state_ptr,
                              image_array.at(1),
                              image_array.at(2),
                              image_array.at(3),
                              image_array.at(6),
                              image_array.at(7) ) == false)
    {
        /* The part of the texture has no size yet. */
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_gl_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                          const Capture::APIDumpLoader::Workload&      in_workload)
{
    GLContextState* state_ptr = find_current_context_ptr(in_call.thread_id);

    /* A call from a thread that has no current context has no effect. */
    if (state_ptr == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        /* While a display list is being made, its calls are stored in the list and run when the list is called. With GL_COMPILE_AND_EXECUTE they also run at once. */
        const bool is_recording = (state_ptr->list_recording.list_ptr != nullptr                  &&
                                   in_call.func                       != APIInterceptor::APIFUNCTION_GL_GLENDLIST);
        const bool is_made      = (is_recording                       == false                    ||
                                   state_ptr->list_recording.is_executing);

        if (is_recording)
        {
            m_list_handler.record_call(in_call,
                                       in_workload,
                                       state_ptr);
        }

        if (is_made)
        {
            switch (in_call.func)
            {
                case APIInterceptor::APIFUNCTION_GL_GLALPHAFUNC:
                case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
                case APIInterceptor::APIFUNCTION_GL_GLCOLORMASK:
                case APIInterceptor::APIFUNCTION_GL_GLCULLFACE:
                case APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC:
                case APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK:
                case APIInterceptor::APIFUNCTION_GL_GLFRONTFACE:
                case APIInterceptor::APIFUNCTION_GL_GLPOLYGONMODE:
                case APIInterceptor::APIFUNCTION_GL_GLSHADEMODEL:
                case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
                case APIInterceptor::APIFUNCTION_GL_GLSTENCILMASK:
                case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
                {
                    m_state_handler.handle_state_value(in_call,
                                                       state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS:
                case APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS:
                {
                    const bool is_indexed = (in_call.func == APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS);

                    m_draw_handler.handle_array_draw(in_call,
                                                     in_workload,
                                                     is_indexed,
                                                     &m_window,
                                                     &m_texture_handler,
                                                     state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE:
                {
                    m_texture_handler.handle_active_texture(in_call,
                                                            state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLBEGIN:
                {
                    m_draw_handler.handle_begin(in_call,
                                                state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
                {
                    m_texture_handler.handle_bind_texture(in_call,
                                                          state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
                {
                    const auto call_func = [this](const Capture::APIDumpLoader::DumpedAPICall& in_list_call,
                                                  const Capture::APIDumpLoader::Workload&      in_list_workload)
                    {
                        handle_gl_call(in_list_call,
                                       in_list_workload);
                    };

                    m_list_handler.handle_call_list(in_call,
                                                    call_func);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCLEAR:
                {
                    m_draw_handler.handle_clear(in_call,
                                                &m_window,
                                                &m_resolution_hazard_counter,
                                                state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCLEARCOLOR:
                {
                    m_state_handler.handle_clear_color(in_call,
                                                       state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCLEARDEPTH:
                {
                    m_state_handler.handle_clear_depth(in_call,
                                                       state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCLEARSTENCIL:
                {
                    m_state_handler.handle_clear_stencil(in_call,
                                                         state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLDISABLE:
                {
                    m_state_handler.handle_enable(in_call,
                                                  false, /* in_is_enabled */
                                                  state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLENABLE:
                {
                    m_state_handler.handle_enable(in_call,
                                                  true, /* in_is_enabled */
                                                  state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE2D:
                {
                    handle_copy_tex_image(in_call,
                                          state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D:
                {
                    handle_copy_tex_sub_image(in_call,
                                              state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLDELETELISTS:
                {
                    m_list_handler.handle_delete_lists(in_call);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES:
                {
                    m_texture_handler.handle_delete_textures(in_call,
                                                             in_workload);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLEND:
                {
                    m_draw_handler.handle_end(&m_window,
                                           &m_texture_handler,
                                           state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLENDLIST:
                {
                    m_list_handler.handle_end_list(state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLFRUSTUM:
                case APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY:
                case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD:
                case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF:
                case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXD:
                case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF:
                case APIInterceptor::APIFUNCTION_GL_GLORTHO:
                case APIInterceptor::APIFUNCTION_GL_GLROTATED:
                case APIInterceptor::APIFUNCTION_GL_GLROTATEF:
                case APIInterceptor::APIFUNCTION_GL_GLSCALED:
                case APIInterceptor::APIFUNCTION_GL_GLSCALEF:
                case APIInterceptor::APIFUNCTION_GL_GLTRANSLATED:
                case APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF:
                {
                    m_matrix_handler.handle_matrix(in_call,
                                                   in_workload,
                                                   state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE:
                {
                    m_matrix_handler.handle_matrix_mode(in_call,
                                                        state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPIXELSTOREF:
                case APIInterceptor::APIFUNCTION_GL_GLPIXELSTOREI:
                {
                    m_texture_handler.handle_pixel_store(in_call,
                                                         state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLLINEWIDTH:
                {
                    m_resolution_hazard_counter.count(Analysis::APIDumpAnalysis::ResolutionHazard::LINE_WIDTH_STATES);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
                {
                    m_list_handler.handle_new_list(in_call,
                                                   state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPOINTSIZE:
                {
                    m_resolution_hazard_counter.count(Analysis::APIDumpAnalysis::ResolutionHazard::POINT_SIZE_STATES);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB:
                {
                    m_state_handler.handle_pop_attributes(m_window,
                                                      state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPOPCLIENTATTRIB:
                {
                    m_state_handler.handle_pop_client_attributes(state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX:
                {
                    m_matrix_handler.handle_pop_matrix(state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB:
                {
                    m_state_handler.handle_push_attributes(in_call,
                                                           state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPUSHCLIENTATTRIB:
                {
                    m_state_handler.handle_push_client_attributes(in_call,
                                                                  state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX:
                {
                    m_matrix_handler.handle_push_matrix(state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLREADPIXELS:
                {
                    /* The model does not follow what the program does with read-back pixels. If the program turns them into a texture, the bytes appear
                     * in the call that uploads them. Reading into a buffer is impossible here, because binding a buffer ends the process.
                     */
                    m_resolution_hazard_counter.count(Analysis::APIDumpAnalysis::ResolutionHazard::PIXEL_READBACKS);

                    m_n_unsupported_calls++;

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLSCISSOR:
                {
                    m_state_handler.handle_scissor(in_call,
                                                   &m_resolution_hazard_counter,
                                                   state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D:
                {
                    m_texture_handler.handle_tex_image(in_call,
                                                       in_workload,
                                                       state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERF:
                case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERFV:
                case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI:
                case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERIV:
                {
                    m_texture_handler.handle_tex_parameter(in_call,
                                                           *state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE2D:
                {
                    m_texture_handler.handle_tex_sub_image(in_call,
                                                           in_workload,
                                                           state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLBINDBUFFER:
                {
                    handle_bind_buffer(in_call);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLGENERATEMIPMAP:
                {
                    m_texture_handler.handle_generate_mipmap(in_call,
                                                             *state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLVIEWPORT:
                {
                    m_state_handler.handle_viewport(in_call,
                                                    m_window,
                                                    state_ptr);

                    break;
                }

                default:
                {
                    GLVertexLayout vertex_layout;

                    const bool is_vertex = GLVertexReader::find_layout(in_call.func,
                                                                       &vertex_layout);

                    if (is_vertex)
                    {
                        m_draw_handler.handle_vertex(in_call,
                                                     in_workload,
                                                     state_ptr);
                    }
                    else
                    {
                        report_unfollowed_function(in_call.func);
                    }

                    break;
                }
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_make_current(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    constexpr size_t N_ARGUMENTS      = 2;
    constexpr size_t CONTEXT_ARGUMENT = 1;

    bool    has_result      = false;
    int32_t recorded_result = 0;

    if (in_call.returned_value_ptr != nullptr)
    {
        has_result = APIInterceptor::read_number(*in_call.returned_value_ptr,
                                                 &recorded_result);
    }

    if (in_call.arg_in_vec.size() < N_ARGUMENTS ||
        has_result                == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (recorded_result != 0)
    {
        /* A failed call changed nothing (for example, the first unbind of a program with no current context fails). */
        const uint64_t context_address = in_call.arg_in_vec.at(CONTEXT_ARGUMENT).get_ptr_as_u64();

        /* The current context of a thread changes only here, so the context state cached by find_current_context_ptr() is stale. */
        m_cached_context_state_ptr = nullptr;

        if (context_address == 0)
        {
            m_current_context_by_thread_map.erase(in_call.thread_id);
        }
        else
        {
            const auto emplace_result = m_context_map.emplace(context_address,
                                                              get_initial_context_state() );

            /* The viewport and the scissor box of a context start as the size of the window that it is first made current to; a program
             * need not call glViewport.
             */
            if (emplace_result.second)
            {
                GLBox    box;
                uint32_t height = 0;
                uint32_t width  = 0;

                if (m_window_extents.get_client_size(in_call.arg_in_vec.at(0).get_ptr_as_u64(),
                                                    &width,
                                                    &height) )
                {
                    box.height = static_cast<int32_t>(height);
                    box.width  = static_cast<int32_t>(width);

                    emplace_result.first->second.attributes.scissor_box = box;
                    emplace_result.first->second.attributes.viewport    = box;
                }
            }

            m_current_context_by_thread_map[in_call.thread_id] = context_address;

            /* The model builder still holds the matrices of the context that was current before, so it is told the matrices of the new one. */
            m_matrix_handler.report_matrices(m_context_map.at(context_address) );
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                   const Capture::APIDumpLoader::Workload&      in_workload)
{
    const auto group = Capture::APIDumpLoader::get_api_group(in_call.func);

    if (group == Capture::APIDumpLoader::APIGroup::GL)
    {
        handle_gl_call(in_call,
                       in_workload);
    }
    else
    if (in_call.func == APIInterceptor::APIFUNCTION_WGL_WGLMAKECURRENT)
    {
        handle_make_current(in_call);
    }
    else
    if (GLWindowExtents::follows_function(in_call.func) )
    {
        m_window_extents.on_call(in_call);
    }

    /* SwapBuffers belongs to GDI32 and ends a frame of an OpenGL application. */
    const bool is_frame_boundary = Capture::APIDumpLoader::is_frame_boundary(in_call);

    if (is_frame_boundary)
    {
        m_draw_handler.on_frame_ended();

        m_builder_ptr->on_frame_presented();
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::report_unfollowed_function(const APIInterceptor::APIFunction& in_function)
{
    const Analysis::APIDumpIndex::GLFunctionInfo function_info = Analysis::APIDumpIndex::GLFunctionClassifier::classify(in_function);

    if (function_info.support == Analysis::APIDumpIndex::GLFunctionSupport::UNSUPPORTED_TEXTURE)
    {
        report_unsupported_use(in_function,
                               "a texture is made or changed in a way that is not followed (the function)",
                               static_cast<uint64_t>(in_function) );
    }
    else
    if (function_info.support == Analysis::APIDumpIndex::GLFunctionSupport::UNSUPPORTED_FRAMEBUFFER_WRITE)
    {
        /* The model does not follow what these calls write to the framebuffer or which lists they call. Without them, the frames
         * (and the textures made from the framebuffer) would be reported wrongly.
         */
        report_unsupported_use(in_function,
                               "the framebuffer is written from somewhere other than a draw, or lists named by an array are called, which is not followed (the function)",
                               static_cast<uint64_t>(in_function) );
    }
    else
    if (function_info.support == Analysis::APIDumpIndex::GLFunctionSupport::UNSUPPORTED_DRAW_WRITE_STATE)
    {
        /* A frame whose only draw depends on this state could look like a frame that writes nothing, and a seek would skip it.
         * The call is reported instead of ignored.
         */
        report_unsupported_use(in_function,
                               "state that decides what a draw writes, or how its pixels are combined with what is there (the logic op, the blend equation, the blend constant color), is not followed (the function)",
                               static_cast<uint64_t>(in_function) );
    }
    else
    if (GLFunctionPolicy::is_left_out_by_design(function_info.kind) == false &&
        function_info.kind != Analysis::APIDumpIndex::GLFunctionKind::BAKES_CURRENT_MATRIX)
    {
        /* The front end does not know this call, so it cannot tell that the call is irrelevant. It is reported instead of ignored
         * (examples: shaders, buffers, evaluators, selection, the accumulation buffer, rectangles, pixel conversion state).
         */
        report_unsupported_use(in_function,
                               "the function is not followed, and is not one that is known to be of no matter to what is found of the frames (the function)",
                               static_cast<uint64_t>(in_function) );
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::report_unsupported_use(const APIInterceptor::APIFunction& in_function,
                                                                                 const char*                        in_reason_ptr,
                                                                                 const uint64_t&                    in_value)
{
    UnsupportedUse::report_with_value(in_function,
                                      in_reason_ptr,
                                      in_value);
}
