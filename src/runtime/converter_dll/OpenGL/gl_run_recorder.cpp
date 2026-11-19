/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "capture/api_dump_loader/api_info.h"
#include "capture/api_dump_names/function_names.h"
#include "common/argument_reader.h"
#include "common/macros.h"
#include "gl_block_recorder.h"
#include "gl_driver_functions.h"
#include "gl_run_recorder.h"
#include <string>

namespace
{
    constexpr uint32_t MODELVIEW  = 0x1700;
    constexpr uint32_t PROJECTION = 0x1701;
    constexpr uint32_t TEXTURE_2D = 0x0DE1;

    /* The functions that are listed here are told apart by what they are. The rest of OpenGL is told apart by the name: a call that asks for something begins with glGet or glIs. */
    Stereo::ConverterDLL::RunCallClass classify_gl_function(const APIInterceptor::APIFunction& in_function)
    {
        Stereo::ConverterDLL::RunCallClass result = Stereo::ConverterDLL::RunCallClass::FLUSH;

        switch (in_function)
        {
            case APIInterceptor::APIFUNCTION_GL_GLBEGIN:
            case APIInterceptor::APIFUNCTION_GL_GLEND:
            {
                result = Stereo::ConverterDLL::RunCallClass::NEUTRAL;

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLFRUSTUM:
            case APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY:
            case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD:
            case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF:
            case APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE:
            case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXD:
            case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF:
            case APIInterceptor::APIFUNCTION_GL_GLORTHO:
            case APIInterceptor::APIFUNCTION_GL_GLPOPMATRIX:
            case APIInterceptor::APIFUNCTION_GL_GLPUSHMATRIX:
            case APIInterceptor::APIFUNCTION_GL_GLROTATED:
            case APIInterceptor::APIFUNCTION_GL_GLROTATEF:
            case APIInterceptor::APIFUNCTION_GL_GLSCALED:
            case APIInterceptor::APIFUNCTION_GL_GLSCALEF:
            case APIInterceptor::APIFUNCTION_GL_GLTRANSLATED:
            case APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF:
            {
                result = Stereo::ConverterDLL::RunCallClass::MATRIX;

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
            case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
            case APIInterceptor::APIFUNCTION_GL_GLCOLORMASK:
            case APIInterceptor::APIFUNCTION_GL_GLCULLFACE:
            case APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC:
            case APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK:
            case APIInterceptor::APIFUNCTION_GL_GLDISABLE:
            case APIInterceptor::APIFUNCTION_GL_GLENABLE:
            case APIInterceptor::APIFUNCTION_GL_GLFRONTFACE:
            case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
            case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
            case APIInterceptor::APIFUNCTION_GL_GLTEXENVI:
            {
                result = Stereo::ConverterDLL::RunCallClass::RECORDED;

                break;
            }

            default:
            {
                std::string name;

                {
                    const char* const name_ptr = Capture::APIDumpNames::get_function_name(in_function);

                    name = (name_ptr != nullptr) ? name_ptr
                                                 : "";
                }

                const size_t cut = name.find_last_of(":. ");

                if (cut != std::string::npos)
                {
                    name = name.substr(cut + 1);
                }

                if (name.compare(0, 5, "glGet") == 0 ||
                    name.compare(0, 4, "glIs")  == 0)
                {
                    result = Stereo::ConverterDLL::RunCallClass::QUERY;
                }

                break;
            }
        }

        return result;
    }

    /* How many numbers a recorded state call has. */
    uint32_t get_n_state_arguments(const APIInterceptor::APIFunction& in_function)
    {
        uint32_t result = 0;

        switch (in_function)
        {
            case APIInterceptor::APIFUNCTION_GL_GLCULLFACE:
            case APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC:
            case APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK:
            case APIInterceptor::APIFUNCTION_GL_GLDISABLE:
            case APIInterceptor::APIFUNCTION_GL_GLENABLE:
            case APIInterceptor::APIFUNCTION_GL_GLFRONTFACE:
            {
                result = 1;

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
            case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
            {
                result = 2;

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
            case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
            case APIInterceptor::APIFUNCTION_GL_GLTEXENVI:
            {
                result = 3;

                break;
            }

            case APIInterceptor::APIFUNCTION_GL_GLCOLORMASK:
            {
                result = 4;

                break;
            }

            default:
            {
                AI_FAIL_UNSUPPORTED(in_function);

                break;
            }
        }

        return result;
    }

    std::vector<Stereo::ConverterDLL::RunCallClass> make_class_table()
    {
        std::vector<Stereo::ConverterDLL::RunCallClass> result(APIInterceptor::APIFUNCTION_COUNT,
                                                               Stereo::ConverterDLL::RunCallClass::NEUTRAL);

        for (uint32_t n_api_func = 0;
                      n_api_func < APIInterceptor::APIFUNCTION_COUNT;
                    ++n_api_func)
        {
            const auto function = static_cast<APIInterceptor::APIFunction>(n_api_func);
            const auto group    = Capture::APIDumpLoader::get_api_group(function);

            /* The calls of the window system that make or change the context, and the end of the frame, need the right eye to be complete. */
            if (group == Capture::APIDumpLoader::APIGroup::GL)
            {
                result.at(n_api_func) = classify_gl_function(function);
            }
            else
            if (group    == Capture::APIDumpLoader::APIGroup::WGL ||
                function == APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS)
            {
                result.at(n_api_func) = Stereo::ConverterDLL::RunCallClass::FLUSH;
            }
        }

        return result;
    }
}

Stereo::ConverterDLL::GLRunRecorder::GLRunRecorder()
{
    /* Stub */
}

void Stereo::ConverterDLL::GLRunRecorder::add_attribute(const RecordedCall& in_call)
{
    RunItem item;

    item.index = static_cast<uint32_t>(m_call_vec.size() );
    item.kind  = RunItemKind::ATTRIBUTE;

    m_call_vec.push_back(in_call);
    m_item_vec.push_back(item);
}

void Stereo::ConverterDLL::GLRunRecorder::add_draw(const RunDraw&      in_draw,
                                                   const RecordedCall* in_calls_ptr)
{
    RunDraw draw = in_draw;
    RunItem item;

    draw.first_call_index = static_cast<uint32_t>(m_call_vec.size() );

    item.index = static_cast<uint32_t>(m_draw_vec.size() );
    item.kind  = RunItemKind::DRAW;

    m_call_vec.insert(m_call_vec.end(),
                      in_calls_ptr,
                      in_calls_ptr + in_draw.n_calls);
    m_draw_vec.push_back(draw);
    m_item_vec.push_back(item);
}

void Stereo::ConverterDLL::GLRunRecorder::add_state(const RunStateCall& in_call)
{
    RunItem item;

    item.index = static_cast<uint32_t>(m_state_vec.size() );
    item.kind  = RunItemKind::STATE;

    m_state_vec.push_back(in_call);
    m_item_vec.push_back(item);
}

Stereo::ConverterDLL::RunCallClass Stereo::ConverterDLL::GLRunRecorder::classify(const APIInterceptor::APIFunction& in_function)
{
    static const std::vector<RunCallClass> class_vec = make_class_table();

    return class_vec.at(in_function);
}

void Stereo::ConverterDLL::GLRunRecorder::clear()
{
    m_call_vec.clear();
    m_draw_vec.clear();
    m_item_vec.clear();
    m_state_vec.clear();
}

uint32_t Stereo::ConverterDLL::GLRunRecorder::get_n_draws() const
{
    return static_cast<uint32_t>(m_draw_vec.size() );
}

bool Stereo::ConverterDLL::GLRunRecorder::is_empty() const
{
    return m_item_vec.empty();
}

void Stereo::ConverterDLL::GLRunRecorder::play() const
{
    const auto&           driver                 = GLDriverFunctions::get();
    bool                  has_projection         = false;
    bool                  is_modelview_current   = false;
    std::array<float, 16> last_projection_f32mat = {};

    for (const auto& item : m_item_vec)
    {
        switch (item.kind)
        {
            case RunItemKind::ATTRIBUTE:
            {
                GLBlockRecorder::play_span(&m_call_vec.at(item.index),
                                           1);

                break;
            }

            case RunItemKind::DRAW:
            {
                const RunDraw& draw = m_draw_vec.at(item.index);

                /* The matrices are those of the moment the block ended, whatever the calls of the run did to them. */
                if (is_modelview_current == false)
                {
                    driver.matrix_mode_func_ptr(MODELVIEW);

                    is_modelview_current = true;
                }

                driver.load_matrixf_func_ptr(draw.modelview_f32mat.data() );

                if (has_projection == false                              ||
                    draw.projection_f32mat != last_projection_f32mat)
                {
                    driver.matrix_mode_func_ptr (PROJECTION);
                    driver.load_matrixf_func_ptr(draw.projection_f32mat.data() );
                    driver.matrix_mode_func_ptr (MODELVIEW);

                    has_projection         = true;
                    last_projection_f32mat = draw.projection_f32mat;
                }

                if (draw.selection.has_twin)
                {
                    driver.bind_texture_func_ptr(TEXTURE_2D,
                                                 draw.selection.twin);
                }

                if (draw.has_pushed_viewport)
                {
                    driver.viewport_func_ptr(draw.pushed_viewport_i32vec4.at(0),
                                             draw.pushed_viewport_i32vec4.at(1),
                                             draw.pushed_viewport_i32vec4.at(2),
                                             draw.pushed_viewport_i32vec4.at(3) );
                }

                GLBlockRecorder::play_block(draw.mode,
                                            &m_call_vec.at(draw.first_call_index),
                                            draw.n_calls);

                if (draw.has_pushed_viewport)
                {
                    driver.viewport_func_ptr(draw.saved_viewport_i32vec4.at(0),
                                             draw.saved_viewport_i32vec4.at(1),
                                             draw.saved_viewport_i32vec4.at(2),
                                             draw.saved_viewport_i32vec4.at(3) );
                }

                if (draw.selection.has_twin)
                {
                    driver.bind_texture_func_ptr(TEXTURE_2D,
                                                 draw.selection.texture);
                }

                break;
            }

            case RunItemKind::STATE:
            {
                play_state(m_state_vec.at(item.index) );

                break;
            }

            default:
            {
                AI_FAIL_UNSUPPORTED(item.kind);

                break;
            }
        }
    }
}

void Stereo::ConverterDLL::GLRunRecorder::play_state(const RunStateCall& in_call)
{
    const auto&                   driver = GLDriverFunctions::get();
    const std::array<int32_t, 4>& value  = in_call.value_i32vec4;

    switch (in_call.function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
        {
            driver.bind_texture_func_ptr(value.at(0),
                                         value.at(1) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLBLENDFUNC:
        {
            driver.blend_function_func_ptr(value.at(0),
                                           value.at(1) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCOLORMASK:
        {
            driver.color_mask_func_ptr(static_cast<GLboolean>(value.at(0) ),
                                       static_cast<GLboolean>(value.at(1) ),
                                       static_cast<GLboolean>(value.at(2) ),
                                       static_cast<GLboolean>(value.at(3) ) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCULLFACE:
        {
            driver.cull_face_func_ptr(value.at(0) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDEPTHFUNC:
        {
            driver.depth_function_func_ptr(value.at(0) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDEPTHMASK:
        {
            driver.depth_mask_func_ptr(static_cast<GLboolean>(value.at(0) ) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDISABLE:
        {
            driver.disable_func_ptr(value.at(0) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLENABLE:
        {
            driver.enable_func_ptr(value.at(0) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLFRONTFACE:
        {
            driver.front_face_func_ptr(value.at(0) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLSTENCILFUNC:
        {
            driver.stencil_function_func_ptr(value.at(0),
                                             value.at(1),
                                             static_cast<GLuint>(value.at(2) ) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLSTENCILOP:
        {
            driver.stencil_op_func_ptr(value.at(0),
                                       value.at(1),
                                       value.at(2) );

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXENVI:
        {
            driver.tex_envi_func_ptr(value.at(0),
                                     value.at(1),
                                     value.at(2) );

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(in_call.function);

            break;
        }
    }
}

bool Stereo::ConverterDLL::GLRunRecorder::read_state_call(const APIInterceptor::APIFunction&         in_function,
                                                          const uint32_t&                            in_n_arguments,
                                                          const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                          RunStateCall*                              out_call_ptr)
{
    const uint32_t n_values = get_n_state_arguments(in_function);
    bool           result   = (in_n_arguments >= n_values);

    out_call_ptr->function = in_function;

    for (uint32_t n_value = 0;
                  n_value < n_values && result;
                ++n_value)
    {
        result = APIInterceptor::read_number(in_arguments_ptr[n_value],
                                            &out_call_ptr->value_i32vec4.at(n_value) );
    }

    return result;
}
