/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The command line of the replay check tool: its usage text and the parsing of its options.
 */
#ifndef STEREO_REPLAY_INSPECTOR_COMMAND_LINE_H
#define STEREO_REPLAY_INSPECTOR_COMMAND_LINE_H

#include "types.h"
#include "utils/images/types.h"
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace Inspector
        {
            class CommandLine
            {
            public:
                static const char* get_usage_ptr();

                /* The name of the image file of one frame when several frames are written: the number of the frame goes before the extension (x.png and 1800 give
                 * x_1800.png), or at the end if the name has no extension. */
                static std::string make_numbered_filename(const std::string& in_filename,
                                                          const uint32_t&    in_frame);

                /* Parses the arguments, which do not include the program name. Returns false, and says why, if they are invalid. */
                static bool parse(const std::vector<std::string>& in_argument_vec,
                                  Options*                        out_options_ptr,
                                  std::string*                    out_error_ptr);

            private:
                /* Checks the screenshot and contact sheet options. A screenshot and a contact sheet cannot be combined. Either one needs an output image
                 * file, and an eye view needs a recipe. Without a screenshot or a contact sheet, no image file or eye view may be given. */
                static bool are_image_options_valid(const Options& in_options);

                /* Converts a name (left, right, both or anaglyph) to an eye view. Returns false for any other name. */
                static bool parse_eye_view(const std::string&      in_name,
                                           Utils::Images::EyeView* out_view_ptr);

                /* Parses the frames of --screenshot: numbers, ranges A-B and ranges with a step A-B:S, separated by commas. The frames come out in order, without repeats.
                 * Returns false, and says why, for any other text or for more than N_FRAMES_MOST frames. */
                static bool parse_frame_list(const std::string&    in_text,
                                             std::vector<uint32_t>* out_frame_vec_ptr,
                                             std::string*          out_error_ptr);

                /* Parses one item of that list: a number, A-B, or A-B:S. Returns false for any other text. */
                static bool parse_frame_item(const std::string& in_text,
                                             uint32_t*          out_first_ptr,
                                             uint32_t*          out_last_ptr,
                                             uint32_t*          out_step_ptr);

                /* Parses a whole number between the least and the most. Returns false, and leaves the output unchanged, for any other text. */
                static bool parse_unsigned(const std::string& in_text,
                                           const uint64_t&    in_least,
                                           const uint64_t&    in_most,
                                           uint32_t*          out_value_ptr);

                /* Splits a comma-separated list into its items. An empty item counts as an item. */
                static std::vector<std::string> split_list(const std::string& in_list);
            };
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_COMMAND_LINE_H */
