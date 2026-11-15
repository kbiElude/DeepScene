/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Decides which GL functions the front end may leave out, and why. A function is either followed (the front end handles it by name) or left out by
 * design (it does not affect what the model finds, and the reason is written beside it). Any other function ends the process. An unknown call must
 * end the process: skipping it silently would make the result wrong.
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
            class GLFunctionPolicy
            {
            public:
                /* Returns true for a function that the front end does not follow, that does matter, and that it lets through. Its effect is relative to
                 * the model-view matrix at the moment of the call (the position or direction of a light, a clip plane, the planes for generating texture
                 * coordinates, the raster position). A replay of a frame for the other eye repeats these calls with the matrix of that eye. A display list
                 * that contains one depends on the matrix (see GLListClassifier). Not in is_left_out_by_design().
                 */
                static bool bakes_current_matrix(const APIInterceptor::APIFunction& in_function);

                /* Returns true for the functions that set state which decides what a draw writes, or how its pixels combine with the existing ones, and
                 * which the model does not follow: the stencil test and its operations, the logic op, the blend equation, the separate blend factors, the
                 * blend constant color, and which face side is the front. A frame containing only such a draw could be mistaken for a frame that writes
                 * nothing and be skipped by a seek. The front end therefore ends the process on these functions (see report_unsupported_use()), so they
                 * are reported and not passed over. Not in is_left_out_by_design().
                 */
                static bool changes_what_a_draw_writes(const APIInterceptor::APIFunction& in_function);

                /* Returns true for a function that the front end does not follow and may leave out: the values for the vertices that follow, the state
                 * of the arrays that a draw reads, names, rendering state that the model does not use to tell draws apart, and the queries. Returns
                 * false for every other function: those are either followed (the front end does not ask this question for them) or end the process.
                 */
                static bool is_left_out_by_design(const APIInterceptor::APIFunction& in_function);

            private:
                /* Returns true for glGet*, glIs* and glAre*, which return what the program asks for and change nothing. */
                static bool is_query(const APIInterceptor::APIFunction& in_function);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_FUNCTION_POLICY_H */
