/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/number_parser.h"
#include "Analysis/APIDumpSlicer/command_line.h"

const char* Analysis::APIDumpSlicer::CommandLine::get_usage_ptr()
{
    return "Usage: APIDumpSlicer in.workload slice.workload --frames B [--strip-chunks-above N] [--threads N]\n"
           "Writes the first frames of a workload, frames 0 to B (counted as APIDumpPrinter counts them), as a workload of its own. Frame B is its last.\n"
           "The slice replays to the same images as the workload does up to that frame. The segments of the file that are wholly in it are copied as\n"
           "they are, and the one that ends the slice is cut, so the slice is as exact as it can be.\n"
           "\n"
           "  --frames B                the last frame (0-B is the same: a slice has to start with the first frame, for what it needs from the frames\n"
           "                            before it)\n"
           "  --strip-chunks-above N    a data chunk of more than N bytes is written empty (the chunks keep their numbers): for analysing, not for a replay\n"
           "  --threads N               the threads that read the workload ahead (2 by default)\n"
           "\n"
           "The index of the workload (see APIDumpPrinter --index) is made, if there is none, and kept next to it.\n";
}

bool Analysis::APIDumpSlicer::CommandLine::parse(const std::vector<std::string>& in_argument_vec,
                                                 Options*                        out_options_ptr,
                                                 std::string*                    out_error_ptr)
{
    constexpr uint64_t MAX_N_THREADS = 64;

    bool has_frames = false;
    bool result     = false;

    *out_options_ptr = Options();

    for (size_t n_argument = 0;
                n_argument < in_argument_vec.size();
              ++n_argument)
    {
        const std::string& argument = in_argument_vec.at(n_argument);
        const bool         has_next = (n_argument + 1 < in_argument_vec.size() );

        if (argument == "--frames"              ||
            argument == "--strip-chunks-above"  ||
            argument == "--threads")
        {
            uint64_t    value = 0;
            std::string text;

            if (has_next == false)
            {
                *out_error_ptr = argument + " needs a value";

                goto end;
            }

            text = in_argument_vec.at(++n_argument);

            if (argument == "--frames")
            {
                /* "B", or "0-B". */
                const size_t n_dash = text.find('-');

                if (n_dash != std::string::npos)
                {
                    if (text.substr(0,
                                    n_dash) != "0")
                    {
                        *out_error_ptr = "A slice has to start with frame 0: what a frame needs is in the frames before it";

                        goto end;
                    }

                    text = text.substr(n_dash + 1);
                }

                if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(text,
                                                                            UINT32_MAX,
                                                                           &value) == false)
                {
                    *out_error_ptr = "--frames is a frame number, or 0-B";

                    goto end;
                }

                out_options_ptr->slice_options.last_frame = value;
                has_frames                                = true;
            }
            else
            if (argument == "--strip-chunks-above")
            {
                if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(text,
                                                                            UINT32_MAX,
                                                                           &value) == false)
                {
                    *out_error_ptr = "--strip-chunks-above is a number of bytes";

                    goto end;
                }

                out_options_ptr->slice_options.strip_chunks_above_n_bytes = static_cast<uint32_t>(value);
            }
            else
            {
                if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(text,
                                                                            MAX_N_THREADS,
                                                                           &value) == false)
                {
                    *out_error_ptr = "--threads is a number of threads, at most 64";

                    goto end;
                }

                out_options_ptr->slice_options.n_reader_threads = static_cast<uint32_t>(value);
            }
        }
        else
        if (argument.empty() == false &&
            argument.at(0)   == '-')
        {
            *out_error_ptr = "Unknown argument " + argument;

            goto end;
        }
        else
        if (out_options_ptr->workload_filename.empty() )
        {
            out_options_ptr->workload_filename = argument;
        }
        else
        if (out_options_ptr->slice_filename.empty() )
        {
            out_options_ptr->slice_filename = argument;
        }
        else
        {
            *out_error_ptr = "More than two files are given";

            goto end;
        }
    }

    if (out_options_ptr->slice_filename.empty() )
    {
        *out_error_ptr = "The workload and the file of the slice are needed";

        goto end;
    }

    if (has_frames == false)
    {
        *out_error_ptr = "--frames is needed";

        goto end;
    }

    if (out_options_ptr->slice_filename == out_options_ptr->workload_filename)
    {
        *out_error_ptr = "The slice cannot be written over the workload";

        goto end;
    }

    result = true;
end:
    return result;
}
