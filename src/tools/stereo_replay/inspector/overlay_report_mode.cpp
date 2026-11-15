/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "overlay_report_mode.h"
#include "analysis/api_dump_dependencies/deps_file.h"
#include "frame_hash_sink.h"
#include "frame_repeatability.h"
#include "overlay_report.h"
#include "replay_runner.h"
#include <windows.h>

bool Stereo::Replay::Inspector::run_report_shards(const Stereo::Replay::Inspector::Options& in_options,
                                                 const uint32_t&                                    in_n_shards,
                                                 const std::vector<std::string>&                    in_result_filename_vec,
                                                 std::string*                                       out_error_ptr)
{
    char                exe_path_i8vec[MAX_PATH] = {};
    std::vector<HANDLE> process_handle_vec;
    bool                result             = true;

    /* Waits for the first running part to finish, to make room for another part (or at the end). Its exit code says whether it succeeded. */
    const auto wait_for_part_func = [&]()
    {
        DWORD exit_code = 0;
        DWORD n_signaled = ::WaitForMultipleObjects(static_cast<DWORD>(process_handle_vec.size() ),
                                                    process_handle_vec.data(),
                                                    FALSE, /* in_wait_all */
                                                    INFINITE) - WAIT_OBJECT_0;

        ::GetExitCodeProcess(process_handle_vec.at(n_signaled),
                            &exit_code);
        ::CloseHandle       (process_handle_vec.at(n_signaled) );

        process_handle_vec.erase(process_handle_vec.begin() + n_signaled);

        if (exit_code != 0)
        {
            *out_error_ptr = "A part of the report failed (exit code " + std::to_string(exit_code) + "); what it said is above.";
            result         = false;
        }
    };

    if (::GetModuleFileNameA(nullptr,
                             exe_path_i8vec,
                             MAX_PATH) == 0)
    {
        *out_error_ptr = "The path of this program cannot be found, to start the parts of the report with.";
        result         = false;

        goto end;
    }

    /* A part is this program started again for its share of the candidates. It is a separate process with its own engine and memory, which is
     * what makes the parts fast: every replay decodes the workload from its start, with threads of its own. The output of a part goes where
     * the output of this process goes.
     */
    for (uint32_t n_shard = 0;
                  n_shard < in_n_shards;
                ++n_shard)
    {
        /* At most as many parts run at a time as there are jobs. When a part fails, no further parts are started. */
        while (process_handle_vec.size() >= in_options.n_jobs)
        {
            wait_for_part_func();
        }

        if (result == false)
        {
            break;
        }

        std::string         command_line        = "\"" + std::string(exe_path_i8vec) + "\" \"" + in_options.workload_filename + "\" --overlay-report \"" +
                                                  in_options.overlay_report_filename + "\" --recipe \"" + in_options.recipe_filename + "\" --dump-dir \"" +
                                                  in_options.dump_directory + "\" --jobs 1 --overlay-report-shard " + std::to_string(n_shard) + " " + std::to_string(in_n_shards) +
                                                  " --overlay-report-result \"" + in_result_filename_vec.at(n_shard) + "\"";
        PROCESS_INFORMATION process_information = {};
        STARTUPINFOA        startup_information = {};

        if (in_options.deps_filename.empty() == false)
        {
            command_line += " --deps \"" + in_options.deps_filename + "\"";
        }

        if (in_options.eye_separation != 0.0f)
        {
            command_line += " --eye-separation " + std::to_string(in_options.eye_separation);
        }

        if (in_options.has_history)
        {
            command_line += " --history";
        }

        if (in_options.overlay_report_is_exact)
        {
            command_line += " --overlay-report-exact";
        }

        if (in_options.overlay_report_last_frame != UINT32_MAX)
        {
            command_line += " --overlay-report-last-frame " + std::to_string(in_options.overlay_report_last_frame);
        }

        startup_information.cb = sizeof(startup_information);

        /* CreateProcess may modify the command line buffer, so it gets a copy. */
        std::vector<char> command_line_i8_vec(command_line.begin(),
                                              command_line.end  ());

        command_line_i8_vec.push_back('\0');

        if (::CreateProcessA(nullptr,
                             command_line_i8_vec.data(),
                             nullptr,
                             nullptr,
                             TRUE, /* in_inherit_handles: the part writes where this one does */
                             0,
                             nullptr,
                             nullptr,
                            &startup_information,
                            &process_information) == FALSE)
        {
            *out_error_ptr = "A part of the report cannot be started (error " + std::to_string(::GetLastError() ) + ").";
            result         = false;

            break;
        }

        ::CloseHandle(process_information.hThread);

        process_handle_vec.push_back(process_information.hProcess);
    }

    /* Parts that were started are waited for, even if starting another part failed. */
    while (process_handle_vec.empty() == false)
    {
        wait_for_part_func();
    }

end:
    return result;
}

bool Stereo::Replay::Inspector::take_overlay_report(const Stereo::Replay::Inspector::Options& in_options,
                                                   std::string*                                       out_error_ptr)
{
    const auto render_frame_func = [](const Stereo::Replay::Inspector::Options& in_probe_options,
                                      const uint32_t&                                    in_frame_index,
                                      Utils::Images::Image*                              out_image_ptr,
                                      std::string*                                       out_error_ptr)
    {
        Stereo::Replay::Inspector::FrameHashSink sink(true); /* in_keeps_images */

        const bool is_ok = replay_to_frame(in_probe_options,
                                           in_frame_index,
                                          &sink,
                                           out_error_ptr);

        if (is_ok)
        {
            *out_image_ptr = sink.get_image({in_frame_index, 0});
        }

        return is_ok;
    };

    const auto merge_parts_func = [&in_options](const uint32_t&                 in_n_shards,
                                                const std::vector<std::string>& in_result_filename_vec,
                                                std::string*                    out_part_error_ptr)
    {
        return run_report_shards(in_options,
                                in_n_shards,
                                in_result_filename_vec,
                                out_part_error_ptr);
    };

    /* The quick path needs the dependencies, which say which frames can be played again from the state they leave. Without them, or if they
     * cannot be read, each candidate is replayed on its own.
     */
    Analysis::APIDumpDependencies::DependencyAnalysis analysis;
    std::string                                       deps_error;
    bool                                              has_deps = false;
    Stereo::Replay::Inspector::FrameVariantReplayPlan frame_variant_replay_plan;

    if (in_options.overlay_report_is_exact == false &&
        in_options.deps_filename.empty()   == false)
    {
        has_deps = Analysis::APIDumpDependencies::DepsFile::read(in_options.deps_filename,
                                                                &analysis,
                                                                &deps_error);

        if (has_deps == false)
        {
            fprintf(stdout,
                    "The dependencies cannot be read for the quick path (%s); each candidate is replayed on its own.\n",
                    deps_error.c_str() );
        }
    }

    frame_variant_replay_plan.is_repeatable_func   = [&analysis](const uint32_t& in_frame_index,
                                                                 std::string*    out_reason_ptr)
    {
        return Stereo::Replay::Inspector::FrameRepeatability::is_repeatable(analysis,
                                                                            in_frame_index,
                                                                            out_reason_ptr);
    };
    frame_variant_replay_plan.render_variants_func = replay_variants_to_frame;

    return Stereo::Replay::Inspector::OverlayReport::run(in_options,
                                                        render_frame_func,
                                                        merge_parts_func,
                                                        out_error_ptr,
                                                        (has_deps) ? &frame_variant_replay_plan
                                                                   : nullptr);
}
