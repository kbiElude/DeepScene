/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/gl_call_describer.h"

Analysis::APIDumpIndex::GLCallDescriber::GLCallDescriber()
    :m_is_recording(false)
{
    /* Stub */
}

Analysis::APIDumpIndex::CallClass Analysis::APIDumpIndex::GLCallDescriber::describe(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    CallClass result = CallClass::STATE;

    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLENDLIST:
        {
            m_is_recording = false;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
        {
            m_is_recording = true;

            break;
        }

        default:
        {
            if (m_is_recording == false)
            {
                result = get_function_class(in_call.func);
            }

            break;
        }
    }

    return result;
}

Analysis::APIDumpIndex::CallClass Analysis::APIDumpIndex::GLCallDescriber::get_function_class(const APIInterceptor::APIFunction& in_function)
{
    CallClass result = CallClass::STATE;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
        {
            result = CallClass::CONDITIONAL;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLARRAYELEMENT:
        case APIInterceptor::APIFUNCTION_GL_GLBEGIN:
        case APIInterceptor::APIFUNCTION_GL_GLBITMAP:
        case APIInterceptor::APIFUNCTION_GL_GLCLEAR:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWRANGEELEMENTS:
        case APIInterceptor::APIFUNCTION_GL_GLEND:
        case APIInterceptor::APIFUNCTION_GL_GLMULTIDRAWARRAYS:
        case APIInterceptor::APIFUNCTION_GL_GLMULTIDRAWELEMENTS:
        case APIInterceptor::APIFUNCTION_GL_GLREADPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLRECTD:
        case APIInterceptor::APIFUNCTION_GL_GLRECTDV:
        case APIInterceptor::APIFUNCTION_GL_GLRECTF:
        case APIInterceptor::APIFUNCTION_GL_GLRECTFV:
        case APIInterceptor::APIFUNCTION_GL_GLRECTI:
        case APIInterceptor::APIFUNCTION_GL_GLRECTIV:
        case APIInterceptor::APIFUNCTION_GL_GLRECTS:
        case APIInterceptor::APIFUNCTION_GL_GLRECTSV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2D:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2DV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2F:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2FV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2I:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2IV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2S:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX2SV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3D:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3DV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3F:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3I:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3IV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3S:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX3SV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4D:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4DV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4F:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4FV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4I:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4IV:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4S:
        case APIInterceptor::APIFUNCTION_GL_GLVERTEX4SV:
        {
            result = CallClass::CONTENT;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

Capture::APIDumpLoader::APIGroup Analysis::APIDumpIndex::GLCallDescriber::get_group() const
{
    return Capture::APIDumpLoader::APIGroup::GL;
}

bool Analysis::APIDumpIndex::GLCallDescriber::is_recording() const
{
    return m_is_recording;
}
