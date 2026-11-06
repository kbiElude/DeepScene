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
            /* Returns false, and says why, if the arguments are not right. The program name is not among them. */
            static bool parse(const std::vector<std::string>& in_argument_vec,
                              Options*                        out_options_ptr,
                              std::string*                    out_error_ptr);

            static const char* get_usage_ptr();
        };
    }
}

#endif /* RECIPEBAKE_COMMAND_LINE_H */
