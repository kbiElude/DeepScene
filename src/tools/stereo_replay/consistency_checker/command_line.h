/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The command line of the replay check tool: the options which it is given, and what it says of them.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_COMMAND_LINE_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_COMMAND_LINE_H

#include "types.h"
#include "utils/images/types.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            class CommandLine
            {
            public:
                static const char* get_usage_ptr();

                /* Returns false, and says why, if the arguments are not right. The program name is not among them. */
                static bool parse(const std::vector<std::string>& in_argument_vec,
                                  Options*                        out_options_ptr,
                                  std::string*                    out_error_ptr);

            private:
                /* False if the options of a screenshot or a contact sheet are given without it, or it without the file to write, or both at once. */
                static bool are_image_options_valid(const Options& in_options);

                /* The view of the eyes that the name stands for: left, right, both or anaglyph. False for any other name. */
                static bool parse_eye_view(const std::string&      in_name,
                                           Utils::Images::EyeView* out_view_ptr);

                /* A whole number from the least to the most. False, and the output left alone, for any other text. */
                static bool parse_unsigned(const std::string& in_text,
                                           const uint64_t&    in_least,
                                           const uint64_t&    in_most,
                                           uint32_t*          out_value_ptr);

                /* The items of a list which is written with commas between them (an empty item is one too). */
                static std::vector<std::string> split_list(const std::string& in_list);
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_COMMAND_LINE_H */
