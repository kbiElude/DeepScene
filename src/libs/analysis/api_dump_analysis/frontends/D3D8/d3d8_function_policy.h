/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Decides which Direct3D 8 functions the front end may leave out, and why. Each function is in one of three groups. It is followed (the front
 * end handles it by name), or it is left out by design (it does not affect what the model finds, and the reason is written beside it), or it
 * ends the process. An unknown call must end the process: skipping it silently would make the result wrong.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D8_D3D8_FUNCTION_POLICY_H
#define APIDUMPANALYSIS_FRONTENDS_D3D8_D3D8_FUNCTION_POLICY_H

#include "common/types.h"

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class D3D8FunctionPolicy
            {
            public:
                /* Returns the reason why the front end does not follow the function and cannot leave it out: its effect on a surface, a volume, a
                 * buffer, a matrix or the screen is not in the workload data. Returns null for any other function. Checked before is_left_out_by_design().
                 */
                static const char* get_unsupported_reason_ptr(const APIInterceptor::APIFunction& in_function);

                /* Returns true for a function that the front end does not follow and may leave out: queries, hints about where the driver keeps a
                 * resource, state that changes how a draw looks but not what it writes, and calls whose data is stored with a call that is followed.
                 * Returns false for any other function.
                 */
                static bool is_left_out_by_design(const APIInterceptor::APIFunction& in_function);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D8_D3D8_FUNCTION_POLICY_H */
