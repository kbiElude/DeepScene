/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The steps shared by the programs that read a workload and write a file of what they found (APIDumpAnalysisExtractor,
 * APIDumpDependencyExtractor, APIDumpTextureExtractor): read the arguments and report errors, read the workload with a session while
 * printing progress, report if the workload ended in the middle of a segment, and then run the part that is specific to the program on what
 * the session found. A program supplies only its own part, as the steps below.
 */
#ifndef APIDUMPANALYSIS_EXTRACTOR_PROGRAM_RUNNER_H
#define APIDUMPANALYSIS_EXTRACTOR_PROGRAM_RUNNER_H

#include "analysis/api_dump_analysis/extractor_command_line.h"
#include "analysis/api_dump_analysis/extractor_options.h"
#include "analysis/api_dump_analysis/extractor_progress.h"
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "analysis/api_dump_analysis/types.h"
#include <functional>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class ExtractorProgramRunner
        {
        public:
            /* Runs after the workload is read: builds the output file, writes it and reports what was done. The progress object provides the elapsed time. */
            typedef std::function<ExtractorExitCode(const ExtractorOptions&     in_options,
                                                    Frontends::AnalysisSession* inout_session_ptr,
                                                    const ExtractorProgress&    in_progress)> AfterReadFunc;

            /* Runs after the arguments are read and before the workload is read, for example to open additional output files. */
            typedef std::function<ExtractorExitCode(const ExtractorOptions& in_options)> BeforeReadFunc;

            /* The steps specific to the program. A step the program does not need is left empty. */
            struct Steps
            {
                AfterReadFunc                           after_read_func;
                BeforeReadFunc                          before_read_func;
                ExtractorProgress::ExtraTextFunc        extra_text_func;
                std::function<void()>                   print_usage_func;
                ExtractorProfile                        profile;
                ExtractorCommandLine::ProgramOptionFunc program_option_func;
            };

            /* The program creates the session and gives it the sinks it needs. This function prints errors and the notes of the reading. */
            static ExtractorExitCode run(const int&                  in_argc,
                                         char*                       in_argv[],
                                         const Steps&                in_steps,
                                         Frontends::AnalysisSession* inout_session_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_EXTRACTOR_PROGRAM_RUNNER_H */
