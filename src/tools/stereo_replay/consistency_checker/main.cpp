/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/command_line/number_parser.h"
#include "analysis/recipe/recipe_loader.h"
#include "Capture/APIDumpLoader/WorkloadIndex.h"
#include "Capture/APIDumpNames/function_names.h"
#include "tools/stereo_replay/engine_dll/entrypoints.h"
#include "utils/images/image_comparer.h"
#include "utils/images/image_composer.h"
#include "utils/images/png_writer.h"
#include "frame_hash_sink.h"
#include "frame_sampler.h"
#include "overlay_probe.h"
#include "replay_profiler.h"
#include "stop_observer.h"
#include "types.h"
#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <vector>
#include <windows.h>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            extern const Variant VARIANT_ARRAY[5];

            /* False if the options of a screenshot or a contact sheet are given without it, or it without the file to write, or both at once. */
            bool are_image_options_valid(const Options& in_options);

            /* Replays the frames of the options from the workload of the options and from the other workload, as they are, and says whether the images are the same.
             * Returns false if a replay fails. A slice of the workload (frames 0 to B) is the same as the workload for the frames up to B.
             */
            bool compare_workloads(const Options& in_options,
                                   bool*          out_has_difference_ptr,
                                   std::string*   out_error_ptr);

            /* Writes the images of one frame, as it is and executed twice, to the directory of the options. */
            void dump_frame(const Options&  in_options,
                            const uint32_t& in_n_frame,
                            const Variant&  in_variant);

            /* The way of replaying that the name stands for (see print_usage), or nullptr. */
            const Variant* get_variant_ptr(const std::string& in_way_name);

            /* Says how the executions of a way of replaying differ from the frames as they are. Returns the first frame in which one of the
             * executions differs, and false if there is none.
             */
            bool print_comparison(const Variant&                                     in_variant,
                                  const std::map<FrameHashSink::ImageKey, uint64_t>& in_baseline_map,
                                  const std::map<FrameHashSink::ImageKey, uint64_t>& in_hash_map,
                                  uint32_t*                                          out_first_frame_ptr);

            void print_usage();

            /* Replays the frames of the options one way, and keeps what the sink is given. */
            /* Replays the workload up to the frame (with the frames before it, if the options say so), with the recipe of the options if they have one, so that
             * the sink has the image of the frame. The frame is the only one the sink keeps.
             */
            bool replay_to_frame(const Options&  in_options,
                                 const uint32_t& in_frame_index,
                                 FrameHashSink*  out_sink_ptr,
                                 std::string*    out_error_ptr);

            /* Starts the parts of an overlay probe, each as a process of this program, and waits for them (see OverlayProbe::RunShardsFunc). */
            bool run_probe_shards(const Options&                    in_options,
                                  const uint32_t&                   in_n_shards,
                                  const std::vector<std::string>&   in_result_filename_vec,
                                  std::string*                      out_error_ptr);

            bool run_variant(const Options&                                  in_options,
                             const Variant&                                  in_variant,
                             FrameHashSink*                                  out_sink_ptr,
                             std::string*                                    out_error_ptr,
                             ::Replay::ReplayEngineDLL::ICallInterceptor*    in_interceptor_ptr = nullptr);

            /* Replays the frames the ways in the vector, one after the other or all at once, and prints how long each took. The sinks are in the order of
             * the ways.
             */
            bool run_variants(const Options&                      in_options,
                              const std::vector<const Variant*>&  in_variant_ptr_vec,
                              std::vector<FrameHashSink>*         out_sink_vec_ptr,
                              std::string*                        out_error_ptr);

            /* Replays the workload once, up to the last of the frames that the options have a contact sheet of, and writes the sheet: the frames spread evenly
             * from the first frame of the options to the last (or the end of the workload), each as a screenshot is.
             */
            bool take_contact_sheet(const Options& in_options,
                                    std::string*   out_error_ptr);

            /* Replays the frames of the options once (with the recipe, if there is one) and says where the time of the calls went: the functions that took
             * longest, and the frames.
             */
            bool take_profile(const Options& in_options,
                              std::string*   out_error_ptr);

            /* Makes the probe of the overlay candidates of the options (see OverlayProbe). */
            bool take_overlay_probe(const Options& in_options,
                                    std::string*   out_error_ptr);

            /* Replays the workload once, up to the screenshot frame, and writes the image of it to the screenshot file: with a recipe the eyes as the options
             * say, otherwise the frame as it is.
             */
            bool take_screenshot(const Options& in_options,
                                 std::string*   out_error_ptr);

            /* Says, and writes an image of, how the image of an execution differs from the one of the frame as it is. */
            void write_difference(const std::string&                        in_label,
                                  const std::string&                        in_path,
                                  const Utils::Images::Image& in_image_as_it_is,
                                  const Utils::Images::Image& in_image_of_execution);
        }
    }
}

const Stereo::Replay::ConsistencyChecker::Variant Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[] =
{
    {Stereo::Replay::EngineDLL::FrameExecution::DIRECT, "as they are",                       false, false},
    {Stereo::Replay::EngineDLL::FrameExecution::DIRECT, "as they are, again",                false, false},
    {Stereo::Replay::EngineDLL::FrameExecution::TWICE,  "twice",                             false, false},
    {Stereo::Replay::EngineDLL::FrameExecution::TWICE,  "twice, the state put back between", true,  false},
    {Stereo::Replay::EngineDLL::FrameExecution::TWICE,  "twice, for the eyes of the recipe", true,  true}
};

