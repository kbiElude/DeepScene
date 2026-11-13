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
                /* True for the functions that the front end does not follow, that are not of no matter, and that it lets through: what they do is made relative to the
                 * model-view matrix of the moment of the call (a position or a direction of a light, a clip plane, the planes of the generation of texture
                 * coordinates, the raster position). A replay of a frame for the other eye makes them again with the matrix of that eye, and a list that has one is
                 * relative to the matrix (see GlListClassifier). Not in is_left_out_by_design().
                 */
                static bool bakes_current_matrix(const APIInterceptor::APIFunction& in_function);

                /* True for the functions of the state that decides what a draw writes, or how its pixels are combined with what is there, which the model does not
                 * follow: the stencil test and what it does, the logic op, the equation, the separate factors and the constant color of blending, and which side of
                 * a face is the front. A frame that has only such a draw could be taken for one that writes nothing, and be left out by a seek; the front end ends the
                 * process on them (see report_unsupported_use()), so that they are said and not passed over. Not in is_left_out_by_design().
                 */
                static bool changes_what_a_draw_writes(const APIInterceptor::APIFunction& in_function);

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
