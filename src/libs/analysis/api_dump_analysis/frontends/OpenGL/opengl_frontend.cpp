/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/opengl_frontend.h"
#include "capture/api_dump_loader/api_info.h"

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
        result_ptr = &m_context_map[thread_iterator->second];
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

Analysis::APIDumpAnalysis::Viewport Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::get_model_viewport(const GlContextState& in_state)
{
    Analysis::APIDumpAnalysis::Viewport result;

    result.height = static_cast<float>(in_state.viewport.height);
    result.width  = static_cast<float>(in_state.viewport.width);
    result.x      = static_cast<float>(in_state.viewport.x);
    result.y      = static_cast<float>(in_state.viewport.y);

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

        const GlBox& box = inout_state_ptr->scissor_box;

        /* With the scissor test on, a clear reaches the box and no more, unless the box is the whole framebuffer. */
        const bool is_box_whole = (box.x      == 0                                         &&
                                   box.y      == 0                                         &&
                                   box.width  == static_cast<int32_t>(m_framebuffer_width) &&
                                   box.height == static_cast<int32_t>(m_framebuffer_height) );

        Analysis::APIDumpAnalysis::ClearOperation clear;

        clear.clears_color           = ( (mask_array.at(0) & COLOR_BUFFER_BIT)   != 0);
        clear.clears_depth           = ( (mask_array.at(0) & DEPTH_BUFFER_BIT)   != 0);
        clear.clears_stencil         = ( (mask_array.at(0) & STENCIL_BUFFER_BIT) != 0);
        clear.color_fp32vec4         = inout_state_ptr->clear_color_fp32vec4;
        clear.depth                  = static_cast<float>(inout_state_ptr->clear_depth);
        clear.has_rectangles         = (inout_state_ptr->is_scissor_enabled &&
                                        is_box_whole == false);
        clear.is_limited_to_viewport = false;
        clear.stencil                = inout_state_ptr->clear_stencil;

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
        inout_state_ptr->clear_color_fp32vec4 = color_array;
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
        inout_state_ptr->clear_depth = depth_array.at(0);
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
        inout_state_ptr->clear_stencil = stencil_array.at(0);
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
        inout_state_ptr->is_scissor_enabled = in_is_enabled;
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_make_current(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    constexpr size_t N_ARGUMENTS      = 2;
    constexpr size_t CONTEXT_ARGUMENT = 1;

    bool    has_result       = false;
    int32_t recorded_result  = 0;

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
                                  GlContextState() );

            m_current_context_by_thread_map[in_call.thread_id] = context_address;
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::OpenGLFrontend::handle_scissor(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                          GlContextState*                              inout_state_ptr)
{
    GlBox box;

    if (read_box(in_call,
                &box) )
    {
        inout_state_ptr->scissor_box = box;
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
        inout_state_ptr->viewport = box;

        /* The model has the viewport of what is rendered to, and there is none before the first clear. */
        if (m_has_default_framebuffer)
        {
            m_builder_ptr->on_set_viewport(get_model_viewport(*inout_state_ptr) );
        }
    }
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

    (void) in_workload;

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