bool Stereo::Replay::ConsistencyChecker::are_image_options_valid(const Stereo::Replay::ConsistencyChecker::Options& in_options)
{
    const bool is_contact_sheet = (in_options.contact_sheet_n_frames != 0);
    const bool is_screenshot    = (in_options.screenshot_frame       != UINT32_MAX);
    bool       result           = false;

    if (is_contact_sheet &&
        is_screenshot)
    {
        goto end;
    }

    if (is_contact_sheet ||
        is_screenshot)
    {
        result = (in_options.image_filename.empty() == false &&
                  (in_options.has_eye_view == false          ||
                   in_options.recipe_filename.empty() == false) );
    }
    else
    {
        result = (in_options.image_filename.empty() &&
                  in_options.has_eye_view == false);
    }

end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::compare_workloads(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                           bool*                                              out_has_difference_ptr,
                                                           std::string*                                       out_error_ptr)
{
    constexpr size_t N_FRAMES_LISTED = 20;

    std::vector<uint32_t>                       different_frame_vec;
    uint32_t                                    n_different_frames  = 0;
    Stereo::Replay::ConsistencyChecker::Options other_options       = in_options;
    Stereo::Replay::ConsistencyChecker::FrameHashSink other_sink(false); /* in_keeps_images */
    Stereo::Replay::ConsistencyChecker::FrameHashSink own_sink  (false); /* in_keeps_images */
    bool                                              result             = false;

    other_options.workload_filename = in_options.same_frames_workload_filename;

    if (run_variant(in_options,
                    VARIANT_ARRAY[0],
                   &own_sink,
                    out_error_ptr) == false)
    {
        goto end;
    }

    if (run_variant(other_options,
                    VARIANT_ARRAY[0],
                   &other_sink,
                    out_error_ptr) == false)
    {
        goto end;
    }

    for (const auto& own_pair : own_sink.get_hash_map() )
    {
        const auto other_iterator = other_sink.get_hash_map().find(own_pair.first);
        const bool is_same        = (other_iterator         != other_sink.get_hash_map().end() &&
                                     other_iterator->second == own_pair.second);

        if (is_same)
        {
            continue;
        }

        n_different_frames++;

        if (different_frame_vec.size() < N_FRAMES_LISTED)
        {
            different_frame_vec.push_back(own_pair.first.first);
        }
    }

    fprintf(stdout,
            "Frames %u to %u, as they are, of %s and of %s: %zu images of the first, %zu of the second; %u of the first differ from the second or are not in it.\n",
            in_options.first_frame,
            in_options.last_frame,
            in_options.workload_filename.c_str(),
            in_options.same_frames_workload_filename.c_str(),
            own_sink.get_hash_map().size(),
            other_sink.get_hash_map().size(),
            n_different_frames);

    if (different_frame_vec.empty() == false)
    {
        fprintf(stdout,
                "  first frames that differ:");

        for (const auto n_frame : different_frame_vec)
        {
            fprintf(stdout,
                    " %u",
                    n_frame);
        }

        fprintf(stdout,
                "\n");
    }

    *out_has_difference_ptr = (n_different_frames                 != 0 ||
                               own_sink.get_hash_map().size()     != other_sink.get_hash_map().size() );

    result = true;
end:
    return result;
}

/* Writes the images of one frame, as it is and executed twice, to the directory of the options. */
void Stereo::Replay::ConsistencyChecker::dump_frame(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                    const uint32_t&                                    in_n_frame,
                                                    const Variant&                                     in_variant)
{
    Stereo::Replay::ConsistencyChecker::FrameHashSink direct_sink(true); /* in_keeps_images */
    std::string                                 error;
    Stereo::Replay::ConsistencyChecker::Options options = in_options;
    Stereo::Replay::ConsistencyChecker::FrameHashSink twice_sink(true);  /* in_keeps_images */

    options.first_frame = in_n_frame;
    options.last_frame  = in_n_frame;

    if (run_variant(options,
                    VARIANT_ARRAY[0],
                   &direct_sink,
                   &error) == false)
    {
        fprintf(stdout,
                "The images of frame %u could not be written: %s\n",
                in_n_frame,
                error.c_str() );

        goto end;
    }

    if (run_variant(options,
                    in_variant,
                   &twice_sink,
                   &error) == false)
    {
        fprintf(stdout,
                "The images of frame %u could not be written: %s\n",
                in_n_frame,
                error.c_str() );

        goto end;
    }

    {
        const std::string prefix = in_options.dump_directory + "/frame_" + std::to_string(in_n_frame);

        const auto& as_it_is_image         = direct_sink.get_image({in_n_frame, 0});
        const auto& first_execution_image  = twice_sink.get_image({in_n_frame, 0});
        const auto& second_execution_image = twice_sink.get_image({in_n_frame, 1});

        Utils::Images::PngWriter::write_file(prefix + "_as_it_is.png",
                                             as_it_is_image);
        Utils::Images::PngWriter::write_file(prefix + "_first_execution.png",
                                             first_execution_image);
        Utils::Images::PngWriter::write_file(prefix + "_second_execution.png",
                                             second_execution_image);

        write_difference("first execution",
                         prefix + "_first_execution_difference.png",
                         as_it_is_image,
                         first_execution_image);
        write_difference("second execution",
                         prefix + "_second_execution_difference.png",
                         as_it_is_image,
                         second_execution_image);

        fprintf(stdout,
                "Images of frame %u written to %s\n",
                in_n_frame,
                in_options.dump_directory.c_str() );
    }

end:
    return;
}

/* Says how the executions of a way of replaying differ from the frames as they are. Returns the first frame in which one of the
 * executions differs, and false if there is none.
 */
const Stereo::Replay::ConsistencyChecker::Variant* Stereo::Replay::ConsistencyChecker::get_variant_ptr(const std::string& in_way_name)
{
    const Variant* result_ptr = nullptr;

    if (in_way_name == "again")
    {
        result_ptr = &VARIANT_ARRAY[1];
    }
    else
    if (in_way_name == "twice")
    {
        result_ptr = &VARIANT_ARRAY[2];
    }
    else
    if (in_way_name == "restore")
    {
        result_ptr = &VARIANT_ARRAY[3];
    }
    else
    if (in_way_name == "recipe")
    {
        result_ptr = &VARIANT_ARRAY[4];
    }

    return result_ptr;
}

bool Stereo::Replay::ConsistencyChecker::print_comparison(const Variant&                                                                         in_variant,
                                                          const std::map<Stereo::Replay::ConsistencyChecker::FrameHashSink::ImageKey, uint64_t>& in_baseline_map,
                                                          const std::map<Stereo::Replay::ConsistencyChecker::FrameHashSink::ImageKey, uint64_t>& in_hash_map,
                                                          uint32_t*                                                                              out_first_frame_ptr)
{
    constexpr size_t N_FRAMES_LISTED = 20;

    bool                  has_difference      = false;
    uint32_t              n_executions_differ = 0;
    uint32_t              n_first_differs     = 0;
    uint32_t              n_second_differs    = 0;
    std::vector<uint32_t> first_frame_vec;

    for (const auto& baseline_pair : in_baseline_map)
    {
        const uint32_t n_frame = baseline_pair.first.first;
        const auto     first   = in_hash_map.find({n_frame, 0});
        const auto     second  = in_hash_map.find({n_frame, 1});

        /* An image which is not there is a difference, except for the second execution of a way which has one only. */
        const bool first_differs  = (first           == in_hash_map.end()                              ||
                                     first->second   != baseline_pair.second);
        const bool second_differs = (in_variant.mode == Stereo::Replay::EngineDLL::FrameExecution::TWICE &&
                                     (second         == in_hash_map.end()                          ||
                                      second->second != baseline_pair.second) );

        if (first_differs)
        {
            ++n_first_differs;

            if (first_frame_vec.size() < N_FRAMES_LISTED)
            {
                first_frame_vec.push_back(n_frame);
            }
        }

        if (second_differs)
        {
            ++n_second_differs;
        }

        if (first         != in_hash_map.end() &&
            second        != in_hash_map.end() &&
            first->second != second->second)
        {
            ++n_executions_differ;
        }

        if (first_differs  ||
            second_differs)
        {
            if (has_difference == false)
            {
                *out_first_frame_ptr = n_frame;
            }

            has_difference = true;
        }
    }

    fprintf(stdout,
            "\n%s, against the frames as they are (%zu frames):\n"
            "  first execution differs in %u frames\n",
            in_variant.name_ptr,
            in_baseline_map.size(),
            n_first_differs);

    if (in_variant.mode == Stereo::Replay::EngineDLL::FrameExecution::TWICE)
    {
        fprintf(stdout,
                "  second execution differs in %u frames\n"
                "  the two executions differ from each other in %u frames\n",
                n_second_differs,
                n_executions_differ);
    }

    if (first_frame_vec.empty() == false)
    {
        fprintf(stdout,
                "  first frames in which the first execution differs:");

        for (const auto n_frame : first_frame_vec)
        {
            fprintf(stdout,
                    " %u",
                    n_frame);
        }

        fprintf(stdout,
                "\n");
    }

    return has_difference;
}

void Stereo::Replay::ConsistencyChecker::print_usage()
{
    fprintf(stdout,
            "Usage:\n"
            "\n"
            "StereoReplayConsistencyChecker_x86 file.workload [--from A] [--to B] [--dump-dir DIR] [--recipe FILE [--eye-separation D]]\n"
            "                                    [--ways again,twice,restore,recipe] [--parallel]\n"
            "StereoReplayConsistencyChecker_x86 file.workload --profile [--from A] [--to B] [--recipe FILE]\n"
            "StereoReplayConsistencyChecker_x86 file.workload --same-frames-as other.workload [--from A] [--to B]\n"
            "StereoReplayConsistencyChecker_x86 file.workload --screenshot N --out FILE.png [--recipe FILE [--eye left|right|both|anaglyph]]\n"
            "StereoReplayConsistencyChecker_x86 file.workload --overlay-probe candidates.json --recipe FILE --dump-dir DIR [--deps FILE] [--eye-separation D]\n"
            "StereoReplayConsistencyChecker_x86 file.workload --contact-sheet K --out FILE.png [--from A] [--to B] [--recipe FILE [--eye ...]]\n"
            "\n"
            "Replays frames A to B (default 0 to 999) of a Direct3D 8 workload in four ways: as they are, as they are again (to\n"
            "see how much a replay differs from itself), executed twice, and executed twice with the state of the device put\n"
            "back before the second execution. The back buffer after each execution is compared with that of the first replay.\n"
            "\n"
            "  --deps FILE      The dependencies of the workload (a .deps file of it, see APIDumpDependencyExtractor): each way goes to the last\n"
            "                   frame as a seek does, leaving out the draws and uploads of the frames which nothing after it reads. For checking\n"
            "                   that it comes out as the replay of every frame does (take two screenshots, with and without, and compare them).\n"
            "  --dump-dir DIR   Write the images of the first frame in which an execution differs, as PNG files\n"
            "  --eye-separation D  The distance between the eyes for the recipe (0 by default)\n"
            "  --recipe FILE    Replay the frames twice with the recipe instead of the last two ways: the first execution is for\n"
            "                   the left eye, the second for the right one. A recipe that changes nothing gives the frames as they are.\n"
            "  --ways LIST      Only these ways, besides the frames as they are (which are always replayed, to compare with): again (as\n"
            "                   they are, again), twice, restore (twice, the state put back between) and recipe (needs --recipe). Each way\n"
            "                   is a replay of the workload from its start, so fewer ways are less to wait for.\n"
            "  --overlay-probe FILE  Instead of the check: FILE is the list of draws that may be 2D overlays (RecipeBake --overlays). The frame in which each is first\n"
            "                   drawn is replayed for the left eye with the draw and without it (the recipe skips it in its pass only); the images go to --dump-dir: an\n"
            "                   animation (GIF) that flips between the frame and the frame with the pixels the draw makes in magenta, and a still, for each candidate that\n"
            "                   makes any, candidates.json with the number of them, and index.html to look at them. A pass which makes no difference left out whole is\n"
            "                   left out for all its draws at once.\n"
            "  --jobs N         With --overlay-probe: share the work between N processes of this program (1 by default: none). Each replay decodes the workload from\n"
            "                   its start with threads of its own, and that is bound by the bandwidth of the memory: one process saturates 8 cores, and two or more\n"
            "                   were no faster (0.9 to 1.0 times). A process takes about 1.5 GB. It may pay on a machine with more memory bandwidth to the cores.\n"
            "  --parallel       Replay the ways at the same time, each with an engine of its own: faster, and needs the memory of all.\n"
            "\n"
            "  --screenshot N   Instead of the check: replay once, up to frame N, and write the image of that frame to the PNG file of --out. With a\n"
            "                   recipe it is both eyes side by side, or the one that --eye says (anaglyph: red-cyan); without one, the frame as it is.\n"
            "  --profile        Instead of the check: replay frames A to B once (as they are, or for the eyes of the recipe) and say where the time of the calls went:\n"
            "                   the functions whose calls took longest, and the frames that took longest.\n"
            "  --same-frames-as FILE  Instead of the check: replay frames A to B of this workload and of FILE as they are, and say whether the images are the\n"
            "                   same (exit code 3 if not). For a slice of a workload (APIDumpSlicer): it has to replay to the images of the workload.\n"
            "  --contact-sheet K  Instead of the check: one PNG with K frames spread evenly from A to B (or the end of the workload), each as a screenshot\n"
            "                   is, and the numbers of the frames in the order they are shown.\n");
}

/* Replays the frames of the options one way, and keeps what the sink is given. */
bool Stereo::Replay::ConsistencyChecker::replay_to_frame(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                         const uint32_t&                                    in_frame_index,
                                                         Stereo::Replay::ConsistencyChecker::FrameHashSink* out_sink_ptr,
                                                         std::string*                                       out_error_ptr)
{
    const bool                                  uses_recipe = (in_options.recipe_filename.empty() == false);
    Stereo::Replay::ConsistencyChecker::Options options     = in_options;

    options.first_frame = in_frame_index;
    options.last_frame  = in_frame_index;

    /* With the history, every frame before the one asked for is replayed with the recipe (it is hurried through, with no image taken), which is what makes
     * what the eyes carry over from frame to frame right; the dependencies leave out what nothing after the frame reads, which makes it quick.
     */
    if (in_options.has_history ||
        in_options.deps_filename.empty() == false)
    {
        options.first_frame = 0;

        out_sink_ptr->set_frames_to_keep({in_frame_index});
    }

    return run_variant(options,
                       VARIANT_ARRAY[(uses_recipe) ? 4 : 0],
                       out_sink_ptr,
                       out_error_ptr);
}

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
                                              command_line.end  () );

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

