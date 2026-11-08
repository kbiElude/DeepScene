/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the command line of the programs that read a workload and write a file of what they found: the workload, -o, --max-frames, --progress-interval,
 * --skip-chunks-above, --stats and --threads are the same for all of them (see ExtractorOptions); a program that has options of its own is asked about each
 * argument that is not one of these before it is taken for an unknown option.
 */
#ifndef APIDUMPANALYSIS_EXTRACTOR_COMMAND_LINE_H
#define APIDUMPANALYSIS_EXTRACTOR_COMMAND_LINE_H

#include "analysis/api_dump_analysis/extractor_options.h"
#include <functional>
#include <stdint.h>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        /* What differs between the programs. */
        struct ExtractorProfile
        {
            uint32_t    default_skip_data_chunks_above_n_bytes = 0;
            std::string output_extension;                         /* with the dot: the name of the file that is written, when -o is not given, is the workload's with it */
        };

        class ExtractorCommandLine
        {
        public:
            /* What a program with options of its own says of an argument: whether it is one of them, whether it takes the argument after it as its value, and, if it is
             * one and what it was given is not right, why (the function returns false then).
             */
            typedef std::function<bool(const std::string& in_argument,
                                       const bool&        in_has_next,
                                       const std::string& in_next_argument,
                                       bool*              out_is_known_ptr,
                                       bool*              out_uses_next_ptr,
                                       std::string*       out_error_ptr)> ProgramOptionFunc;

            /* The name of the workload with the extension (a dot and what follows it, if that is in the last part of the name) taken off, and the one that is given put on. */
            static std::string get_default_output_filename(const std::string& in_input_filename,
                                                           const std::string& in_extension);

            /* False, and the reason, if the arguments are not right. The program's function may be empty. */
            static bool parse(const int&              in_argc,
                              char*                   in_argv[],
                              const ExtractorProfile& in_profile,
                              const ProgramOptionFunc& in_program_option_func,
                              ExtractorOptions*       out_options_ptr,
                              std::string*            out_error_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_EXTRACTOR_COMMAND_LINE_H */
