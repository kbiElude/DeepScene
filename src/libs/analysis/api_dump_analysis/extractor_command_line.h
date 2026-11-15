/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Reads the command line of the programs that read a workload and write a file of what they found. The workload, -o, --max-frames,
 * --progress-interval, --skip-chunks-above, --stats and --threads are the same for all of them (see ExtractorOptions). A program with options
 * of its own is asked about each argument that is not one of these before the argument is rejected as an unknown option.
 */
#ifndef APIDUMPANALYSIS_EXTRACTOR_COMMAND_LINE_H
#define APIDUMPANALYSIS_EXTRACTOR_COMMAND_LINE_H

#include "analysis/api_dump_analysis/extractor_options.h"
#include "utils/command_line/program_option_func.h"
#include <functional>
#include <stdint.h>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        /* The parts that differ between the programs. */
        struct ExtractorProfile
        {
            uint32_t    default_skip_data_chunks_above_n_bytes = 0;
            std::string output_extension;                         /* with the dot; without -o the output name is the workload name with this extension                    */
        };

        class ExtractorCommandLine
        {
        public:
            /* How a program with options of its own handles an argument. */
            typedef Utils::CommandLine::ProgramOptionFunc ProgramOptionFunc;

            /* Returns the workload name with its extension replaced by the given one. The extension is the last dot and what follows it, if the dot is in the file name part. */
            static std::string get_default_output_filename(const std::string& in_input_filename,
                                                           const std::string& in_extension);

            /* Returns false and sets the reason if an argument is invalid. The program's function may be empty. */
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