bool Stereo::Replay::ConsistencyChecker::run_variant(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                     const Variant&                                     in_variant,
                                                     Stereo::Replay::ConsistencyChecker::FrameHashSink* out_sink_ptr,
                                                     std::string*                                       out_error_ptr,
                                                     ::Replay::ReplayEngineDLL::ICallInterceptor*       in_interceptor_ptr)
{
    constexpr auto POLL_INTERVAL = std::chrono::milliseconds(2);

    Stereo::Replay::EngineDLL::FrameExecutionOptions execution_options;
    bool                                             result            = false;

    auto* const engine_ptr = Stereo::Replay::EngineDLL::aiCreateReplayEngine();

    execution_options.first_frame    = in_options.first_frame;
    execution_options.last_frame     = in_options.last_frame;
    execution_options.mode           = in_variant.mode;
    execution_options.restores_state = in_variant.restores_state;

    {
        auto* const                                    controller_ptr = engine_ptr->get_playback_controller_ptr();
        Stereo::Replay::ConsistencyChecker::StopObserver observer(controller_ptr,
                                                                  in_options.last_frame);

        engine_ptr->add_replay_observer (&observer);

        if (in_interceptor_ptr != nullptr)
        {
            engine_ptr->add_call_interceptor(in_interceptor_ptr);
        }

        engine_ptr->set_frame_execution (execution_options);
        engine_ptr->set_frame_image_sink(out_sink_ptr);

        if (in_variant.uses_recipe)
        {
            engine_ptr->set_recipe(in_options.recipe);
            engine_ptr->get_playback_controller_ptr()->set_eye_separation(in_options.eye_separation);
        }

        if (engine_ptr->load(in_options.workload_filename.c_str(),
                             out_error_ptr) == false)
        {
            goto end;
        }

        if (in_options.deps_filename.empty() == false                                                  &&
            engine_ptr->set_dependencies_file(in_options.deps_filename.c_str(),
                                              out_error_ptr) == false)
        {
            goto end;
        }

        controller_ptr->set_speed(0.0);

        /* Going to the last frame is what a seek does, which leaves out what the dependencies say nothing after it reads. */
        if (in_options.deps_filename.empty() &&
            in_options.has_history == false)
        {
            controller_ptr->play();
        }
        else
        {
            controller_ptr->play_to_frame(in_options.last_frame);
        }

        while (true)
        {
            const auto status = controller_ptr->get_status();

            if (status.state == Stereo::Replay::EngineDLL::PlaybackState::FAILED)
            {
                *out_error_ptr = status.error_message;

                break;
            }

            if (status.state == Stereo::Replay::EngineDLL::PlaybackState::FINISHED ||
                status.state == Stereo::Replay::EngineDLL::PlaybackState::PAUSED)
            {
                result = true;

                break;
            }

            std::this_thread::sleep_for(POLL_INTERVAL);
        }

        if (in_interceptor_ptr != nullptr)
        {
            engine_ptr->remove_call_interceptor(in_interceptor_ptr);
        }

        engine_ptr->remove_replay_observer(&observer);
    }

end:
    Stereo::Replay::EngineDLL::aiDestroyReplayEngine(engine_ptr);

    return result;
}

