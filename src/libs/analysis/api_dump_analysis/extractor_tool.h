/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the programs that read a workload and write a file of what they found (APIDumpAnalysisExtractor, APIDumpDependencyExtractor, APIDumpTextureExtractor) do the
 * same way: read the arguments (and say what is wrong with them), read the workload with a session while the progress is told, say that it ended in the middle of a
 * segment if it did, and then do what the program is for with what the session found. A program says only what is its own, as the steps.
 */
#ifndef APIDUMPANALYSIS_EXTRACTOR_TOOL_H
#define APIDUMPANALYSIS_EXTRACTOR_TOOL_H

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
        class ExtractorTool
        {
        public:
            /* What the program does with what the session found: makes its file, writes it, and says what it did. The progress is there for the time that went by. */
            typedef std::function<ExtractorExitCode(const ExtractorOptions&     in_options,
                                                    Frontends::AnalysisSession* inout_session_ptr,
                                                    const ExtractorProgress&    in_progress)> AfterReadFunc;

            /* What the program does once the arguments are read, before the workload is: opens what it writes beside its file, for one. */
            typedef std::function<ExtractorExitCode(const ExtractorOptions& in_options)> BeforeReadFunc;

            /* What is the program's own. The functions that say nothing are left empty. */
            struct Steps
            {
                AfterReadFunc                           after_read_func;
                BeforeReadFunc                          before_read_func;
                ExtractorProgress::ExtraTextFunc        extra_text_func;
                std::function<void()>                   print_usage_func;
                ExtractorProfile                        profile;
                ExtractorCommandLine::ProgramOptionFunc program_option_func;
            };

            /* The session is the program's, made and given its sinks as the program needs. Errors are printed here, and the notes of the reading. */
            static ExtractorExitCode run(const int&                  in_argc,
                                         char*                       in_argv[],
                                         const Steps&                in_steps,
                                         Frontends::AnalysisSession* inout_session_ptr);
        };
    }
}

#endif /* APIDUMPANALYSIS_EXTRACTOR_TOOL_H */
