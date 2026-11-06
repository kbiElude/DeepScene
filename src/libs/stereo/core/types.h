/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_TYPES_H
#define STEREO_CORE_TYPES_H

#include "common/api_function_argument.h"
#include "common/types.h"
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Core
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

        /* What the device has of a matrix, as far as the slots know. */
        enum class MatrixSlotBinding
        {
            LEFT,
            MONO,
            NOTHING, /* the matrix was set by the application, and has not been put in the device since */
            OVERLAY_LEFT,
            OVERLAY_RIGHT,
            RIGHT
        };

        /* How the x of the vertices are moved. */
        enum class ScreenShiftKind
        {
            NONE,       /* as they are                                                              */
            PER_VERTEX, /* each by its depth, with the distances below (the draw is part of the scene) */
            UNIFORM     /* all by the same pixels (the draw is pushed toward the viewer as a whole)    */
        };
    }
}

#endif /* STEREO_CORE_TYPES_H */
