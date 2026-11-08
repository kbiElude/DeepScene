/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Which of the Direct3D 8 functions that the front end does not follow it may leave out, and why. A function is followed (the front end names it), or is left out by
 * design (it is of no matter to what the model finds, for a reason that is written beside it), or ends the process: a call that the front end does not know is not one
 * that it can pass over, since what is found would be wrong without it, and nobody would have said so.
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
                /* The reason that a call of the function is one that the front end does not follow, and that it cannot leave out: what it does to a surface, a volume, a
                 * buffer, a matrix or the screen is in no data that the workload holds. Null for the others. Checked before is_left_out_by_design().
                 */
                static const char* get_unsupported_reason_ptr(const APIInterceptor::APIFunction& in_function);

                /* True for the functions that the front end does not follow and may leave out: the queries, the hints about where the driver keeps a resource, the state that
                 * changes how a draw looks and not what it writes, and the calls whose data is stored with a call that is followed. False for every other.
                 */
                static bool is_left_out_by_design(const APIInterceptor::APIFunction& in_function);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D8_D3D8_FUNCTION_POLICY_H */
