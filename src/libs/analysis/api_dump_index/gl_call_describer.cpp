/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/gl_call_describer.h"
#include "analysis/api_dump_index/gl_function_classifier.h"

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

    switch (GLFunctionClassifier::get_kind(in_function))
    {
        case GLFunctionKind::CONDITIONAL:
        {
            result = CallClass::CONDITIONAL;

            break;
        }

        case GLFunctionKind::CONTENT:
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