bool Stereo::Replay::ConsistencyChecker::run_variants(const Stereo::Replay::ConsistencyChecker::Options&                     in_options,
                                                      const std::vector<const Stereo::Replay::ConsistencyChecker::Variant*>& in_variant_ptr_vec,
                                                      std::vector<Stereo::Replay::ConsistencyChecker::FrameHashSink>*        out_sink_vec_ptr,
                                                      std::string*                                                           out_error_ptr)
{
    struct Run
    {
        std::string error;
        bool        is_ok   = false;
        double      seconds = 0.0;
    };

    std::vector<Run> run_vec(in_variant_ptr_vec.size() );
    bool             result = true;

    out_sink_vec_ptr->clear();
    out_sink_vec_ptr->reserve(in_variant_ptr_vec.size() );

    for (size_t n_variant = 0;
                n_variant < in_variant_ptr_vec.size();
              ++n_variant)
    {
        out_sink_vec_ptr->emplace_back(false); /* in_keeps_images */
    }

    const auto run_func = [&](const size_t& in_n_variant)
    {
        const auto start_time = std::chrono::steady_clock::now();
        Run&       run      = run_vec.at(in_n_variant);

        run.is_ok   = run_variant(in_options,
                                  *in_variant_ptr_vec.at(in_n_variant),
                                  &out_sink_vec_ptr->at(in_n_variant),
                                  &run.error);
        run.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
    };
    const auto print_func = [&](const size_t& in_n_variant)
    {
        fprintf(stdout,
                "%-34s %zu images in %.1f s\n",
                in_variant_ptr_vec.at(in_n_variant)->name_ptr,
                out_sink_vec_ptr->at(in_n_variant).get_hash_map().size(),
                run_vec.at(in_n_variant).seconds);
        fflush(stdout);
    };

    if (in_options.is_parallel)
    {
        std::vector<std::thread> thread_vec;

        for (size_t n_variant = 0;
                    n_variant < in_variant_ptr_vec.size();
                  ++n_variant)
        {
            thread_vec.emplace_back(run_func,
                                    n_variant);
        }

        for (auto& thread : thread_vec)
        {
            thread.join();
        }
    }

    for (size_t n_variant = 0;
                n_variant < in_variant_ptr_vec.size() && result;
              ++n_variant)
    {
        if (in_options.is_parallel == false)
        {
            run_func(n_variant);
        }

        if (run_vec.at(n_variant).is_ok == false)
        {
            *out_error_ptr = std::string("Replaying the frames ")                          +
                             in_variant_ptr_vec.at(n_variant)->name_ptr                    +
                             " failed: "                                                    +
                             run_vec.at(n_variant).error;

            result = false;
        }
        else
        {
            print_func(n_variant);
        }
    }

    return result;
}

