/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef RECIPEBAKE_COMMAND_LINE_H
#define RECIPEBAKE_COMMAND_LINE_H

#include "types.h"
#include <string>
#include <vector>

namespace Analysis
{
    namespace RecipeBake
    {
        class CommandLine
        {
        public:
            /* Parses the arguments, which do not include the program name. Returns false, and says why, if they are invalid. */
            static bool parse(const std::vector<std::string>& in_argument_vec,
                              Options*                        out_options_ptr,
                              std::string*                    out_error_ptr);

            static const char* get_usage_ptr();
        };
    }
}

#endif /* RECIPEBAKE_COMMAND_LINE_H */
