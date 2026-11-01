/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOCORE_TYPES_H
#define STEREOCORE_TYPES_H

#include "Common/types.h"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace Stereo
{
    namespace StereoCore
    {
        /* Called when the converter finds that the application is one it cannot convert right. It is not expected to return: what it
         * does instead (log and end the process, say) is the caller's to decide.
         */
        typedef std::function<void(const std::string& in_message)> FailFunc;

        /* Called to say what the converter did, for the log of the session. */
        typedef std::function<void(const std::string& in_message)> LogFunc;

        /* A matrix to put in the device before a draw. The function and the arguments are those of the call with which the application set
         * it, whose matrix argument is the one to replace; the caller makes the call again with this matrix.
         */
        struct MatrixChange
        {
            std::vector<APIInterceptor::APIFunctionArgument> argument_vec;
            APIInterceptor::APIFunction                      function          = APIInterceptor::APIFunction::APIFUNCTION_UNKNOWN;
            std::array<float, 16>                            matrix_fp32vec16  = {};
        };
    }
}

#endif /* STEREOCORE_TYPES_H */
