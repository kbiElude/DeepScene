/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_argument_reader.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_state_handler.h"
#include <array>

Analysis::APIDumpAnalysis::Frontends::GLStateHandler::GLStateHandler(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr)
    :m_builder_ptr       (in_builder_ptr),
     m_n_unfollowed_calls(0)
{
    /* Stub */
}

uint64_t Analysis::APIDumpAnalysis::Frontends::GLStateHandler::get_n_unfollowed_calls() const
{
    return m_n_unfollowed_calls;
}

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_clear_color(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              GLContextState*                              inout_state_ptr)
{
    std::array<float, 4> color_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                        &color_array) )
    {
        inout_state_ptr->attributes.clear_color_fp32vec4 = color_array;
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_clear_depth(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              GLContextState*                              inout_state_ptr)
{
    std::array<double, 1> depth_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                        &depth_array) )
    {
        inout_state_ptr->attributes.clear_depth = depth_array.at(0);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_clear_stencil(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                GLContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> stencil_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                        &stencil_array) )
    {
        inout_state_ptr->attributes.clear_stencil = stencil_array.at(0);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_enable(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                         const bool&                                  in_is_enabled,
                                                                         GLContextState*                              inout_state_ptr)
{
    constexpr uint32_t SCISSOR_TEST = 0x0C11;

    std::array<uint32_t, 1> capability_array = {};

    if (GLArgumentReader::read_arguments(in_call,
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
    if (GLPipelineState::is_unit_capability(capability_array.at(0) ) )
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

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_pop_attributes(const GLWindow& in_window,
                                                                                 GLContextState* inout_state_ptr)
{
    if (inout_state_ptr->attribute_frame_vec.empty() )
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const GLAttributeFrame frame        = inout_state_ptr->attribute_frame_vec.back();
        const GLBox            old_viewport = inout_state_ptr->attributes.viewport;

        inout_state_ptr->attribute_frame_vec.pop_back();

        restore_attributes(frame,
                          &inout_state_ptr->attributes);

        inout_state_ptr->pipeline_state.restore(frame.pipeline_state,
                                                frame.mask);

        const bool is_viewport_same = is_same_box(old_viewport,
                                                  inout_state_ptr->attributes.viewport);

        /* The restored state may contain a different viewport, which changes what is rendered. */
        const bool has_window = in_window.has_default_framebuffer();

        if (has_window &&
            is_viewport_same == false)
        {
            m_builder_ptr->on_set_viewport(GLWindow::get_model_viewport(*inout_state_ptr) );
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_pop_client_attributes(GLContextState* inout_state_ptr)
{
    constexpr uint32_t CLIENT_PIXEL_STORE_BIT = 0x00000001;

    if (inout_state_ptr->client_attribute_frame_vec.empty() )
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        const GLClientAttributeFrame frame = inout_state_ptr->client_attribute_frame_vec.back();

        inout_state_ptr->client_attribute_frame_vec.pop_back();

        if ( (frame.mask & CLIENT_PIXEL_STORE_BIT) != 0)
        {
            inout_state_ptr->pixel_store = frame.pixel_store;
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_push_attributes(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                  GLContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> mask_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                        &mask_array) )
    {
        GLAttributeFrame frame;

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

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_push_client_attributes(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                         GLContextState*                              inout_state_ptr)
{
    std::array<uint32_t, 1> mask_array = {};

    if (GLArgumentReader::read_arguments(in_call,
                                        &mask_array) )
    {
        GLClientAttributeFrame frame;

        frame.mask        = mask_array.at(0);
        frame.pixel_store = inout_state_ptr->pixel_store;

        inout_state_ptr->client_attribute_frame_vec.push_back(frame);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_scissor(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                          ResolutionHazardCounter*                     inout_counter_ptr,
                                                                          GLContextState*                              inout_state_ptr)
{
    GLBox box;

    if (read_box(in_call,
                &box) )
    {
        inout_state_ptr->attributes.scissor_box = box;

        inout_counter_ptr->count(Analysis::APIDumpAnalysis::ResolutionHazard::SCISSOR_BOXES);
    }
    else
    {
        m_n_unfollowed_calls++;
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_state_value(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                              GLContextState*                              inout_state_ptr)
{
    constexpr uint32_t BACK           = 0x0405;
    constexpr uint32_t FRONT          = 0x0404;
    constexpr uint32_t FRONT_AND_BACK = 0x0408;

    std::array<uint32_t, 4> argument_array = {};
    GLPipelineState*        pipeline_ptr   = &inout_state_ptr->pipeline_state;
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

        case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
        case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
        {
            n_arguments = 3;

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
                pipeline_ptr->set_value(GLStateValue::ALPHA_FUNC,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
            {
                pipeline_ptr->set_value(GLStateValue::BLEND_SOURCE,
                                        argument_array.at(0) );
                pipeline_ptr->set_value(GLStateValue::BLEND_DESTINATION,
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

                pipeline_ptr->set_value(GLStateValue::COLOR_MASK,
                                        mask);

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLCULLFACE:
            {
                pipeline_ptr->set_value(GLStateValue::CULL_FACE,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC:
            {
                pipeline_ptr->set_value(GLStateValue::DEPTH_FUNC,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK:
            {
                pipeline_ptr->set_value(GLStateValue::DEPTH_MASK,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLFRONTFACE:
            {
                pipeline_ptr->set_value(GLStateValue::FRONT_FACE,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLPOLYGONMODE:
            {
                const bool is_back = (argument_array.at(0) == BACK  ||
                                       argument_array.at(0) == FRONT_AND_BACK);
                const bool is_front = (argument_array.at(0) == FRONT ||
                                       argument_array.at(0) == FRONT_AND_BACK);

                if (is_back)
                {
                    pipeline_ptr->set_value(GLStateValue::POLYGON_MODE_BACK,
                                            argument_array.at(1) );
                }

                if (is_front)
                {
                    pipeline_ptr->set_value(GLStateValue::POLYGON_MODE_FRONT,
                                            argument_array.at(1) );
                }

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLSHADEMODEL:
            {
                pipeline_ptr->set_value(GLStateValue::SHADE_MODEL,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
            {
                pipeline_ptr->set_value(GLStateValue::STENCIL_FUNC,
                                        argument_array.at(0) );
                pipeline_ptr->set_value(GLStateValue::STENCIL_REFERENCE,
                                        argument_array.at(1) );
                pipeline_ptr->set_value(GLStateValue::STENCIL_VALUE_MASK,
                                        argument_array.at(2) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLSTENCILMASK:
            {
                pipeline_ptr->set_value(GLStateValue::STENCIL_WRITE_MASK,
                                        argument_array.at(0) );

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
            {
                pipeline_ptr->set_value(GLStateValue::STENCIL_FAIL,
                                        argument_array.at(0) );
                pipeline_ptr->set_value(GLStateValue::STENCIL_DEPTH_FAIL,
                                        argument_array.at(1) );
                pipeline_ptr->set_value(GLStateValue::STENCIL_DEPTH_PASS,
                                        argument_array.at(2) );

                break;
            }

            default:
            {
                break;
            }
        }
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::handle_viewport(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                           const GLWindow&                              in_window,
                                                                           GLContextState*                              inout_state_ptr)
{
    GLBox box;

    if (read_box(in_call,
                &box) == false)
    {
        m_n_unfollowed_calls++;
    }
    else
    {
        inout_state_ptr->attributes.viewport = box;

        /* The model tracks the viewport of the render target, which exists only while a default framebuffer is present. */
        if (in_window.has_default_framebuffer() )
        {
            m_builder_ptr->on_set_viewport(GLWindow::get_model_viewport(*inout_state_ptr) );
        }
    }
}

bool Analysis::APIDumpAnalysis::Frontends::GLStateHandler::is_same_box(const GLBox& in_first,
                                                                       const GLBox& in_second)
{
    return (in_first.height == in_second.height &&
            in_first.width  == in_second.width  &&
            in_first.x      == in_second.x      &&
            in_first.y      == in_second.y);
}

bool Analysis::APIDumpAnalysis::Frontends::GLStateHandler::read_box(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                    GLBox*                                       out_box_ptr)
{
    std::array<int32_t, 4> value_array = {};

    const bool result = GLArgumentReader::read_arguments(in_call,
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

void Analysis::APIDumpAnalysis::Frontends::GLStateHandler::restore_attributes(const GLAttributeFrame& in_frame,
                                                                              GLAttributes*           inout_attributes_ptr)
{
    constexpr uint32_t COLOR_BUFFER_BIT   = 0x00004000;
    constexpr uint32_t DEPTH_BUFFER_BIT   = 0x00000100;
    constexpr uint32_t ENABLE_BIT         = 0x00002000;
    constexpr uint32_t SCISSOR_BIT        = 0x00080000;
    constexpr uint32_t STENCIL_BUFFER_BIT = 0x00000400;
    constexpr uint32_t TEXTURE_BIT        = 0x00040000;
    constexpr uint32_t TRANSFORM_BIT      = 0x00001000;
    constexpr uint32_t VIEWPORT_BIT       = 0x00000800;

    const GLAttributes& saved = in_frame.attributes;

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

    /* The scissor test flag belongs to both the enable group and the scissor group. */
    if ( (in_frame.mask & (ENABLE_BIT | SCISSOR_BIT) ) != 0)
    {
        inout_attributes_ptr->is_scissor_enabled = saved.is_scissor_enabled;
    }

    if ( (in_frame.mask & SCISSOR_BIT) != 0)
    {
        inout_attributes_ptr->scissor_box = saved.scissor_box;
    }

    /* The bound textures and the active texture unit. */
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