bool Stereo::Replay::ConsistencyChecker::take_contact_sheet(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                            std::string*                                       out_error_ptr)
{
    constexpr uint32_t N_INDEX_READER_THREADS = 2;

    uint32_t                                    n_columns       = 0;
    Capture::APIDumpLoader::WorkloadIndex       index;
    bool                                        is_index_built  = false;
    std::string                                 index_error;
    const bool                                  uses_recipe     = (in_options.recipe_filename.empty() == false);
    Stereo::Replay::ConsistencyChecker::Options options         = in_options;
    bool                                        result          = false;
    std::set<uint32_t>                          frame_set;
    std::vector<uint32_t>                       frame_vec;
    std::vector<uint32_t>                       shown_frame_vec;
    Utils::Images::Image                        sheet_image;
    std::vector<Utils::Images::Image>           tile_image_vec;
    Stereo::Replay::ConsistencyChecker::FrameHashSink sink(true); /* in_keeps_images */

    /* The end of the workload: the range of the options may go beyond it. */
    if (Capture::APIDumpLoader::WorkloadIndex::load_or_build(in_options.workload_filename.c_str(),
                                                             N_INDEX_READER_THREADS,
                                                            &index,
                                                            &is_index_built,
                                                            &index_error) &&
        index.get_n_frames() != 0)
    {
        options.last_frame = static_cast<uint32_t>(std::min<uint64_t>(options.last_frame,
                                                                      index.get_n_frames() - 1) );
    }

    frame_vec = Stereo::Replay::ConsistencyChecker::FrameSampler::sample(options.first_frame,
                                                                         options.last_frame,
                                                                         in_options.contact_sheet_n_frames);

    if (frame_vec.empty() )
    {
        *out_error_ptr = "The range of frames has none to show.";

        goto end;
    }

    frame_set.insert(frame_vec.begin(),
                     frame_vec.end() );
    sink.set_frames_to_keep(frame_set);

    options.first_frame = frame_vec.front();
    options.last_frame  = frame_vec.back();

    if (run_variant(options,
                    VARIANT_ARRAY[(uses_recipe) ? 4 : 0],
                   &sink,
                    out_error_ptr) == false)
    {
        goto end;
    }

    for (const auto n_frame : frame_vec)
    {
        Utils::Images::Image tile_image = sink.get_image({n_frame, 0});

        if (tile_image.width == 0)
        {
            continue;
        }

        if (uses_recipe)
        {
            const Utils::Images::Image& right_eye_image = sink.get_image({n_frame, 1});
            Utils::Images::Image        view_image;

            if (Utils::Images::ImageComposer::make_eye_view((in_options.has_eye_view) ? in_options.eye_view
                                                                                      : Utils::Images::EyeView::BOTH,
                                                            tile_image,
                                                            right_eye_image,
                                                            &view_image) == false)
            {
                *out_error_ptr = "The images of the eyes cannot be put together.";

                goto end;
            }

            tile_image = view_image;
        }

        shown_frame_vec.push_back(n_frame);
        tile_image_vec.push_back(tile_image);
    }

    if (tile_image_vec.empty() )
    {
        *out_error_ptr = "The workload has none of the frames.";

        goto end;
    }

    /* About as many tiles across as down; a tile with both eyes is twice as wide as one with an eye. */
    n_columns = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(tile_image_vec.size() ) ) ) );

    if (uses_recipe                                                                                                            &&
        (in_options.has_eye_view == false || in_options.eye_view == Utils::Images::EyeView::BOTH) )
    {
        n_columns = std::max<uint32_t>(1,
                                       (n_columns + 1) / 2);
    }

    if (Utils::Images::ImageComposer::make_contact_sheet(tile_image_vec,
                                                         n_columns,
                                                         &sheet_image) == false)
    {
        *out_error_ptr = "The contact sheet cannot be made.";

        goto end;
    }

    if (Utils::Images::PngWriter::write_file(in_options.image_filename,
                                             sheet_image) == false)
    {
        *out_error_ptr = "The file " + in_options.image_filename + " cannot be written.";

        goto end;
    }

    fprintf(stdout,
            "Contact sheet of %zu frames in %u columns, from left to right and from top to bottom:",
            shown_frame_vec.size(),
            n_columns);

    for (const auto n_frame : shown_frame_vec)
    {
        fprintf(stdout,
                " %u",
                n_frame);
    }

    fprintf(stdout,
            "\n");

    result = true;
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

