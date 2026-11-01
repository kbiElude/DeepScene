/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_PRESENTER_COMMAND_LINE_H
#define STEREO_PRESENTER_COMMAND_LINE_H

#include "Stereo/Presenter/types.h"
#include <string>
#include <vector>

namespace Stereo
{
    namespace Presenter
    {
        class CommandLine
        {
        public:
            /* Returns false, and says why, if the arguments are not right. The program name is not among them. */
            static bool parse(const std::vector<std::string>& in_argument_vec,
                              Options*                        out_options_ptr,
                              std::string*                    out_error_ptr);

            static const char* get_usage_ptr();
        };
    }
}

#endif /* STEREO_PRESENTER_COMMAND_LINE_H */
