/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_GL_RUN_CALL_CLASSIFIER_H
#define STEREO_CONVERTERDLL_GL_RUN_CALL_CLASSIFIER_H

#include "common/api_function_argument.h"
#include "types.h"
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Says what the right eye does about a call of the application while a run is kept (see GLRunRecorder): keep it, leave it, or play the run first.
         * Only what is listed in the class is kept or left; every other call of OpenGL plays the run first, so a call that the converter does not know cannot
         * change what the right eye sees without the run having been played before it.
         */
        class GLRunCallClassifier
        {
        public:
            static RunCallClass classify(const APIInterceptor::APIFunction& in_function);

            /* Reads the numbers of a call that classify() says is RECORDED. Returns false if its arguments are not numbers. */
            static bool read_state_call(const APIInterceptor::APIFunction&         in_function,
                                        const uint32_t&                            in_n_arguments,
                                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                        RunStateCall*                              out_call_ptr);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_GL_RUN_CALL_CLASSIFIER_H */
