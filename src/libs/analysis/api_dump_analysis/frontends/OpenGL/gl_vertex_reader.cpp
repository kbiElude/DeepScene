/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_vertex_reader.h"
#include "analysis/api_dump_analysis/frontends/shared/call_view.h"
#include "common/argument_reader.h"
#include <string.h>

bool Analysis::APIDumpAnalysis::Frontends::GlVertexReader::find_layout(const APIInterceptor::APIFunction& in_function,
                                                                       GlVertexLayout*                    out_layout_ptr)
{
    bool result = true;

    *out_layout_ptr = GlVertexLayout();

    /* What each function takes: the number of components and their kind, and below whether it is the one of a pointer. */
    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2D:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2DV:
        {
            out_layout_ptr->component    = GlVertexComponent::DOUBLE;
            out_layout_ptr->n_components = 2;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2F:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2FV:
        {
            out_layout_ptr->component    = GlVertexComponent::FLOAT;
            out_layout_ptr->n_components = 2;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2I:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2IV:
        {
            out_layout_ptr->component    = GlVertexComponent::INT;
            out_layout_ptr->n_components = 2;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2S:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2SV:
        {
            out_layout_ptr->component    = GlVertexComponent::SHORT;
            out_layout_ptr->n_components = 2;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3D:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3DV:
        {
            out_layout_ptr->component    = GlVertexComponent::DOUBLE;
            out_layout_ptr->n_components = 3;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3F:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV:
        {
            out_layout_ptr->component    = GlVertexComponent::FLOAT;
            out_layout_ptr->n_components = 3;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3I:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3IV:
        {
            out_layout_ptr->component    = GlVertexComponent::INT;
            out_layout_ptr->n_components = 3;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3S:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3SV:
        {
            out_layout_ptr->component    = GlVertexComponent::SHORT;
            out_layout_ptr->n_components = 3;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4D:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4DV:
        {
            out_layout_ptr->component    = GlVertexComponent::DOUBLE;
            out_layout_ptr->n_components = 4;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4F:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4FV:
        {
            out_layout_ptr->component    = GlVertexComponent::FLOAT;
            out_layout_ptr->n_components = 4;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4I:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4IV:
        {
            out_layout_ptr->component    = GlVertexComponent::INT;
            out_layout_ptr->n_components = 4;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4S:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4SV:
        {
            out_layout_ptr->component    = GlVertexComponent::SHORT;
            out_layout_ptr->n_components = 4;

            break;
        }

        default:
        {
            result = false;

            break;
        }
    }

    if (result)
    {
        switch (in_function)
        {
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX2DV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX2FV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX2IV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX2SV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX3DV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX3IV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX3SV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX4DV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX4FV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX4IV:
            case APIInterceptor::APIFUNCTION_GL_GLVERTEX4SV:
            {
                out_layout_ptr->is_vector = true;

                break;
            }

            default:
            {
                break;
            }
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GlVertexReader::read_components(const Capture::APIDumpLoader::DataChunk& in_chunk,
                                                                           const GlVertexLayout&                    in_layout,
                                                                           std::array<double, 4>*                   out_component_array_ptr)
{
    size_t component_n_bytes = 0;

    switch (in_layout.component)
    {
        case GlVertexComponent::DOUBLE:
        {
            component_n_bytes = sizeof(double);

            break;
        }

        case GlVertexComponent::FLOAT:
        {
            component_n_bytes = sizeof(float);

            break;
        }

        case GlVertexComponent::INT:
        {
            component_n_bytes = sizeof(int32_t);

            break;
        }

        case GlVertexComponent::SHORT:
        {
            component_n_bytes = sizeof(int16_t);

            break;
        }

        default:
        {
            break;
        }
    }

    const size_t n_chunk_bytes = in_chunk.size();
    const bool   result        = (component_n_bytes != 0                                    &&
                                  n_chunk_bytes     >= component_n_bytes * in_layout.n_components);

    if (result)
    {
        for (size_t n_component = 0;
                    n_component < in_layout.n_components;
                  ++n_component)
        {
            const uint8_t* component_ptr = in_chunk.data() + n_component * component_n_bytes;

            switch (in_layout.component)
            {
                case GlVertexComponent::DOUBLE:
                {
                    double value = 0.0;

                    ::memcpy(&value,
                             component_ptr,
                             sizeof(value) );

                    out_component_array_ptr->at(n_component) = value;

                    break;
                }

                case GlVertexComponent::FLOAT:
                {
                    float value = 0.0f;

                    ::memcpy(&value,
                             component_ptr,
                             sizeof(value) );

                    out_component_array_ptr->at(n_component) = static_cast<double>(value);

                    break;
                }

                case GlVertexComponent::INT:
                {
                    int32_t value = 0;

                    ::memcpy(&value,
                             component_ptr,
                             sizeof(value) );

                    out_component_array_ptr->at(n_component) = static_cast<double>(value);

                    break;
                }

                case GlVertexComponent::SHORT:
                {
                    int16_t value = 0;

                    ::memcpy(&value,
                             component_ptr,
                             sizeof(value) );

                    out_component_array_ptr->at(n_component) = static_cast<double>(value);

                    break;
                }

                default:
                {
                    break;
                }
            }
        }
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GlVertexReader::read_position(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                         const Capture::APIDumpLoader::Workload&      in_workload,
                                                                         std::array<float, 3>*                        out_position_ptr)
{
    constexpr size_t POINTER_ARGUMENT = 0;

    std::array<double, 4> component_array = {};
    GlVertexLayout        layout;
    bool                  result          = find_layout(in_call.func,
                                                        &layout);

    if (result)
    {
        if (layout.is_vector)
        {
            const CallView                           view(in_call,
                                                          in_workload,
                                                          in_call.arg_in_vec.size() );
            const Capture::APIDumpLoader::DataChunk* chunk_ptr = view.get_chunk_ptr(POINTER_ARGUMENT);

            result = (chunk_ptr != nullptr);

            if (result)
            {
                result = read_components(*chunk_ptr,
                                         layout,
                                         &component_array);
            }
        }
        else
        {
            result = (in_call.arg_in_vec.size() >= layout.n_components);

            for (size_t n_component = 0;
                        result &&
                        n_component < layout.n_components;
                      ++n_component)
            {
                result = APIInterceptor::read_number(in_call.arg_in_vec.at(n_component),
                                                     &component_array.at(n_component) );
            }
        }
    }

    if (result)
    {
        out_position_ptr->at(0) = static_cast<float>(component_array.at(0) );
        out_position_ptr->at(1) = static_cast<float>(component_array.at(1) );
        out_position_ptr->at(2) = (layout.n_components >= 3) ? static_cast<float>(component_array.at(2) )
                                                             : 0.0f;
    }

    return result;
}
