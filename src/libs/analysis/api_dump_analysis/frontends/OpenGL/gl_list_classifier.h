/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Classifies how the calls of a display list affect the state that differs per eye. The class decides whether a list that the driver compiled
 * once can be called for both eyes of a stereo frame (see GLListEyeDependence).
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
            class GLListClassifier
            {
            public:
                /* Classifies a call. A call that loads a matrix, or sets a projection, a viewport or a scissor box, is EYE_DEPENDENT: each eye needs its own
                 * values. A call that changes matrices relative to the current ones, or whose effect is relative to the current model-view matrix (a light,
                 * a clip plane), is RELATIVE_MATRIX_ONLY. Any other call is INDEPENDENT. A call of another list is not classified here: it takes the
                 * classification of the list it calls.
                 */
                static GLListEyeDependence classify_call(const APIInterceptor::APIFunction& in_function);

                /* Returns whichever of the two classes demands more of the replay (in order: an eye-dependent list, then one that moves the matrices, then the rest). */
                static GLListEyeDependence combine(const GLListEyeDependence& in_first,
                                                   const GLListEyeDependence& in_second);

            private:
                /* The more a replay has to do for a class, the higher its value. */
                static uint32_t get_rank(const GLListEyeDependence& in_dependence);
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_OPENGL_GL_LIST_CLASSIFIER_H */