bool Stereo::Replay::ConsistencyChecker::take_profile(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                      std::string*                                       out_error_ptr)
{
    constexpr size_t   N_FRAMES_LISTED    = 10;
    constexpr size_t   N_FUNCTIONS_LISTED = 15;
    constexpr double   NANOSECONDS_PER_MILLISECOND = 1000000.0;
    constexpr double   NANOSECONDS_PER_MICROSECOND = 1000.0;

    Stereo::Replay::ConsistencyChecker::ReplayProfiler profiler;
    bool                                               result   = false;
    Stereo::Replay::ConsistencyChecker::FrameHashSink  sink(false); /* in_keeps_images */
    const bool                                         uses_recipe = (in_options.recipe_filename.empty() == false);

    if (run_variant(in_options,
                    VARIANT_ARRAY[(uses_recipe) ? 4 : 0],
                   &sink,
                    out_error_ptr,
                   &profiler) == false)
    {
        goto end;
    }

    fprintf(stdout,
            "Replay of frames %u to %u%s: %llu calls took %.1f ms in all (the time of the calls, without the waiting for the pace of the capture).\n"
            "\n"
            "Functions by the time of their calls:\n"
            "  %10s %10s %12s %10s  %s\n",
            in_options.first_frame,
            in_options.last_frame,
            (uses_recipe) ? ", for the eyes of the recipe" : "",
            static_cast<unsigned long long>(profiler.get_n_calls() ),
            static_cast<double>(profiler.get_total_n_nanoseconds() ) / NANOSECONDS_PER_MILLISECOND,
            "total ms",
            "calls",
            "average us",
            "slowest us",
            "function");

    for (const auto& entry : profiler.get_slowest_functions(N_FUNCTIONS_LISTED) )
    {
        const char* function_name_ptr = Capture::APIDumpNames::get_function_name(entry.function);

        fprintf(stdout,
                "  %10.1f %10llu %12.2f %10.1f  %s\n",
                static_cast<double>(entry.total_n_nanoseconds) / NANOSECONDS_PER_MILLISECOND,
                static_cast<unsigned long long>(entry.n_calls),
                static_cast<double>(entry.total_n_nanoseconds) / static_cast<double>(entry.n_calls) / NANOSECONDS_PER_MICROSECOND,
                static_cast<double>(entry.max_n_nanoseconds) / NANOSECONDS_PER_MICROSECOND,
                function_name_ptr);
    }

    fprintf(stdout,
            "\nFrames that took longest:\n");

    for (const auto& entry : profiler.get_slowest_frames(N_FRAMES_LISTED) )
    {
        const char* slowest_function_name_ptr = Capture::APIDumpNames::get_function_name(entry.slowest_function);

        fprintf(stdout,
                "  frame %6u: %8.1f ms in %llu calls; the slowest call is %s (%.1f ms)\n",
                entry.n_frame,
                static_cast<double>(entry.total_n_nanoseconds) / NANOSECONDS_PER_MILLISECOND,
                static_cast<unsigned long long>(entry.n_calls),
                slowest_function_name_ptr,
                static_cast<double>(entry.slowest_n_nanoseconds) / NANOSECONDS_PER_MILLISECOND);
    }

    result = true;
end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::take_screenshot(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                         std::string*                                       out_error_ptr)
{
    const bool           uses_recipe = (in_options.recipe_filename.empty() == false);
    Utils::Images::Image image;
    bool                 result      = false;
    Stereo::Replay::ConsistencyChecker::FrameHashSink sink(true); /* in_keeps_images */

    if (replay_to_frame(in_options,
                        in_options.screenshot_frame,
                       &sink,
                        out_error_ptr) == false)
    {
        goto end;
    }

    image = sink.get_image({in_options.screenshot_frame, 0});

    if (image.width == 0)
    {
        *out_error_ptr = "The workload has no frame " + std::to_string(in_options.screenshot_frame) + ".";

        goto end;
    }

    if (uses_recipe)
    {
        const Utils::Images::Image& right_eye_image = sink.get_image({in_options.screenshot_frame, 1});
        Utils::Images::Image        view_image;

        if (Utils::Images::ImageComposer::make_eye_view(in_options.eye_view,
                                                        image,
                                                        right_eye_image,
                                                        &view_image) == false)
        {
            *out_error_ptr = "The images of the eyes cannot be put together.";

            goto end;
        }

        image = view_image;
    }

    if (Utils::Images::PngWriter::write_file(in_options.image_filename,
                                             image) == false)
    {
        *out_error_ptr = "The file " + in_options.image_filename + " cannot be written.";

        goto end;
    }

    result = true;
end:
    return result;
}

void Stereo::Replay::ConsistencyChecker::write_difference(const std::string&          in_label,
                                                          const std::string&          in_path,
                                                          const Utils::Images::Image& in_image_as_it_is,
                                                          const Utils::Images::Image& in_image_of_execution)
{
    Utils::Images::Image difference_image;
    const auto           difference       = Utils::Images::ImageComparer::compare(in_image_as_it_is,
                                                                                                in_image_of_execution);

    if (difference.has_same_size == false)
    {
        fprintf(stdout,
                "  %s: the image is not of the size of the one of the frame as it is\n",
                in_label.c_str() );

        goto end;
    }

    if (difference.n_differing_pixels == 0)
    {
        fprintf(stdout,
                "  %s: the same as the frame as it is\n",
                in_label.c_str() );

        goto end;
    }

    fprintf(stdout,
            "  %s: %llu pixels differ, in x %u to %u and y %u to %u, by up to %u levels of a channel\n",
            in_label.c_str(),
            static_cast<unsigned long long>(difference.n_differing_pixels),
            difference.bounding_box_min_x,
            difference.bounding_box_max_x,
            difference.bounding_box_min_y,
            difference.bounding_box_max_y,
            static_cast<uint32_t>(difference.max_channel_difference) );

    if (Utils::Images::ImageComparer::make_difference_image(in_image_as_it_is,
                                                            in_image_of_execution,
                                                            &difference_image) &&
        Utils::Images::PngWriter::write_file(in_path,
                                             difference_image) )
    {
        fprintf(stdout,
                "  %s: the pixels that differ are red in %s\n",
                in_label.c_str(),
                in_path.c_str() );
    }

end:
    return;
}

int main(int   in_argc,
         char* in_argv[])
{
    Stereo::Replay::ConsistencyChecker::ExitCode                    exit_code       = Stereo::Replay::ConsistencyChecker::ExitCode::SUCCESS;
    bool                                                            is_usage_error  = false;
    Stereo::Replay::ConsistencyChecker::Options                     options;
    std::vector<Stereo::Replay::ConsistencyChecker::FrameHashSink>  sink_vec;
    std::vector<const Stereo::Replay::ConsistencyChecker::Variant*> variant_ptr_vec;

    for (int n_argument = 1;
             n_argument < in_argc;
           ++n_argument)
    {
        const std::string argument = in_argv[n_argument];
        const bool        has_next = (n_argument + 1 < in_argc);

        if (argument == "--from" &&
            has_next)
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 UINT32_MAX,
                                                                 &value) == false)
            {
                is_usage_error = true;
            }

            options.first_frame = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--to" &&
            has_next)
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 UINT32_MAX,
                                                                 &value) == false)
            {
                is_usage_error = true;
            }

            options.last_frame = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--contact-sheet" &&
            has_next)
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 UINT32_MAX,
                                                                 &value) == false)
            {
                is_usage_error = true;
            }

            options.contact_sheet_n_frames = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--dump-dir" &&
            has_next)
        {
            options.dump_directory = in_argv[++n_argument];
        }
        else
        if (argument == "--dump-frame" &&
            has_next)
        {
            options.dump_frame = static_cast<uint32_t>(::strtoul(in_argv[++n_argument],
                                                                  nullptr,
                                                                  10) );
        }
        else
        if (argument == "--eye-separation" &&
            has_next)
        {
            double separation = 0.0;

            if (Utils::CommandLine::NumberParser::parse_double(in_argv[++n_argument],
                                                               &separation) == false)
            {
                is_usage_error = true;
            }

            options.eye_separation = static_cast<float>(separation);
        }
        else
        if (argument == "--deps" &&
            has_next)
        {
            options.deps_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--history")
        {
            options.has_history = true;
        }
        else
        if (argument == "--recipe" &&
            has_next)
        {
            options.recipe_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--ways" &&
            has_next)
        {
            std::string list    = in_argv[++n_argument];
            size_t      n_start = 0;

            while (n_start <= list.size() )
            {
                const size_t n_end = list.find(',', n_start);

                options.way_name_vec.push_back(list.substr(n_start,
                                                           (n_end == std::string::npos) ? std::string::npos
                                                                                        : n_end - n_start) );

                n_start = (n_end == std::string::npos) ? list.size() + 1
                                                       : n_end + 1;
            }
        }
        else
        if (argument == "--eye" &&
            has_next)
        {
            const std::string eye_name = in_argv[++n_argument];

            if (eye_name == "left")
            {
                options.eye_view     = Utils::Images::EyeView::LEFT;
                options.has_eye_view = true;
            }
            else
            if (eye_name == "right")
            {
                options.eye_view     = Utils::Images::EyeView::RIGHT;
                options.has_eye_view = true;
            }
            else
            if (eye_name == "both")
            {
                options.eye_view     = Utils::Images::EyeView::BOTH;
                options.has_eye_view = true;
            }
            else
            if (eye_name == "anaglyph")
            {
                options.eye_view     = Utils::Images::EyeView::ANAGLYPH;
                options.has_eye_view = true;
            }
            else
            {
                is_usage_error = true;
            }
        }
        else
        if (argument == "--out" &&
            has_next)
        {
            options.image_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--screenshot" &&
            has_next)
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 UINT32_MAX - 1,
                                                                 &value) == false)
            {
                is_usage_error = true;
            }

            options.screenshot_frame = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--same-frames-as" &&
            has_next)
        {
            options.same_frames_workload_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--profile")
        {
            options.is_profiling = true;
        }
        else
        if (argument == "--overlay-probe" &&
            has_next)
        {
            options.overlay_probe_filename = in_argv[++n_argument];
        }
        else
        if (argument == "--parallel")
        {
            options.is_parallel = true;
        }
        else
        if (argument == "--jobs" &&
            has_next)
        {
            uint64_t value = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 MAXIMUM_WAIT_OBJECTS - 1,
                                                                 &value) == false ||
                value == 0)
            {
                is_usage_error = true;
            }

            options.n_jobs = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--overlay-probe-shard" &&
            n_argument + 2 < in_argc)
        {
            uint64_t index = 0;
            uint64_t count = 0;

            if (Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 MAXIMUM_WAIT_OBJECTS,
                                                                 &index) == false ||
                Utils::CommandLine::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                 MAXIMUM_WAIT_OBJECTS,
                                                                 &count) == false ||
                index >= count)
            {
                is_usage_error = true;
            }

            options.overlay_probe_shard_count = static_cast<uint32_t>(count);
            options.overlay_probe_shard_index = static_cast<uint32_t>(index);
        }
        else
        if (argument == "--overlay-probe-result" &&
            has_next)
        {
            options.overlay_probe_result_filename = in_argv[++n_argument];
        }
        else
        if (argument.empty()                  == false &&
            argument.at(0)                    != '-'   &&
            options.workload_filename.empty() )
        {
            options.workload_filename = argument;
        }
        else
        {
            is_usage_error = true;
        }
    }

    if (is_usage_error                                         ||
        options.workload_filename.empty()                      ||
        options.first_frame               > options.last_frame ||
        Stereo::Replay::ConsistencyChecker::are_image_options_valid(options) == false)
    {
        Stereo::Replay::ConsistencyChecker::print_usage();

        exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::BAD_ARGUMENTS;

        goto end;
    }

    if (options.recipe_filename.empty() == false)
    {
        std::string error;

        if (Analysis::Recipe::RecipeLoader::load(options.recipe_filename,
                                                 &options.recipe,
                                                 &error) == false)
        {
            fprintf(stdout,
                    "The recipe cannot be read: %s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::BAD_ARGUMENTS;

            goto end;
        }
    }

    if (options.is_profiling)
    {
        std::string error;

        if (Stereo::Replay::ConsistencyChecker::take_profile(options,
                                                            &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;
        }

        goto end;
    }

    if (options.same_frames_workload_filename.empty() == false)
    {
        bool        has_difference = false;
        std::string error;

        if (Stereo::Replay::ConsistencyChecker::compare_workloads(options,
                                                                 &has_difference,
                                                                 &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }

        if (has_difference)
        {
            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::DIFFERENCES_FOUND;
        }

        goto end;
    }

    if (options.contact_sheet_n_frames != 0)
    {
        std::string error;

        if (Stereo::Replay::ConsistencyChecker::take_contact_sheet(options,
                                                                  &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }

        fprintf(stdout,
                "Written to %s\n",
                options.image_filename.c_str() );

        goto end;
    }

    if (options.overlay_probe_filename.empty() == false)
    {
        std::string error;

        if (options.recipe_filename.empty() ||
            options.dump_directory.empty() )
        {
            fprintf(stdout,
                    "--overlay-probe needs --recipe and --dump-dir.\n");

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::BAD_ARGUMENTS;

            goto end;
        }

        if (Stereo::Replay::ConsistencyChecker::take_overlay_probe(options,
                                                                  &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }

        fprintf(stdout,
                "Written to %s\n",
                options.dump_directory.c_str() );

        goto end;
    }

    if (options.screenshot_frame != UINT32_MAX)
    {
        std::string error;

        if (Stereo::Replay::ConsistencyChecker::take_screenshot(options,
                                                               &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }

        fprintf(stdout,
                "Frame %u written to %s\n",
                options.screenshot_frame,
                options.image_filename.c_str() );

        goto end;
    }

    /* The frames as they are come first: they are what the other ways are compared with. Without a choice, the next one tells how much a replay
     * differs from itself; then either the two with executions twice, or the one with the recipe.
     */
    variant_ptr_vec.push_back(&Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[0]);

    if (options.way_name_vec.empty() )
    {
        variant_ptr_vec.push_back(&Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[1]);

        if (options.recipe_filename.empty() )
        {
            variant_ptr_vec.push_back(&Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[2]);
            variant_ptr_vec.push_back(&Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[3]);
        }
        else
        {
            variant_ptr_vec.push_back(&Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[4]);
        }
    }
    else
    {
        for (const auto& way_name : options.way_name_vec)
        {
            const auto* variant_ptr = Stereo::Replay::ConsistencyChecker::get_variant_ptr(way_name);

            if (variant_ptr == nullptr                                                                 ||
                (variant_ptr->uses_recipe && options.recipe_filename.empty() ) )
            {
                fprintf(stdout,
                        "The way \"%s\" is not one that can be replayed here: again, twice, restore, or recipe with --recipe.\n",
                        way_name.c_str() );

                exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::BAD_ARGUMENTS;

                goto end;
            }

            variant_ptr_vec.push_back(variant_ptr);
        }
    }

    {
        std::string error;

        if (Stereo::Replay::ConsistencyChecker::run_variants(options,
                                                             variant_ptr_vec,
                                                            &sink_vec,
                                                            &error) == false)
        {
            fprintf(stdout,
                    "%s\n",
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }
    }

    if (options.dump_frame != UINT32_MAX &&
        options.dump_directory.empty() == false)
    {
        /* The frame is the one that the last way makes: the one under test. */
        Stereo::Replay::ConsistencyChecker::dump_frame(options,
                                                       options.dump_frame,
                                                       *variant_ptr_vec.back() );
    }

    {
        uint32_t first_frame            = 0;
        bool     has_difference_to_dump = false;

        for (size_t n_variant = 1;
                    n_variant < sink_vec.size();
                  ++n_variant)
        {
            uint32_t   variant_first_frame = 0;
            const bool has_difference      = Stereo::Replay::ConsistencyChecker::print_comparison(*variant_ptr_vec.at(n_variant),
                                                                                                  sink_vec.at        (0).get_hash_map        (),
                                                                                                  sink_vec.at        (n_variant).get_hash_map(),
                                                                                                 &variant_first_frame);

            /* The last way is the one under test; the ones before it only show how much a replay differs from itself. */
            if (has_difference                        &&
                n_variant      == sink_vec.size() - 1)
            {
                first_frame            = variant_first_frame;
                has_difference_to_dump = true;
            }
        }

        if (has_difference_to_dump)
        {
            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::DIFFERENCES_FOUND;

            if (options.dump_directory.empty() == false)
            {
                Stereo::Replay::ConsistencyChecker::dump_frame(options,
                                                               first_frame,
                                                               *variant_ptr_vec.back() );
            }
        }
    }

end:
    return static_cast<int>(exit_code);
}
