/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "overlay_probe_mode.h"
#include "frame_hash_sink.h"
#include "overlay_probe.h"
#include "replay_runner.h"
#include <windows.h>

bool Stereo::Replay::ConsistencyChecker::run_probe_shards(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                          const uint32_t&                                    in_n_shards,
                                                          const std::vector<std::string>&                    in_result_filename_vec,
                                                          std::string*                                       out_error_ptr)
{
    char                exe_path_i8vec[MAX_PATH] = {};
    std::vector<HANDLE> process_handle_vec;
    bool                result             = true;

    if (::GetModuleFileNameA(nullptr,
                             exe_path_i8vec,
                             MAX_PATH) == 0)
    {
        *out_error_ptr = "The path of this program cannot be found, to start the parts of the probe with.";
        result         = false;

        goto end;
    }

    /* A part is this program again, for its share of the candidates: its own process, with its own engine and the memory of it, which is what makes the parts
     * quick (a replay of the workload decodes it from its start, with threads of its own), and its output goes where this one's goes.
     */
    for (uint32_t n_shard = 0;
                  n_shard < in_n_shards;
                ++n_shard)
    {
        std::string         command_line        = "\"" + std::string(exe_path_i8vec) + "\" \"" + in_options.workload_filename + "\" --overlay-probe \"" +
                                                  in_options.overlay_probe_filename + "\" --recipe \"" + in_options.recipe_filename + "\" --dump-dir \"" +
                                                  in_options.dump_directory + "\" --jobs 1 --overlay-probe-shard " + std::to_string(n_shard) + " " + std::to_string(in_n_shards) +
                                                  " --overlay-probe-result \"" + in_result_filename_vec.at(n_shard) + "\"";
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

        startup_information.cb = sizeof(startup_information);

        /* The command line of CreateProcess is written to: it is a copy. */
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
            *out_error_ptr = "A part of the probe cannot be started (error " + std::to_string(::GetLastError() ) + ").";
            result         = false;

            break;
        }

        ::CloseHandle(process_information.hThread);

        process_handle_vec.push_back(process_information.hProcess);
    }

    /* Those that were started are waited for, whether or not the others could be. */
    for (const auto process_handle : process_handle_vec)
    {
        DWORD exit_code = 0;

        ::WaitForSingleObject(process_handle,
                              INFINITE);
        ::GetExitCodeProcess (process_handle,
                             &exit_code);
        ::CloseHandle        (process_handle);

        if (exit_code != 0)
        {
            *out_error_ptr = "A part of the probe failed (exit code " + std::to_string(exit_code) + "); what it said is above.";
            result         = false;
        }
    }

end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::take_overlay_probe(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                            std::string*                                       out_error_ptr)
{
    return Stereo::Replay::ConsistencyChecker::OverlayProbe::run(in_options,
                                                                 [](const Stereo::Replay::ConsistencyChecker::Options& in_probe_options,
                                                                    const uint32_t&                                    in_frame_index,
                                                                    Utils::Images::Image*                out_image_ptr,
                                                                    std::string*                                       out_error_ptr)
                                                                 {
                                                                     Stereo::Replay::ConsistencyChecker::FrameHashSink sink(true); /* in_keeps_images */

                                                                     const bool is_ok = replay_to_frame(in_probe_options,
                                                                                                        in_frame_index,
                                                                                                       &sink,
                                                                                                        out_error_ptr);

                                                                     if (is_ok)
                                                                     {
                                                                         *out_image_ptr = sink.get_image({in_frame_index, 0});
                                                                     }

                                                                     return is_ok;
                                                                 },
                                                                 [&in_options](const uint32_t&                 in_n_shards,
                                                                               const std::vector<std::string>& in_result_filename_vec,
                                                                               std::string*                    out_part_error_ptr)
                                                                 {
                                                                     return run_probe_shards(in_options,
                                                                                             in_n_shards,
                                                                                             in_result_filename_vec,
                                                                                             out_part_error_ptr);
                                                                 },
                                                                 out_error_ptr);
}
