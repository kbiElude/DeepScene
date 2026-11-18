/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What each function is comes from GLFunctionClassifier: the groups and the reason for each are written there.
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_function_policy.h"
#include "analysis/api_dump_index/gl_function_classifier.h"

bool Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::bakes_current_matrix(const APIInterceptor::APIFunction& in_function)
{
    return (Analysis::APIDumpIndex::GLFunctionClassifier::get_kind(in_function) == Analysis::APIDumpIndex::GLFunctionKind::BAKES_CURRENT_MATRIX);
}

bool Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::changes_what_a_draw_writes(const APIInterceptor::APIFunction& in_function)
{
    return (Analysis::APIDumpIndex::GLFunctionClassifier::get_support(in_function) == Analysis::APIDumpIndex::GLFunctionSupport::UNSUPPORTED_DRAW_WRITE_STATE);
}

bool Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::is_left_out_by_design(const APIInterceptor::APIFunction& in_function)
{
    return is_left_out_by_design(Analysis::APIDumpIndex::GLFunctionClassifier::get_kind(in_function));
}

bool Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::is_left_out_by_design(const Analysis::APIDumpIndex::GLFunctionKind& in_kind)
{
    bool result = false;

    switch (in_kind)
    {
        case Analysis::APIDumpIndex::GLFunctionKind::CLIENT_ARRAY_STATE:
        case Analysis::APIDumpIndex::GLFunctionKind::CURRENT_VALUE:
        case Analysis::APIDumpIndex::GLFunctionKind::NAME_OR_SIZE:
        case Analysis::APIDumpIndex::GLFunctionKind::QUERY:
        case Analysis::APIDumpIndex::GLFunctionKind::RENDER_STATE_UNTRACKED:
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
