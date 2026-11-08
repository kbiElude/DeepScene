/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tells what the calls of a display list do to what an eye has of its own, which says whether a list that the driver made once can be called
 * for both eyes of a stereo frame (see GlListEyeDependence).
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_LIST_CLASSIFIER_H
#define APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_LIST_CLASSIFIER_H

#include "analysis/api_dump_analysis/frontends/OpenGL/types.h"
#include "common/types.h"

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class GlListClassifier
            {
            public:
                /* What a call does: the matrices that it loads, the projections, viewports and scissor boxes that it sets are the eye's own to change;
                 * those which it moves relative to the current ones are not, nor are those that are made relative to the current model-view matrix (a light, a
                 * clip plane); anything else does not touch them. A call of a list is not told here: it is what the called list is.
                 */
                static GlListEyeDependence classify_call(const APIInterceptor::APIFunction& in_function);

                /* The one of the two that asks more of the replay (an eye-dependent list, then one that moves the matrices, then the rest). */
                static GlListEyeDependence combine(const GlListEyeDependence& in_first,
                                                   const GlListEyeDependence& in_second);

            private:
                /* The more a replay has to do for it, the higher. */
                static uint32_t get_rank(const GlListEyeDependence& in_dependence);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_LIST_CLASSIFIER_H */
