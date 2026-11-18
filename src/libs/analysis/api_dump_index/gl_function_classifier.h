/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Says what an OpenGL function is: the one place that every layer that has to recognize a function asks (the call describer, the state coalescing, the
 * policy of the front end). A function that is not here ends the process in all of them: no layer accepts a function that another does not know.
 */
#ifndef APIDUMPINDEX_GL_FUNCTION_CLASSIFIER_H
#define APIDUMPINDEX_GL_FUNCTION_CLASSIFIER_H

#include "analysis/api_dump_index/types.h"
#include "common/types.h"

namespace Analysis
{
    namespace APIDumpIndex
    {
        class GLFunctionClassifier
        {
        public:
            /* What the function is (GLFunctionKind::UNKNOWN if nobody knows it) and whether the analysis follows it. */
            static GLFunctionInfo classify(const APIInterceptor::APIFunction& in_function);

            /* Each of the two, for a caller that needs one: a lookup each, and the callers that ask for every call of a workload (the seek) pay for one only. */
            static GLFunctionKind    get_kind   (const APIInterceptor::APIFunction& in_function);
            static GLFunctionSupport get_support(const APIInterceptor::APIFunction& in_function);
        };
    }
}

#endif /* APIDUMPINDEX_GL_FUNCTION_CLASSIFIER_H */
