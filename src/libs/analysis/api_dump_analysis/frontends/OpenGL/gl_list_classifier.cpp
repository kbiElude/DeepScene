/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_function_policy.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_list_classifier.h"

Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence Analysis::APIDumpAnalysis::Frontends::GLListClassifier::classify_call(const APIInterceptor::APIFunction& in_function)
{
    GLListEyeDependence result = GLListEyeDependence::INDEPENDENT;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLFRUSTUM:
        case APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY:
        case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD:
        case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF:
        case APIInterceptor::APIFUNCTION_GL_GLORTHO:
        case APIInterceptor::APIFUNCTION_GL_GLSCISSOR:
        case APIInterceptor::APIFUNCTION_GL_GLVIEWPORT:
        {
            result = GLListEyeDependence::EYE_DEPENDENT;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXD:
        case APIInterceptor::APIFUNCTION_GL_GLMULTMATRIXF:
        case APIInterceptor::APIFUNCTION_GL_GLROTATED:
        case APIInterceptor::APIFUNCTION_GL_GLROTATEF:
        case APIInterceptor::APIFUNCTION_GL_GLSCALED:
        case APIInterceptor::APIFUNCTION_GL_GLSCALEF:
        case APIInterceptor::APIFUNCTION_GL_GLTRANSLATED:
        case APIInterceptor::APIFUNCTION_GL_GLTRANSLATEF:
        {
            result = GLListEyeDependence::RELATIVE_MATRIX_ONLY;

            break;
        }

        default:
        {
            /* A call whose effect is relative to the current matrix (a light, a clip plane) depends on the eye only through that matrix. */
            if (GLFunctionPolicy::bakes_current_matrix(in_function) )
            {
                result = GLListEyeDependence::RELATIVE_MATRIX_ONLY;
            }

            break;
        }
    }

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GLListEyeDependence Analysis::APIDumpAnalysis::Frontends::GLListClassifier::combine(const GLListEyeDependence& in_first,
                                                                                                                          const GLListEyeDependence& in_second)
{
    const uint32_t first_rank  = get_rank(in_first);
    const uint32_t second_rank = get_rank(in_second);

    return (first_rank >= second_rank) ? in_first
                                       : in_second;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GLListClassifier::get_rank(const GLListEyeDependence& in_dependence)
{
    uint32_t result = 0;

    switch (in_dependence)
    {
        case GLListEyeDependence::EYE_DEPENDENT:
        {
            result = 2;

            break;
        }

        case GLListEyeDependence::RELATIVE_MATRIX_ONLY:
        {
            result = 1;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}
