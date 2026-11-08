/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Which of the GL functions that the front end does not follow it may leave out, and why. A function is followed (the front end names it), or is left out by design
 * (it is of no matter to what the model finds, for a reason that is written beside it), and any other ends the process: a call that the front end does not know
 * is not one that it can pass over, since what is found would be wrong without it, and nobody would have said so.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_FUNCTION_POLICY_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_FUNCTION_POLICY_H

#include "common/types.h"

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GlFunctionPolicy
            {
            public:
                /* True for the functions that the front end does not follow and may leave out: the values for the vertices that follow, the state of the arrays that a draw
                 * reads, names, the state of the rendering that the model does not tell draws apart by, and the queries. False for every other: they are followed (by the
                 * front end, which does not ask this of them) or end the process.
                 */
                static bool is_left_out_by_design(const APIInterceptor::APIFunction& in_function);

            private:
                /* glGet*, glIs* and glAre*: they return what the program asks for and change nothing. */
                static bool is_query(const APIInterceptor::APIFunction& in_function);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_FUNCTION_POLICY_H */
