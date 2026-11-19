/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "common/macros.h"
#include "gl_block_recorder.h"
#include "gl_driver_functions.h"

Stereo::ConverterDLL::GLBlockRecorder::GLBlockRecorder()
    :m_mode(0)
{
    /* Stub */
}

void Stereo::ConverterDLL::GLBlockRecorder::add(const RecordedCallKind&     in_kind,
                                                const std::array<float, 4>& in_value_fp32vec4)
{
    RecordedCall call;

    call.kind           = in_kind;
    call.value_fp32vec4 = in_value_fp32vec4;

    m_call_vec.push_back(call);
}

void Stereo::ConverterDLL::GLBlockRecorder::begin(const uint32_t& in_mode)
{
    m_call_vec.clear();

    m_mode = in_mode;
}

bool Stereo::ConverterDLL::GLBlockRecorder::find_layout(const APIInterceptor::APIFunction& in_function,
                                                        RecordedCallLayout*                out_layout_ptr)
{
    bool result = true;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3F:     *out_layout_ptr = {false, RecordedCallKind::COLOR3,    3}; break;
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR3FV:    *out_layout_ptr = {true,  RecordedCallKind::COLOR3,    3}; break;
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4F:     *out_layout_ptr = {false, RecordedCallKind::COLOR4,    4}; break;
        case APIInterceptor::APIFUNCTION_GL_GLCOLOR4FV:    *out_layout_ptr = {true,  RecordedCallKind::COLOR4,    4}; break;
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3F:    *out_layout_ptr = {false, RecordedCallKind::NORMAL3,   3}; break;
        case APIInterceptor::APIFUNCTION_GL_GLNORMAL3FV:   *out_layout_ptr = {true,  RecordedCallKind::NORMAL3,   3}; break;
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2F:  *out_layout_ptr = {false, RecordedCallKind::TEXCOORD2, 2}; break;
        case APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2FV: *out_layout_ptr = {true,  RecordedCallKind::TEXCOORD2, 2}; break;
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2F:    *out_layout_ptr = {false, RecordedCallKind::VERTEX2,   2}; break;
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2FV:   *out_layout_ptr = {true,  RecordedCallKind::VERTEX2,   2}; break;
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3F:    *out_layout_ptr = {false, RecordedCallKind::VERTEX3,   3}; break;
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV:   *out_layout_ptr = {true,  RecordedCallKind::VERTEX3,   3}; break;

        default:
        {
            result = false;

            break;
        }
    }

    return result;
}

uint32_t Stereo::ConverterDLL::GLBlockRecorder::get_mode() const
{
    return m_mode;
}

bool Stereo::ConverterDLL::GLBlockRecorder::is_vertex(const RecordedCallKind& in_kind)
{
    return (in_kind == RecordedCallKind::VERTEX2 ||
            in_kind == RecordedCallKind::VERTEX3);
}

void Stereo::ConverterDLL::GLBlockRecorder::play() const
{
    const auto& driver = GLDriverFunctions::get();

    driver.begin_func_ptr(m_mode);

    play_calls();

    driver.end_func_ptr();
}

void Stereo::ConverterDLL::GLBlockRecorder::play_calls() const
{
    const auto& driver = GLDriverFunctions::get();

    for (const auto& call : m_call_vec)
    {
        const float* value_ptr = call.value_fp32vec4.data();

        switch (call.kind)
        {
            case RecordedCallKind::COLOR3:
            {
                driver.color3f_func_ptr(value_ptr[0], value_ptr[1], value_ptr[2]);

                break;
            }

            case RecordedCallKind::COLOR4:
            {
                driver.color4f_func_ptr(value_ptr[0], value_ptr[1], value_ptr[2], value_ptr[3]);

                break;
            }

            case RecordedCallKind::NORMAL3:
            {
                driver.normal3f_func_ptr(value_ptr[0], value_ptr[1], value_ptr[2]);

                break;
            }

            case RecordedCallKind::TEXCOORD2:
            {
                driver.tex_coord2f_func_ptr(value_ptr[0], value_ptr[1]);

                break;
            }

            case RecordedCallKind::VERTEX2:
            {
                driver.vertex2f_func_ptr(value_ptr[0], value_ptr[1]);

                break;
            }

            case RecordedCallKind::VERTEX3:
            {
                driver.vertex3f_func_ptr(value_ptr[0], value_ptr[1], value_ptr[2]);

                break;
            }

            default:
            {
                AI_FAIL_UNSUPPORTED(call.kind);

                break;
            }
        }
    }
}
