/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/opengl_frontend.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_matrix_math.h"
#include "analysis/api_dump_analysis/frontends/shared/call_view.h"
#include "capture/api_dump_loader/api_info.h"
#include <string.h>

Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::OpenGLFrontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_builder_ptr            (in_builder_ptr),
     m_framebuffer_height     (0),
     m_framebuffer_width      (0),
     m_has_default_framebuffer(false),
     m_n_unfollowed_calls     (0),
     m_n_unsupported_calls    (0)
{
    /* Stub */
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
    const auto      thread_iterator = m_current_context_by_thread_map.find(in_thread_id);
    const auto      thread_end      = m_current_context_by_thread_map.end();
    GlContextState* result_ptr      = nullptr;

    if (thread_iterator != thread_end)
    {
        result_ptr = &m_context_map.at(thread_iterator->second);
    }

    return result_ptr;
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::find_resource_view(const uint64_t&                          in_address,
                                                                              Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const
{
    /* The objects of OpenGL are numbers, not addresses of the application: there is nothing to find by an address. */
    (void) in_address;
    (void) out_view_ptr;

    return false;
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
    return m_n_unfollowed_calls;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_n_unsupported_calls() const
{
    return m_n_unsupported_calls;
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_clear(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                        GlContextState*                              inout_state_ptr)
{
    constexpr uint32_t COLOR_BUFFER_BIT   = 0x4000;
    constexpr uint32_t DEPTH_BUFFER_BIT   = 0x0100;
    constexpr uint32_t STENCIL_BUFFER_BIT = 0x0400;

    std::array<uint32_t, 1> mask_array = {};

    if (read_arguments(in_call,
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
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_clear_color(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              GlContextState*                              inout_state_ptr)
{
    std::array<float, 4> color_array = {};

    if (read_arguments(in_call,
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

    if (read_arguments(in_call,
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

    if (read_arguments(in_call,
                      &stencil_array) )
    {
        inout_state_ptr->attributes.clear_stencil = stencil_array.at(0);
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

    if (read_arguments(in_call,
                      &capability_array) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    if (capability_array.at(0) == SCISSOR_TEST)
    {
        inout_state_ptr->attributes.is_scissor_enabled = in_is_enabled;
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

        report_matrices(*inout_state_ptr);
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_matrix_mode(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              GlContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> mode_array = {};

    if (read_arguments(in_call,
                      &mode_array) )
    {
        inout_state_ptr->attributes.matrix_mode = mode_array.at(0);
    }
    else
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

        report_matrices(*inout_state_ptr);
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_push_attributes(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                  GlContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> mask_array = {};

    if (read_arguments(in_call,
                      &mask_array) )
    {
        GlAttributeFrame frame;

        frame.attributes = inout_state_ptr->attributes;
        frame.mask       = mask_array.at(0);

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
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_scissor(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                          GlContextState*                              inout_state_ptr)
{
    GlBox box;

    if (read_box(in_call,
                &box) )
    {
        inout_state_ptr->attributes.scissor_box = box;
    }
    else
    {
        m_n_unfollowed_calls++;
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
        case APIInterceptor::APIFUNCTION_GL_GLBEGIN:
        case APIInterceptor::APIFUNCTION_GL_GLBITMAP:
        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLREADPIXELS:
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
        GlContextState* state_ptr = find_current_context_ptr(in_call.thread_id);

        /* A call made by a thread which has no context current does nothing. */
        if (state_ptr == nullptr)
        {
            m_n_unfollowed_calls++;
        }
        else
        {
            switch (in_call.func)
            {
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

                case APIInterceptor::APIFUNCTION_GL_GLSCISSOR:
                {
                    handle_scissor(in_call,
                                   state_ptr);

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
                    if (is_unsupported_function(in_call.func) )
                    {
                        m_n_unsupported_calls++;
                    }

                    break;
                }
            }
        }
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
        m_builder_ptr->on_frame_presented();
    }
}

bool Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::read_box(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                    GlBox*                                       out_box_ptr)
{
    std::array<int32_t, 4> value_array = {};

    const bool result = read_arguments(in_call,
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

            result = read_arguments(in_call,
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

            result = read_arguments(in_call,
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

            result = read_arguments(in_call,
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

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::report_matrix(const GlMatrix&                                   in_matrix_f32mat,
                                                                         const Analysis::APIDumpAnalysis::ConstantSemantic& in_semantic,
                                                                         const uint64_t&                                   in_byte_offset,
                                                                         GlReportedMatrix*                                 inout_reported_ptr)
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

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::restore_attributes(const GlAttributeFrame& in_frame,
                                                                              GlAttributes*           inout_attributes_ptr)
{
    constexpr uint32_t COLOR_BUFFER_BIT   = 0x00004000;
    constexpr uint32_t DEPTH_BUFFER_BIT   = 0x00000100;
    constexpr uint32_t ENABLE_BIT         = 0x00002000;
    constexpr uint32_t SCISSOR_BIT        = 0x00080000;
    constexpr uint32_t STENCIL_BUFFER_BIT = 0x00000400;
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

    if ( (in_frame.mask & TRANSFORM_BIT) != 0)
    {
        inout_attributes_ptr->matrix_mode = saved.matrix_mode;
    }

    if ( (in_frame.mask & VIEWPORT_BIT) != 0)
    {
        inout_attributes_ptr->viewport = saved.viewport;
    }
}
