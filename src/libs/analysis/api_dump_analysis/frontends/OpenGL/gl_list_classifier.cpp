/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_list_classifier.h"

Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence Analysis::APIDumpAnalysis::Frontends::GlListClassifier::classify_call(const APIInterceptor::APIFunction& in_function)
{
    GlListEyeDependence result = GlListEyeDependence::INDEPENDENT;

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
            result = GlListEyeDependence::EYE_DEPENDENT;

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
            result = GlListEyeDependence::RELATIVE_MATRIX_ONLY;

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

Analysis::APIDumpAnalysis::Frontends::GlListEyeDependence Analysis::APIDumpAnalysis::Frontends::GlListClassifier::combine(const GlListEyeDependence& in_first,
                                                                                                                          const GlListEyeDependence& in_second)
{
    const uint32_t first_rank  = get_rank(in_first);
    const uint32_t second_rank = get_rank(in_second);

    return (first_rank >= second_rank) ? in_first
                                       : in_second;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::GlListClassifier::get_rank(const GlListEyeDependence& in_dependence)
{
    uint32_t result = 0;

    switch (in_dependence)
    {
        case GlListEyeDependence::EYE_DEPENDENT:
        {
            result = 2;

            break;
        }

        case GlListEyeDependence::RELATIVE_MATRIX_ONLY:
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
