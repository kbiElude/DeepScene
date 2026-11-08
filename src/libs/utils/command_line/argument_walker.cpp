/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/command_line/argument_walker.h"

bool Utils::CommandLine::ArgumentWalker::walk(const int&         in_argc,
                                              char*              in_argv[],
                                              const HandlerFunc& in_handler_func,
                                              std::string*       out_error_ptr)
{
    bool result = true;

    for (int n_argument = 1;
             n_argument < in_argc;
           ++n_argument)
    {
        const ArgumentCursor cursor(in_argc,
                                    in_argv,
                                    n_argument);

        bool uses_next = false;

        if (in_handler_func(cursor,
                           &uses_next,
                            out_error_ptr) == false)
        {
            result = false;

            break;
        }

        n_argument += (uses_next) ? 1
                                  : 0;
    }

    return result;
}
