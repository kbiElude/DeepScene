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
#include "analysis/api_dump_analysis/frontends/shared/unsupported_use.h"
#include "capture/api_dump_loader/api_info.h"
#include "capture/api_dump_names/function_names.h"
#include "common/macros.h"
#include <stdio.h>
#include <string.h>

namespace
{
    constexpr uint32_t MODELVIEW_MODE = 0x1700; /* GL_MODELVIEW, the matrix mode */
}

Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::OpenGLFrontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_array_draw_describer    (in_builder_ptr),
     m_builder_ptr             (in_builder_ptr),
     m_cached_context_state_ptr(nullptr),
     m_cached_thread_id        (0),
     m_framebuffer_height      (0),
     m_framebuffer_width       (0),
     m_has_default_framebuffer (false),
     m_list_depth              (0),
     m_n_unfollowed_calls      (0),
     m_n_unsupported_calls     (0),
     m_texture_handler         (in_builder_ptr)
{
    /* Stub */
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::add_view_anchor(const GlContextState&                       in_state,
                                                                          Analysis::APIDumpAnalysis::DrawDescription* out_description_ptr) const
{
    out_description_ptr->view_anchor_call_index = in_state.modelview_anchor_call_index_vec.back();
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::copy_to_bound_texture(const GlContextState& in_state,
                                                                                 const uint32_t&       in_level,
                                                                                 const uint32_t&       in_x_offset,
                                                                                 const uint32_t&       in_y_offset,
                                                                                 const uint32_t&       in_width,
                                                                                 const uint32_t&       in_height)
{
    const uint32_t name        = GlTextureHandler::get_bound_texture_name(in_state);
    const auto*    texture_ptr = m_texture_handler.find_texture_ptr(name);
    const bool     result      = (texture_ptr != nullptr);

    if (result)
    {
        ensure_default_framebuffer(in_state);

        /* What is copied is the color of the window, or its depth if the texture is of depth. */
        const auto format_class = GlTextureHandler::get_format_class(texture_ptr->internal_format);
        const auto source       = (format_class == Analysis::APIDumpAnalysis::FormatClass::COLOR) ? m_color_view
                                                                                                   : m_depth_stencil_view;

        Analysis::APIDumpAnalysis::AccessRectangle destination_rectangle;
        Analysis::APIDumpAnalysis::ResourceView    destination;

        destination.mip_level   = in_level;
        destination.resource_id = texture_ptr->resource_id;

        /* Where in the level the copy lands, for the frames that the dependencies say are not needed: a texture that the frames fill a strip at a time is
         * whole again after a few of them.
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

        count_hazard(Analysis::APIDumpAnalysis::ResolutionHazard::COPIES_WITH_RECTANGLES);
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::count_hazard(const Analysis::APIDumpAnalysis::ResolutionHazard& in_hazard)
{
    m_n_resolution_hazard_map[in_hazard]++;
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::ensure_default_framebuffer(const GlContextState& in_state)
{
    if (m_has_default_framebuffer == false)
    {
        Analysis::APIDumpAnalysis::AttachmentSet       attachments;
        Analysis::APIDumpAnalysis::ResourceDescription color_description;
        Analysis::APIDumpAnalysis::ResourceDescription depth_description;
        const Analysis::APIDumpAnalysis::Viewport      viewport = get_model_viewport(in_state);

        m_framebuffer_height = static_cast<uint32_t>(viewport.height);
        m_framebuffer_width  = static_cast<uint32_t>(viewport.width);

        color_description.format.format_class = Analysis::APIDumpAnalysis::FormatClass::COLOR;
        color_description.height              = m_framebuffer_height;
        color_description.is_render_target    = true;
        color_description.kind                = Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE;
        color_description.width               = m_framebuffer_width;

        depth_description.format.format_class = Analysis::APIDumpAnalysis::FormatClass::DEPTH_STENCIL;
        depth_description.height              = m_framebuffer_height;
        depth_description.is_depth_stencil    = true;
        depth_description.kind                = Analysis::APIDumpAnalysis::ResourceKind::TEXTURE_2D;
        depth_description.width               = m_framebuffer_width;

        m_color_view.resource_id         = m_builder_ptr->on_create_resource(color_description,
                                                                             "Default framebuffer: color");
        m_depth_stencil_view.resource_id = m_builder_ptr->on_create_resource(depth_description,
                                                                             "Default framebuffer: depth and stencil");

        attachments.color_view_vec.push_back(m_color_view);

        attachments.depth_stencil_view = m_depth_stencil_view;

        m_builder_ptr->on_set_attachments(attachments);
        m_builder_ptr->on_set_viewport   (viewport);

        m_has_default_framebuffer = true;
    }
}

Analysis::APIDumpAnalysis::Frontends::GlContextState* Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::find_current_context_ptr(const uint32_t& in_thread_id)
{
    GlContextState* result_ptr = nullptr;

    /* The thread that makes a call nearly always made the one before it. A state is never taken out of the map, so one that was found stays good until handle_make_current(). */
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

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::find_list_eye_dependence(const uint32_t&      in_list,
                                                                                    GlListEyeDependence* out_dependence_ptr) const
{
    const auto list_iterator = m_display_list_map.find(in_list);
    const bool result        = (list_iterator != m_display_list_map.end() );

    if (result)
    {
        *out_dependence_ptr = list_iterator->second->eye_dependence;
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::find_resource_view(const uint64_t&                          in_address,
                                                                              Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const
{
    return m_texture_handler.find_resource_view(in_address,
                                                out_view_ptr);
}

const Analysis::APIDumpAnalysis::Frontends::GlBlobStore& Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_blob_store() const
{
    return m_array_draw_describer.get_blob_store();
}

Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_call_eye_dependence(const APIInterceptor::APIFunction&                      in_function,
                                                                                                                                        const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec) const
{
    GlListEyeDependence result = GlListClassifier::classify_call(in_function);

    if (in_function == APIInterceptor::APIFUNCTION_GL_GLCALLLIST)
    {
        uint32_t list     = 0;
        bool     is_found = false;

        if (in_argument_vec.empty() == false)
        {
            is_found = APIInterceptor::read_number(in_argument_vec.at(0),
                                                   &list);
        }

        /* A list that is not there yet when the call is kept may be anything when it is made: it is taken for the worst. */
        result = GlListEyeDependence::EYE_DEPENDENT;

        if (is_found)
        {
            find_list_eye_dependence(list,
                                     &result);
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_current_matrices(const uint32_t& in_thread_id,
                                                                                GlMatrix*       out_modelview_f32mat_ptr,
                                                                                GlMatrix*       out_projection_f32mat_ptr) const
{
    const auto thread_iterator = m_current_context_by_thread_map.find(in_thread_id);
    const auto thread_end      = m_current_context_by_thread_map.end();
    bool       result          = false;

    if (thread_iterator != thread_end)
    {
        const GlContextState& state = m_context_map.at(thread_iterator->second);

        *out_modelview_f32mat_ptr  = state.modelview_f32mat_vec.back();
        *out_projection_f32mat_ptr = state.projection_f32mat_vec.back();

        result = true;
    }

    return result;
}

std::vector<Analysis::APIDumpAnalysis::Frontends::GlMatrix>* Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_current_stack_ptr(GlContextState* inout_state_ptr)
{
    constexpr uint32_t MODELVIEW  = 0x1700;
    constexpr uint32_t PROJECTION = 0x1701;

    std::vector<GlMatrix>* result_ptr = nullptr;

    switch (inout_state_ptr->attributes.matrix_mode)
    {
        case MODELVIEW:
        {
            result_ptr = &inout_state_ptr->modelview_f32mat_vec;

            break;
        }

        case PROJECTION:
        {
            result_ptr = &inout_state_ptr->projection_f32mat_vec;

            break;
        }

        default:
        {
            /* The texture and the color matrices. */
            result_ptr = &inout_state_ptr->other_f32mat_vec;

            break;
        }
    }

    return result_ptr;
}

Analysis::APIDumpAnalysis::Frontends::GlContextState Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_initial_context_state()
{
    const GlMatrix identity = GlMatrixMath::get_identity();
    GlContextState result;

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

Analysis::APIDumpAnalysis::Viewport Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_model_viewport(const GlContextState& in_state)
{
    Analysis::APIDumpAnalysis::Viewport result;

    result.height = static_cast<float>(in_state.attributes.viewport.height);
    result.width  = static_cast<float>(in_state.attributes.viewport.width);
    result.x      = static_cast<float>(in_state.attributes.viewport.x);
    result.y      = static_cast<float>(in_state.attributes.viewport.y);

    return result;
}

const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_n_resolution_hazards() const
{
    return m_n_resolution_hazard_map;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_n_unfollowed_calls() const
{
    return m_n_unfollowed_calls + m_texture_handler.get_n_unfollowed_calls();
}

uint64_t Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_n_unsupported_calls() const
{
    return m_n_unsupported_calls;
}

Analysis::APIDumpAnalysis::PrimitiveTopology Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_primitive_topology(const uint32_t& in_mode)
{
    constexpr uint32_t LINES          = 0x0001;
    constexpr uint32_t LINE_LOOP      = 0x0002;
    constexpr uint32_t LINE_STRIP     = 0x0003;
    constexpr uint32_t POINTS         = 0x0000;
    constexpr uint32_t POLYGON        = 0x0009;
    constexpr uint32_t QUADS          = 0x0007;
    constexpr uint32_t QUAD_STRIP     = 0x0008;
    constexpr uint32_t TRIANGLES      = 0x0004;
    constexpr uint32_t TRIANGLE_FAN   = 0x0006;
    constexpr uint32_t TRIANGLE_STRIP = 0x0005;

    Analysis::APIDumpAnalysis::PrimitiveTopology result = Analysis::APIDumpAnalysis::PrimitiveTopology::UNKNOWN;

    switch (in_mode)
    {
        case LINES:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::LINE_LIST;

            break;
        }

        case LINE_LOOP:
        case LINE_STRIP:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::LINE_STRIP;

            break;
        }

        case POINTS:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::POINT_LIST;

            break;
        }

        case POLYGON:
        case TRIANGLE_FAN:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_FAN;

            break;
        }

        case QUADS:
        case TRIANGLES:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_LIST;

            break;
        }

        case QUAD_STRIP:
        case TRIANGLE_STRIP:
        {
            result = Analysis::APIDumpAnalysis::PrimitiveTopology::TRIANGLE_STRIP;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_array_draw(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                             const Capture::APIDumpLoader::Workload&      in_workload,
                                                                             const bool&                                  in_is_indexed,
                                                                             GlContextState*                              inout_state_ptr)
{
    constexpr size_t MODE_ARGUMENT = 0;

    uint32_t mode         = 0;
    bool     is_mode_read = false;

    if (in_call.arg_in_vec.size() > MODE_ARGUMENT)
    {
        is_mode_read = APIInterceptor::read_number(in_call.arg_in_vec.at(MODE_ARGUMENT),
                                                   &mode);
    }

    if (is_mode_read == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const auto                                 topology = get_primitive_topology(mode);
        Analysis::APIDumpAnalysis::DrawDescription description;

        const auto result = m_array_draw_describer.describe(in_call,
                                                            in_workload,
                                                            in_is_indexed,
                                                            topology,
                                                            inout_state_ptr->pipeline_state,
                                                            &description);

        switch (result)
        {
            case GlArrayDrawResult::DRAW:
            {
                ensure_default_framebuffer(*inout_state_ptr);

                m_texture_handler.add_texture_bindings(*inout_state_ptr,
                                                       &description);
                add_view_anchor                       (*inout_state_ptr,
                                                       &description);
                m_texture_handler.report_snapshots    (*inout_state_ptr);

                m_builder_ptr->on_draw(description);

                break;
            }

            case GlArrayDrawResult::EMPTY:
            {
                break;
            }

            default:
            {
                m_n_unfollowed_calls++;

                break;
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_begin(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                        GlContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> mode_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                        &mode_array) )
    {
        GlImmediateBlock& block = inout_state_ptr->immediate_block;

        /* A glBegin that comes while a block is open starts it again. */
        block.first_call_index = m_builder_ptr->get_call_index();
        block.is_open         = true;
        block.mode            = mode_array.at(0);
        block.n_vertices      = 0;
        block.wants_positions = m_builder_ptr->wants_geometry();

        block.position_vec.clear();
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_bind_buffer(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    constexpr uint32_t PIXEL_PACK_BUFFER   = 0x88EB;
    constexpr uint32_t PIXEL_UNPACK_BUFFER = 0x88EC;

    std::array<uint32_t, 2> bind_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                         &bind_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (bind_array.at(0) == PIXEL_PACK_BUFFER &&
        bind_array.at(1) != 0)
    {
        /* What glReadPixels reads goes to the buffer, and not to the memory of the program: the bytes are in no call of the workload, and the program may have
         * them made into a texture from the buffer.
         */
        report_unsupported_use(in_call.func,
                               "pixels are packed into a buffer, so that what is read back never reaches the memory of the program and is in no call (the target)",
                               bind_array.at(0) );
    }
    else
    if (bind_array.at(0) == PIXEL_UNPACK_BUFFER &&
        bind_array.at(1) != 0)
    {
        /* The pixels of what is uploaded from now on are in the buffer, and not in the call: the bytes of an upload would be left out. */
        report_unsupported_use(in_call.func,
                               "pixels are unpacked from a buffer, so that the pixels of an upload are not in the call (the target)",
                               bind_array.at(0) );
    }
    else
    if (bind_array.at(1) != 0)
    {
        /* The data of a buffer (vertices, indices, constants) is made by calls that the front end does not follow, and a draw that reads from one comes with no
         * snapshot of what it reads: the geometry of the draw would be left out.
         */
        report_unsupported_use(in_call.func,
                               "a buffer object is bound, and buffer objects are not followed, so that what a draw reads from one is in no snapshot (the target)",
                               bind_array.at(0) );
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_call_list(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    /* A list that calls itself, or lists that call each other, are not made for ever. */
    constexpr uint32_t MAX_LIST_DEPTH = 64;

    std::array<uint32_t, 1> list_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                         &list_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const auto list_iterator = m_display_list_map.find(list_array.at(0) );

        if (list_iterator == m_display_list_map.end() ||
            m_list_depth  >= MAX_LIST_DEPTH)
        {
            m_n_unfollowed_calls++;
        }
        else
        {
            /* The list is held on to, as a call of it may delete it. */
            const std::shared_ptr<GlDisplayList> list_ptr = list_iterator->second;

            /* One call is reused for the whole list, so that its room for the arguments is allocated once and not for every recorded call. */
            Capture::APIDumpLoader::DumpedAPICall call;

            call.thread_id     = in_call.thread_id;
            call.timestamp_qpc = in_call.timestamp_qpc;

            ++m_list_depth;

            for (const auto& recorded_call : list_ptr->call_vec)
            {
                call.arg_in_vec = recorded_call.arg_in_vec;
                call.func       = recorded_call.func;

                handle_gl_call(call,
                               list_ptr->workload);
            }

            --m_list_depth;
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_clear(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                        GlContextState*                              inout_state_ptr)
{
    constexpr uint32_t COLOR_BUFFER_BIT   = 0x4000;
    constexpr uint32_t DEPTH_BUFFER_BIT   = 0x0100;
    constexpr uint32_t STENCIL_BUFFER_BIT = 0x0400;

    std::array<uint32_t, 1> mask_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                        &mask_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        ensure_default_framebuffer(*inout_state_ptr);

        const GlBox& box = inout_state_ptr->attributes.scissor_box;

        /* With the scissor test on, a clear reaches the box and no more, unless the box is the whole framebuffer. */
        const bool is_box_whole = (box.x      == 0                                         &&
                                   box.y      == 0                                         &&
                                   box.width  == static_cast<int32_t>(m_framebuffer_width) &&
                                   box.height == static_cast<int32_t>(m_framebuffer_height) );

        Analysis::APIDumpAnalysis::ClearOperation clear;

        clear.clears_color           = ( (mask_array.at(0) & COLOR_BUFFER_BIT)   != 0);
        clear.clears_depth           = ( (mask_array.at(0) & DEPTH_BUFFER_BIT)   != 0);
        clear.clears_stencil         = ( (mask_array.at(0) & STENCIL_BUFFER_BIT) != 0);
        clear.color_fp32vec4         = inout_state_ptr->attributes.clear_color_fp32vec4;
        clear.depth                  = static_cast<float>(inout_state_ptr->attributes.clear_depth);
        clear.has_rectangles         = (inout_state_ptr->attributes.is_scissor_enabled &&
                                        is_box_whole == false);
        clear.is_limited_to_viewport = false;
        clear.stencil                = inout_state_ptr->attributes.clear_stencil;

        m_builder_ptr->on_clear(clear);

        if (clear.has_rectangles)
        {
            count_hazard(Analysis::APIDumpAnalysis::ResolutionHazard::CLEARS_WITH_RECTANGLES);
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_clear_color(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              GlContextState*                              inout_state_ptr)
{
    std::array<float, 4> color_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                        &color_array) )
    {
        inout_state_ptr->attributes.clear_color_fp32vec4 = color_array;
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_clear_depth(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              GlContextState*                              inout_state_ptr)
{
    std::array<double, 1> depth_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                        &depth_array) )
    {
        inout_state_ptr->attributes.clear_depth = depth_array.at(0);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_clear_stencil(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                GlContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> stencil_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                        &stencil_array) )
    {
        inout_state_ptr->attributes.clear_stencil = stencil_array.at(0);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_copy_tex_image(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                 GlContextState*                              inout_state_ptr)
{
    /* glCopyTexImage2D(target, level, internalformat, x, y, width, height, border). */
    constexpr uint32_t TEXTURE_2D = 0x0DE1;

    std::array<int32_t, 7> image_array = {};

    if (GlArgumentReader::read_arguments(in_call,
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

        GlTextureLevel texture_level;

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
                                                                                     GlContextState*                              inout_state_ptr)
{
    /* glCopyTexSubImage2D(target, level, xoffset, yoffset, x, y, width, height). */
    constexpr uint32_t TEXTURE_2D = 0x0DE1;

    std::array<uint32_t, 8> image_array = {};

    if (GlArgumentReader::read_arguments(in_call,
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
        /* A part of a texture that has not been given a size. */
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_delete_lists(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    std::array<int32_t, 2> list_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                         &list_array) )
    {
        const int32_t first_list = list_array.at(0);
        const int32_t n_lists    = list_array.at(1);

        for (int32_t n_list = 0;
                     n_list < n_lists;
                   ++n_list)
        {
            m_display_list_map.erase(static_cast<uint32_t>(first_list + n_list) );
        }
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_enable(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                         const bool&                                  in_is_enabled,
                                                                         GlContextState*                              inout_state_ptr)
{
    constexpr uint32_t SCISSOR_TEST = 0x0C11;

    std::array<uint32_t, 1> capability_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                        &capability_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (capability_array.at(0) == SCISSOR_TEST)
    {
        inout_state_ptr->attributes.is_scissor_enabled = in_is_enabled;
    }
    else
    if (GlPipelineState::is_unit_capability(capability_array.at(0) ) )
    {
        inout_state_ptr->pipeline_state.set_unit_capability(inout_state_ptr->attributes.active_texture_unit,
                                                            capability_array.at(0),
                                                            in_is_enabled);
    }
    else
    {
        inout_state_ptr->pipeline_state.set_capability(capability_array.at(0),
                                                       in_is_enabled);
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_end(GlContextState* inout_state_ptr)
{
    GlImmediateBlock& block = inout_state_ptr->immediate_block;

    /* The vertices of a block are one draw; a glEnd with no glBegin, or a block with no vertices, is none. */
    if (block.is_open &&
        block.n_vertices > 0)
    {
        ensure_default_framebuffer(*inout_state_ptr);

        const auto topology = get_primitive_topology(block.mode);

        auto description = inout_state_ptr->pipeline_state.describe_draw(topology,
                                                                         block.n_vertices);

        /* The vertices are calls of the draw: a draw that is left out is left out from the glBegin. */
        description.first_call_index = block.first_call_index;

        m_texture_handler.add_texture_bindings(*inout_state_ptr,
                                               &description);
        add_view_anchor                       (*inout_state_ptr,
                                               &description);
        m_texture_handler.report_snapshots    (*inout_state_ptr);

        if (block.wants_positions)
        {
            description.geometry_ptr = GlGeometrySampler::make_sample(block.position_vec);
        }

        m_builder_ptr->on_draw(description);
    }

    block.is_open = false;
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_end_list(GlContextState* inout_state_ptr)
{
    GlListRecording& recording = inout_state_ptr->list_recording;

    if (recording.list_ptr != nullptr)
    {
        m_display_list_map[recording.list_number] = recording.list_ptr;

        recording = GlListRecording();
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_gl_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                          const Capture::APIDumpLoader::Workload&      in_workload)
{
    GlContextState* state_ptr = find_current_context_ptr(in_call.thread_id);

    /* A call made by a thread which has no context current does nothing. */
    if (state_ptr == nullptr)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        /* A call made while a list is made is kept in it, and is made when the list is called (and at once too, for GL_COMPILE_AND_EXECUTE). */
        const bool is_recording = (state_ptr->list_recording.list_ptr != nullptr                  &&
                                   in_call.func                       != APIInterceptor::APIFUNCTION_GL_GLENDLIST);
        const bool is_made      = (is_recording                       == false                    ||
                                   state_ptr->list_recording.is_executing);

        if (is_recording)
        {
            record_call(in_call,
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
                case APIInterceptor::APIFUNCTION_GL_GLPOLYGONMODE:
                case APIInterceptor::APIFUNCTION_GL_GLSHADEMODEL:
                {
                    handle_state_value(in_call,
                                       state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS:
                case APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS:
                {
                    const bool is_indexed = (in_call.func == APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS);

                    handle_array_draw(in_call,
                                      in_workload,
                                      is_indexed,
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
                    handle_begin(in_call,
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
                    handle_call_list(in_call);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCLEAR:
                {
                    handle_clear(in_call,
                                 state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCLEARCOLOR:
                {
                    handle_clear_color(in_call,
                                       state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCLEARDEPTH:
                {
                    handle_clear_depth(in_call,
                                       state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLCLEARSTENCIL:
                {
                    handle_clear_stencil(in_call,
                                         state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLDISABLE:
                {
                    handle_enable(in_call,
                                  false, /* in_is_enabled */
                                  state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLENABLE:
                {
                    handle_enable(in_call,
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
                    handle_delete_lists(in_call);

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
                    handle_end(state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLENDLIST:
                {
                    handle_end_list(state_ptr);

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
                    handle_matrix(in_call,
                                  in_workload,
                                  state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE:
                {
                    handle_matrix_mode(in_call,
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
                    count_hazard(Analysis::APIDumpAnalysis::ResolutionHazard::LINE_WIDTH_STATES);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
                {
                    handle_new_list(in_call,
                                    state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPOINTSIZE:
                {
                    count_hazard(Analysis::APIDumpAnalysis::ResolutionHazard::POINT_SIZE_STATES);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPOPATTRIB:
                {
                    handle_pop_attributes(state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX:
                {
                    handle_pop_matrix(state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPUSHATTRIB:
                {
                    handle_push_attributes(in_call,
                                           state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX:
                {
                    handle_push_matrix(state_ptr);

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLREADPIXELS:
                {
                    /* What a program reads back is used by the program, which the model does not follow. What it makes of the pixels that it reads into its memory (a
                     * texture, say) is in the call that gives it, as the bytes of an upload; reading to a buffer is not possible here, as binding one ends the process.
                     */
                    count_hazard(Analysis::APIDumpAnalysis::ResolutionHazard::PIXEL_READBACKS);

                    m_n_unsupported_calls++;

                    break;
                }

                case APIInterceptor::APIFUNCTION_GL_GLSCISSOR:
                {
                    handle_scissor(in_call,
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
                    handle_viewport(in_call,
                                    state_ptr);

                    break;
                }

                default:
                {
                    GlVertexLayout vertex_layout;

                    const bool is_vertex = GlVertexReader::find_layout(in_call.func,
                                                                       &vertex_layout);

                    if (is_vertex)
                    {
                        handle_vertex(in_call,
                                      in_workload,
                                      state_ptr);
                    }
                    else
                    if (GlTextureHandler::is_unsupported_texture_function(in_call.func) )
                    {
                        report_unsupported_use(in_call.func,
                                               "a texture is made or changed in a way that is not followed (the function)",
                                               static_cast<uint64_t>(in_call.func) );
                    }
                    else
                    if (is_unsupported_function(in_call.func) )
                    {
                        /* What it writes to the framebuffer, or the lists that it calls, is not in anything the model follows: what is found of the frames would be
                         * wrong without it (and so would the textures that are made of the framebuffer).
                         */
                        report_unsupported_use(in_call.func,
                                               "the framebuffer is written from somewhere other than a draw, or lists named by an array are called, which is not followed (the function)",
                                               static_cast<uint64_t>(in_call.func) );
                    }
                    else
                    if (GlFunctionPolicy::changes_what_a_draw_writes(in_call.func) )
                    {
                        /* Not followed: a frame that has only a draw that this decides could be taken for one that writes nothing, and a seek would leave it out.
                         * It is said, and not passed over.
                         */
                        report_unsupported_use(in_call.func,
                                               "state that decides what a draw writes, or how its pixels are combined with what is there (the stencil test, the logic op, the blend equation, the winding of faces), is not followed (the function)",
                                               static_cast<uint64_t>(in_call.func) );
                    }
                    else
                    if (GlFunctionPolicy::is_left_out_by_design(in_call.func) == false &&
                        GlFunctionPolicy::bakes_current_matrix (in_call.func) == false)
                    {
                        /* Not followed, and not known to be of no matter: a call that the front end has never been told about is not passed over without saying so (a
                         * shader, a buffer, an evaluator, the selection, an accumulation buffer, a rectangle, what changes how pixels are converted, and so on).
                         */
                        report_unsupported_use(in_call.func,
                                               "the function is not followed, and is not one that is known to be of no matter to what is found of the frames (the function)",
                                               static_cast<uint64_t>(in_call.func) );
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
        /* A call which failed changed nothing (the first unbind of a program, with no context current, fails). */
        const uint64_t context_address = in_call.arg_in_vec.at(CONTEXT_ARGUMENT).get_ptr_as_u64();

        /* The context of a thread is changed here and nowhere else: what find_current_context_ptr() kept is not to be trusted. */
        m_cached_context_state_ptr = nullptr;

        if (context_address == 0)
        {
            m_current_context_by_thread_map.erase(in_call.thread_id);
        }
        else
        {
            m_context_map.emplace(context_address,
                                  get_initial_context_state() );

            m_current_context_by_thread_map[in_call.thread_id] = context_address;

            /* What the model builder has been told of the matrices is those of the context that was current before. */
            report_matrices(m_context_map.at(context_address) );
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_matrix(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                         const Capture::APIDumpLoader::Workload&      in_workload,
                                                                         GlContextState*                              inout_state_ptr)
{
    const bool is_identity = (in_call.func == APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY);
    const bool is_load     = (is_identity                                                       ||
                              in_call.func == APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD      ||
                              in_call.func == APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF);
    bool       has_operand = true;
    GlMatrix   operand     = GlMatrixMath::get_identity();

    if (is_identity == false)
    {
        has_operand = read_matrix_operand(in_call,
                                          in_workload,
                                          &operand);
    }

    if (has_operand == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        std::vector<GlMatrix>* stack_ptr = get_current_stack_ptr(inout_state_ptr);
        const GlMatrix         current   = stack_ptr->back();

        if (is_load)
        {
            stack_ptr->back() = operand;
        }
        else
        {
            stack_ptr->back() = GlMatrixMath::multiply(current,
                                                       operand);
        }

        /* A load replaces what the matrix was, so the camera of everything that is done to it next begins with the load: it is where a player puts the shift of an eye.
         * A push copies the matrix with its camera, a pop gives back the one of the matrix below, and what is multiplied in leaves it as it is.
         */
        if (is_load                                                  &&
            inout_state_ptr->attributes.matrix_mode == MODELVIEW_MODE)
        {
            inout_state_ptr->modelview_anchor_call_index_vec.back() = m_builder_ptr->get_call_index();
        }

        report_matrices(*inout_state_ptr);
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_matrix_mode(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              GlContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> mode_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                        &mode_array) )
    {
        inout_state_ptr->attributes.matrix_mode = mode_array.at(0);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_new_list(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                           GlContextState*                              inout_state_ptr)
{
    constexpr uint32_t COMPILE             = 0x1300;
    constexpr uint32_t COMPILE_AND_EXECUTE = 0x1301;

    std::array<uint32_t, 2> list_array  = {};
    bool                    is_followed = false;

    if (GlArgumentReader::read_arguments(in_call,
                                         &list_array) )
    {
        const uint32_t mode = list_array.at(1);

        /* The calls made while a list is made are kept in it, and are not made, unless the mode says that they are made too. */
        if (mode == COMPILE ||
            mode == COMPILE_AND_EXECUTE)
        {
            GlListRecording& recording = inout_state_ptr->list_recording;

            recording.is_executing = (mode == COMPILE_AND_EXECUTE);
            recording.list_number  = list_array.at(0);
            recording.list_ptr     = std::make_shared<GlDisplayList>();
            is_followed            = true;
        }
    }

    if (is_followed == false)
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_pop_attributes(GlContextState* inout_state_ptr)
{
    if (inout_state_ptr->attribute_frame_vec.empty() )
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const GlAttributeFrame frame        = inout_state_ptr->attribute_frame_vec.back();
        const GlBox            old_viewport = inout_state_ptr->attributes.viewport;

        inout_state_ptr->attribute_frame_vec.pop_back();

        restore_attributes(frame,
                          &inout_state_ptr->attributes);

        inout_state_ptr->pipeline_state.restore(frame.pipeline_state,
                                                frame.mask);

        const bool is_viewport_same = is_same_box(old_viewport,
                                                  inout_state_ptr->attributes.viewport);

        /* What was put back may be another viewport, which is a change for what is rendered to. */
        if (m_has_default_framebuffer &&
            is_viewport_same == false)
        {
            m_builder_ptr->on_set_viewport(get_model_viewport(*inout_state_ptr) );
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_pop_matrix(GlContextState* inout_state_ptr)
{
    std::vector<GlMatrix>* stack_ptr = get_current_stack_ptr(inout_state_ptr);

    /* A stack is never empty: the matrix at its bottom is the one that the context starts with. */
    if (stack_ptr->size() <= 1)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        stack_ptr->pop_back();

        if (inout_state_ptr->attributes.matrix_mode == MODELVIEW_MODE)
        {
            inout_state_ptr->modelview_anchor_call_index_vec.pop_back();
        }

        report_matrices(*inout_state_ptr);
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_push_attributes(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                  GlContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> mask_array = {};

    if (GlArgumentReader::read_arguments(in_call,
                                        &mask_array) )
    {
        GlAttributeFrame frame;

        frame.attributes     = inout_state_ptr->attributes;
        frame.mask           = mask_array.at(0);
        frame.pipeline_state = inout_state_ptr->pipeline_state;

        inout_state_ptr->attribute_frame_vec.push_back(frame);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_push_matrix(GlContextState* inout_state_ptr)
{
    std::vector<GlMatrix>* stack_ptr = get_current_stack_ptr(inout_state_ptr);
    const GlMatrix         current   = stack_ptr->back();

    stack_ptr->push_back(current);

    if (inout_state_ptr->attributes.matrix_mode == MODELVIEW_MODE)
    {
        inout_state_ptr->modelview_anchor_call_index_vec.push_back(inout_state_ptr->modelview_anchor_call_index_vec.back() );
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_scissor(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                          GlContextState*                              inout_state_ptr)
{
    GlBox box;

    if (read_box(in_call,
                &box) )
    {
        inout_state_ptr->attributes.scissor_box = box;

        count_hazard(Analysis::APIDumpAnalysis::ResolutionHazard::SCISSOR_BOXES);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_state_value(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              GlContextState*                              inout_state_ptr)
{
    constexpr uint32_t BACK           = 0x0405;
    constexpr uint32_t FRONT          = 0x0404;
    constexpr uint32_t FRONT_AND_BACK = 0x0408;

    std::array<uint32_t, 4> argument_array = {};
    GlPipelineState*        pipeline_ptr   = &inout_state_ptr->pipeline_state;
    size_t                  n_arguments    = 1;

    /* The functions that have more than one argument. */
    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLALPHAFUNC:
        case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
        case APIInterceptor::APIFUNCTION_GL_GLPOLYGONMODE:
        {
            n_arguments = 2;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCOLORMASK:
        {
            n_arguments = 4;

            break;
        }

        default:
        {
            break;
        }
    }

    bool are_read = (in_call.arg_in_vec.size() >= n_arguments);

    for (size_t n_argument = 0;
                are_read &&
                n_argument < n_arguments;
              ++n_argument)
    {
        are_read = APIInterceptor::read_number(in_call.arg_in_vec.at(n_argument),
                                               &argument_array.at(n_argument) );
    }

    if (are_read == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        switch (in_call.func)
        {
            case APIInterceptor::APIFUNCTION_GL_GLALPHAFUNC:
            {
                pipeline_ptr->set_value(GlStateValue::ALPHA_FUNC,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
            {
                pipeline_ptr->set_value(GlStateValue::BLEND_SOURCE,
                                        argument_array.at(0) );
                pipeline_ptr->set_value(GlStateValue::BLEND_DESTINATION,
                                        argument_array.at(1) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLCOLORMASK:
            {
                constexpr uint32_t ALPHA_BIT = 8;
                constexpr uint32_t BLUE_BIT  = 4;
                constexpr uint32_t GREEN_BIT = 2;
                constexpr uint32_t RED_BIT   = 1;

                /* The four flags in one number. */
                const uint32_t mask = ( (argument_array.at(0) != 0) ? RED_BIT   : 0) |
                                      ( (argument_array.at(1) != 0) ? GREEN_BIT : 0) |
                                      ( (argument_array.at(2) != 0) ? BLUE_BIT  : 0) |
                                      ( (argument_array.at(3) != 0) ? ALPHA_BIT : 0);

                pipeline_ptr->set_value(GlStateValue::COLOR_MASK,
                                        mask);

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLCULLFACE:
            {
                pipeline_ptr->set_value(GlStateValue::CULL_FACE,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC:
            {
                pipeline_ptr->set_value(GlStateValue::DEPTH_FUNC,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK:
            {
                pipeline_ptr->set_value(GlStateValue::DEPTH_MASK,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLPOLYGONMODE:
            {
                const bool is_back  = (argument_array.at(0) == BACK  ||
                                       argument_array.at(0) == FRONT_AND_BACK);
                const bool is_front = (argument_array.at(0) == FRONT ||
                                       argument_array.at(0) == FRONT_AND_BACK);

                if (is_back)
                {
                    pipeline_ptr->set_value(GlStateValue::POLYGON_MODE_BACK,
                                            argument_array.at(1) );
                }

                if (is_front)
                {
                    pipeline_ptr->set_value(GlStateValue::POLYGON_MODE_FRONT,
                                            argument_array.at(1) );
                }

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLSHADEMODEL:
            {
                pipeline_ptr->set_value(GlStateValue::SHADE_MODEL,
                                        argument_array.at(0) );

                break;
            }

            default:
            {
                break;
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_vertex(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                         const Capture::APIDumpLoader::Workload&      in_workload,
                                                                         GlContextState*                              inout_state_ptr)
{
    GlImmediateBlock& block = inout_state_ptr->immediate_block;

    /* A vertex that comes with no block open is one that OpenGL does not use. */
    if (block.is_open)
    {
        block.n_vertices++;

        if (block.wants_positions)
        {
            std::array<float, 3> position = {};

            if (GlVertexReader::read_position(in_call,
                                              in_workload,
                                              &position) )
            {
                block.position_vec.push_back(position);
            }
            else
            {
                m_n_unfollowed_calls++;
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_viewport(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                           GlContextState*                              inout_state_ptr)
{
    GlBox box;

    if (read_box(in_call,
                &box) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        inout_state_ptr->attributes.viewport = box;

        /* The model has the viewport of what is rendered to, and there is none before the first clear. */
        if (m_has_default_framebuffer)
        {
            m_builder_ptr->on_set_viewport(get_model_viewport(*inout_state_ptr) );
        }
    }
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::is_same_box(const GlBox& in_first,
                                                                       const GlBox& in_second)
{
    return (in_first.height == in_second.height &&
            in_first.width  == in_second.width  &&
            in_first.x      == in_second.x      &&
            in_first.y      == in_second.y);
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::is_unsupported_function(const APIInterceptor::APIFunction& in_function)
{
    bool result = false;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLBITMAP:
        case APIInterceptor::APIFUNCTION_GL_GLCALLLISTS:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWPIXELS:
        {
            result = true;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
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

    /* SwapBuffers is a function of GDI32, and the end of a frame of OpenGL. */
    const bool is_frame_boundary = Capture::APIDumpLoader::is_frame_boundary(in_call);

    if (is_frame_boundary)
    {
        m_array_draw_describer.on_frame_ended();

        m_builder_ptr->on_frame_presented();
    }
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::read_box(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                    GlBox*                                       out_box_ptr)
{
    std::array<int32_t, 4> value_array = {};

    const bool result = GlArgumentReader::read_arguments(in_call,
                                                        &value_array);

    if (result)
    {
        out_box_ptr->x      = value_array.at(0);
        out_box_ptr->y      = value_array.at(1);
        out_box_ptr->width  = value_array.at(2);
        out_box_ptr->height = value_array.at(3);
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::read_matrix_chunk(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                             const Capture::APIDumpLoader::Workload&      in_workload,
                                                                             const bool&                                  in_is_double,
                                                                             GlMatrix*                                    out_matrix_f32mat_ptr)
{
    constexpr size_t MATRIX_ARGUMENT = 0;

    const CallView                           view(in_call,
                                                  in_workload,
                                                  in_call.arg_in_vec.size() );
    const Capture::APIDumpLoader::DataChunk* chunk_ptr = view.get_chunk_ptr(MATRIX_ARGUMENT);
    bool                                     result    = false;

    if (chunk_ptr != nullptr)
    {
        const size_t n_bytes = chunk_ptr->size();

        if (in_is_double == false &&
            n_bytes      == sizeof(GlMatrix) )
        {
            ::memcpy(out_matrix_f32mat_ptr->data(),
                     chunk_ptr->data(),
                     sizeof(GlMatrix) );

            result = true;
        }
        else
        if (in_is_double &&
            n_bytes      == sizeof(std::array<double, 16>) )
        {
            std::array<double, 16> double_array = {};

            ::memcpy(double_array.data(),
                     chunk_ptr->data(),
                     sizeof(double_array) );

            for (size_t n_element = 0;
                        n_element < double_array.size();
                      ++n_element)
            {
                out_matrix_f32mat_ptr->at(n_element) = static_cast<float>(double_array.at(n_element) );
            }

            result = true;
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::read_matrix_operand(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                               const Capture::APIDumpLoader::Workload&      in_workload,
                                                                               GlMatrix*                                    out_matrix_f32mat_ptr)
{
    bool result = false;

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLFRUSTUM:
        case APIInterceptor::APIFUNCTION_GL_GLORTHO:
        {
            std::array<double, 6> box_array = {};

            result = GlArgumentReader::read_arguments(in_call,
                                                     &box_array);

            if (result)
            {
                const bool is_frustum = (in_call.func == APIInterceptor::APIFUNCTION_GL_GLFRUSTUM);

                *out_matrix_f32mat_ptr = (is_frustum) ? GlMatrixMath::get_frustum(box_array.at(0),
                                                                                  box_array.at(1),
                                                                                  box_array.at(2),
                                                                                  box_array.at(3),
                                                                                  box_array.at(4),
                                                                                  box_array.at(5) )
                                                      : GlMatrixMath::get_ortho  (box_array.at(0),
                                                                                  box_array.at(1),
                                                                                  box_array.at(2),
                                                                                  box_array.at(3),
                                                                                  box_array.at(4),
                                                                                  box_array.at(5) );
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD:
        case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXD:
        {
            result = read_matrix_chunk(in_call,
                                       in_workload,
                                       true, /* in_is_double */
                                       out_matrix_f32mat_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF:
        case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF:
        {
            result = read_matrix_chunk(in_call,
                                       in_workload,
                                       false, /* in_is_double */
                                       out_matrix_f32mat_ptr);

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLROTATED:
        case APIInterceptor::APIFUNCTION_GL_GLROTATEF:
        {
            std::array<double, 4> rotation_array = {};

            result = GlArgumentReader::read_arguments(in_call,
                                                     &rotation_array);

            if (result)
            {
                *out_matrix_f32mat_ptr = GlMatrixMath::get_rotation(rotation_array.at(0),
                                                                    rotation_array.at(1),
                                                                    rotation_array.at(2),
                                                                    rotation_array.at(3) );
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLSCALED:
        case APIInterceptor::APIFUNCTION_GL_GLSCALEF:
        case APIInterceptor::APIFUNCTION_GL_GLTRANSLATED:
        case APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF:
        {
            std::array<double, 3> vector_array = {};

            result = GlArgumentReader::read_arguments(in_call,
                                                     &vector_array);

            if (result)
            {
                const bool is_scale = (in_call.func == APIInterceptor::APIFUNCTION_GL_GLSCALED ||
                                       in_call.func == APIInterceptor::APIFUNCTION_GL_GLSCALEF);

                *out_matrix_f32mat_ptr = (is_scale) ? GlMatrixMath::get_scale      (vector_array.at(0),
                                                                                    vector_array.at(1),
                                                                                    vector_array.at(2) )
                                                    : GlMatrixMath::get_translation(vector_array.at(0),
                                                                                    vector_array.at(1),
                                                                                    vector_array.at(2) );
            }

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::record_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                       const Capture::APIDumpLoader::Workload&      in_workload,
                                                                       GlContextState*                              inout_state_ptr)
{
    GlDisplayList* list_ptr = inout_state_ptr->list_recording.list_ptr.get();
    GlRecordedCall recorded_call;

    recorded_call.arg_in_vec = in_call.arg_in_vec;
    recorded_call.func       = in_call.func;

    /* The workload that the call is in is let go of before the list is called, so that the list keeps the data of the call itself. */
    for (auto& argument : recorded_call.arg_in_vec)
    {
        if (argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
        {
            const auto chunk_ptr = Capture::APIDumpLoader::get_data_chunk_ptr(in_workload,
                                                                              argument.get_data_chunk_id() );

            if (chunk_ptr != nullptr)
            {
                const uint32_t chunk_id = static_cast<uint32_t>(list_ptr->workload.data_chunk_ptr_vec.size() );

                list_ptr->workload.data_chunk_ptr_vec.emplace_back(new Capture::APIDumpLoader::DataChunk(*chunk_ptr) );

                argument = APIInterceptor::APIFunctionArgument::create_data_chunk_id(chunk_id);
            }
            else
            {
                /* The data is not there: an argument which is not a chunk, so that it is not taken for another one of the list. */
                argument = APIInterceptor::APIFunctionArgument::create_void_ptr(nullptr);
            }
        }
    }

    const GlListEyeDependence call_dependence = get_call_eye_dependence(recorded_call.func,
                                                                        recorded_call.arg_in_vec);

    list_ptr->eye_dependence = GlListClassifier::combine(list_ptr->eye_dependence,
                                                         call_dependence);

    list_ptr->call_vec.push_back(std::move(recorded_call) );
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::report_matrices(const GlContextState& in_state)
{
    const GlMatrix& modelview  = in_state.modelview_f32mat_vec.back();
    const GlMatrix& projection = in_state.projection_f32mat_vec.back();

    report_matrix(modelview,
                  Analysis::APIDumpAnalysis::ConstantSemantic::MODEL_VIEW_MATRIX,
                  0,
                  &m_reported_modelview);
    report_matrix(projection,
                  Analysis::APIDumpAnalysis::ConstantSemantic::PROJECTION_MATRIX,
                  sizeof(GlMatrix),
                  &m_reported_projection);
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::report_matrix(const GlMatrix&                                    in_matrix_f32mat,
                                                                         const Analysis::APIDumpAnalysis::ConstantSemantic& in_semantic,
                                                                         const uint64_t&                                    in_byte_offset,
                                                                         GlReportedMatrix*                                  inout_reported_ptr)
{
    const bool is_changed = (inout_reported_ptr->has_value     == false ||
                             inout_reported_ptr->matrix_f32mat != in_matrix_f32mat);

    if (is_changed)
    {
        Analysis::APIDumpAnalysis::ConstantUpdate update;

        /* The state of the fixed-function pipeline has no memory the application can see: the front end says where the matrices are in it. */
        update.location.block_kind  = Analysis::APIDumpAnalysis::ConstantBlockKind::FIXED_FUNCTION_STATE;
        update.location.byte_offset = in_byte_offset;
        update.location.byte_size   = sizeof(GlMatrix);
        update.provenance           = Analysis::APIDumpAnalysis::ConstantProvenance::API_DEFINED;
        update.semantic             = in_semantic;

        update.value_u8_vec.resize(sizeof(GlMatrix) );

        ::memcpy(update.value_u8_vec.data(),
                 in_matrix_f32mat.data(),
                 sizeof(GlMatrix) );

        m_builder_ptr->on_update_constant(update);

        inout_reported_ptr->has_value     = true;
        inout_reported_ptr->matrix_f32mat = in_matrix_f32mat;
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

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::restore_attributes(const GlAttributeFrame& in_frame,
                                                                              GlAttributes*           inout_attributes_ptr)
{
    constexpr uint32_t COLOR_BUFFER_BIT   = 0x00004000;
    constexpr uint32_t DEPTH_BUFFER_BIT   = 0x00000100;
    constexpr uint32_t ENABLE_BIT         = 0x00002000;
    constexpr uint32_t SCISSOR_BIT        = 0x00080000;
    constexpr uint32_t STENCIL_BUFFER_BIT = 0x00000400;
    constexpr uint32_t TEXTURE_BIT        = 0x00040000;
    constexpr uint32_t TRANSFORM_BIT      = 0x00001000;
    constexpr uint32_t VIEWPORT_BIT       = 0x00000800;

    const GlAttributes& saved = in_frame.attributes;

    if ( (in_frame.mask & COLOR_BUFFER_BIT) != 0)
    {
        inout_attributes_ptr->clear_color_fp32vec4 = saved.clear_color_fp32vec4;
    }

    if ( (in_frame.mask & DEPTH_BUFFER_BIT) != 0)
    {
        inout_attributes_ptr->clear_depth = saved.clear_depth;
    }

    if ( (in_frame.mask & STENCIL_BUFFER_BIT) != 0)
    {
        inout_attributes_ptr->clear_stencil = saved.clear_stencil;
    }

    /* The scissor test is one of the flags of the enable group as well as of the scissor group. */
    if ( (in_frame.mask & (ENABLE_BIT | SCISSOR_BIT) ) != 0)
    {
        inout_attributes_ptr->is_scissor_enabled = saved.is_scissor_enabled;
    }

    if ( (in_frame.mask & SCISSOR_BIT) != 0)
    {
        inout_attributes_ptr->scissor_box = saved.scissor_box;
    }

    /* The textures that are bound, and the unit that is active. */
    if ( (in_frame.mask & TEXTURE_BIT) != 0)
    {
        inout_attributes_ptr->active_texture_unit = saved.active_texture_unit;
        inout_attributes_ptr->texture_binding_map = saved.texture_binding_map;
    }

    if ( (in_frame.mask & TRANSFORM_BIT) != 0)
    {
        inout_attributes_ptr->matrix_mode = saved.matrix_mode;
    }

    if ( (in_frame.mask & VIEWPORT_BIT) != 0)
    {
        inout_attributes_ptr->viewport = saved.viewport;
    }
}
