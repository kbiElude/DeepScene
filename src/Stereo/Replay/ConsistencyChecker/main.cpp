/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/number_parser.h"
#include "Analysis/RecipeLoader/recipe_loader.h"
#include "Stereo/Replay/EngineDLL/entrypoints.h"
#include "frame_hash_sink.h"
#include "stop_observer.h"
#include "types.h"
#include <chrono>
#include <map>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            extern const Variant VARIANT_ARRAY[5];

            /* Writes the images of one frame, as it is and executed twice, to the directory of the options. */
            void dump_frame(const Options&  in_options,
                            const uint32_t& in_n_frame,
                            const Variant&  in_variant);

            /* Says how the executions of a way of replaying differ from the frames as they are. Returns the first frame in which one of the
             * executions differs, and false if there is none.
             */
            bool print_comparison(const Variant&                                     in_variant,
                                  const std::map<FrameHashSink::ImageKey, uint64_t>& in_baseline_map,
                                  const std::map<FrameHashSink::ImageKey, uint64_t>& in_hash_map,
                                  uint32_t*                                          out_first_frame_ptr);

            void print_usage();

            /* Replays the frames of the options one way, and keeps what the sink is given. */
            bool run_variant(const Options& in_options,
                             const Variant& in_variant,
                             FrameHashSink* out_sink_ptr,
                             std::string*   out_error_ptr);

            /* A 32-bit bitmap, top row first. */
            bool write_bitmap(const std::string&          in_path,
                              const FrameHashSink::Image& in_image);
        }
    }
}

const Stereo::Replay::ConsistencyChecker::Variant Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[] =
{
    {Stereo::Replay::EngineDLL::FrameExecution::DIRECT, "as they are",                       false, false},
    {Stereo::Replay::EngineDLL::FrameExecution::DIRECT, "as they are, again",                false, false},
    {Stereo::Replay::EngineDLL::FrameExecution::TWICE,  "twice",                             false, false},
    {Stereo::Replay::EngineDLL::FrameExecution::TWICE,  "twice, the state put back between", true,  false},
    {Stereo::Replay::EngineDLL::FrameExecution::TWICE,  "twice, for the eyes of the recipe", false, true}
};

/* Writes the images of one frame, as it is and executed twice, to the directory of the options. */
void Stereo::Replay::ConsistencyChecker::dump_frame(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                    const uint32_t&                                    in_n_frame,
                                                    const Variant&                                     in_variant)
{
    Stereo::Replay::ConsistencyChecker::FrameHashSink direct_sink(true); /* in_keeps_images */
    std::string                                       error;
    Stereo::Replay::ConsistencyChecker::Options       options = in_options;
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

        write_bitmap(prefix + "_as_it_is.bmp",
                     direct_sink.get_image({in_n_frame, 0}) );
        write_bitmap(prefix + "_first_execution.bmp",
                     twice_sink.get_image({in_n_frame, 0}) );
        write_bitmap(prefix + "_second_execution.bmp",
                     twice_sink.get_image({in_n_frame, 1}) );

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
bool Stereo::Replay::ConsistencyChecker::print_comparison(const Variant&                                                                         in_variant,
                                                          const std::map<Stereo::Replay::ConsistencyChecker::FrameHashSink::ImageKey, uint64_t>& in_baseline_map,
                                                          const std::map<Stereo::Replay::ConsistencyChecker::FrameHashSink::ImageKey, uint64_t>& in_hash_map,
                                                          uint32_t*                                                                              out_first_frame_ptr)
{
    constexpr size_t N_FRAMES_LISTED = 20;

    bool                  has_difference         = false;
    uint32_t              n_executions_differ    = 0;
    uint32_t              n_first_differs        = 0;
    uint32_t              n_second_differs       = 0;
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
            "\n"
            "Replays frames A to B (default 0 to 999) of a Direct3D 8 workload in four ways: as they are, as they are again (to\n"
            "see how much a replay differs from itself), executed twice, and executed twice with the state of the device put\n"
            "back before the second execution. The back buffer after each execution is compared with that of the first replay.\n"
            "\n"
            "  --dump-dir DIR   Write the images of the first frame in which an execution differs, as bitmaps\n"
            "  --eye-separation D  The distance between the eyes for the recipe (0 by default)\n"
            "  --recipe FILE    Replay the frames twice with the recipe instead of the last two ways: the first execution is for\n"
            "                   the left eye, the second for the right one. A recipe that changes nothing gives the frames as they are.\n");
}

/* Replays the frames of the options one way, and keeps what the sink is given. */
bool Stereo::Replay::ConsistencyChecker::run_variant(const Stereo::Replay::ConsistencyChecker::Options& in_options,
                                                     const Variant&                                     in_variant,
                                                     Stereo::Replay::ConsistencyChecker::FrameHashSink* out_sink_ptr,
                                                     std::string*                                       out_error_ptr)
{
    constexpr auto POLL_INTERVAL = std::chrono::milliseconds(2);

    Stereo::Replay::EngineDLL::FrameExecutionOptions execution_options;
    bool                                             result = false;

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

        controller_ptr->set_speed(0.0);
        controller_ptr->play();

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

        engine_ptr->remove_replay_observer(&observer);
    }

end:
    Stereo::Replay::EngineDLL::aiDestroyReplayEngine(engine_ptr);

    return result;
}

/* A 32-bit bitmap, top row first. */
bool Stereo::Replay::ConsistencyChecker::write_bitmap(const std::string&                                              in_path,
                                                      const Stereo::Replay::ConsistencyChecker::FrameHashSink::Image& in_image)
{
#pragma pack(push, 1)
    struct BitmapHeader
    {
        uint16_t signature;
        uint32_t file_size;
        uint32_t reserved;
        uint32_t pixel_offset;
        uint32_t header_size;
        int32_t  width;
        int32_t  height;
        uint16_t n_planes;
        uint16_t n_bits_per_pixel;
        uint32_t compression;
        uint32_t image_size;
        int32_t  x_pixels_per_meter;
        int32_t  y_pixels_per_meter;
        uint32_t n_colors;
        uint32_t n_important_colors;
    };
#pragma pack(pop)

    constexpr uint16_t BITMAP_SIGNATURE    = 0x4d42; /* BM */
    constexpr uint16_t N_BITS_PER_PIXEL    = 32;
    constexpr uint32_t INFO_HEADER_N_BYTES = 40;

    BitmapHeader header      = {};
    FILE* const  file_handle = ::fopen(in_path.c_str(),
                                       "wb");
    bool         result      = false;

    if (file_handle != nullptr)
    {
        header.file_size        = static_cast<uint32_t>(sizeof(header) + in_image.pixel_u8_vec.size() );
        header.header_size      = INFO_HEADER_N_BYTES;
        header.height           = -static_cast<int32_t>(in_image.height);
        header.image_size       = static_cast<uint32_t>(in_image.pixel_u8_vec.size() );
        header.n_bits_per_pixel = N_BITS_PER_PIXEL;
        header.n_planes         = 1;
        header.pixel_offset     = sizeof(header);
        header.signature        = BITMAP_SIGNATURE;
        header.width            = static_cast<int32_t>(in_image.width);

        const bool is_header_written = (::fwrite(&header,
                                                 sizeof(header),
                                                 1,
                                                 file_handle) == 1);
        const bool is_image_written  = (::fwrite(in_image.pixel_u8_vec.data(),
                                                 in_image.pixel_u8_vec.size(),
                                                 1,
                                                 file_handle) == 1);

        result = (::fclose(file_handle) == 0 &&
                  is_header_written          &&
                  is_image_written);
    }

    return result;
}

int main(int   in_argc,
         char* in_argv[])
{
    Stereo::Replay::ConsistencyChecker::ExitCode                    exit_code      = Stereo::Replay::ConsistencyChecker::ExitCode::SUCCESS;
    bool                                                            is_usage_error = false;
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

            if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(in_argv[++n_argument],
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

            if (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(in_argv[++n_argument],
                                                                        UINT32_MAX,
                                                                       &value) == false)
            {
                is_usage_error = true;
            }

            options.last_frame = static_cast<uint32_t>(value);
        }
        else
        if (argument == "--dump-dir" &&
            has_next)
        {
            options.dump_directory = in_argv[++n_argument];
        }
        else
        if (argument == "--eye-separation" &&
            has_next)
        {
            double separation = 0.0;

            if (Analysis::APIDumpAnalysis::NumberParser::parse_double(in_argv[++n_argument],
                                                                     &separation) == false)
            {
                is_usage_error = true;
            }

            options.eye_separation = static_cast<float>(separation);
        }
        else
        if (argument == "--recipe" &&
            has_next)
        {
            options.recipe_filename = in_argv[++n_argument];
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
        options.first_frame               > options.last_frame)
    {
        Stereo::Replay::ConsistencyChecker::print_usage();

        exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::BAD_ARGUMENTS;

        goto end;
    }

    if (options.recipe_filename.empty() == false)
    {
        std::string error;

        if (Analysis::RecipeLoader::RecipeLoader::load(options.recipe_filename,
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

    /* The first two ways tell how much a replay differs from itself; then either the two with executions twice, or the one with the recipe. */
    if (options.recipe_filename.empty() )
    {
        variant_ptr_vec =
        {
            &Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[0],
            &Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[1],
            &Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[2],
            &Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[3]
        };
    }
    else
    {
        variant_ptr_vec =
        {
            &Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[0],
            &Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[1],
            &Stereo::Replay::ConsistencyChecker::VARIANT_ARRAY[4]
        };
    }

    sink_vec.reserve(variant_ptr_vec.size() );

    for (const auto variant_ptr : variant_ptr_vec)
    {
        const Stereo::Replay::ConsistencyChecker::Variant& variant = *variant_ptr;
        std::string                                        error;

        sink_vec.emplace_back(false); /* in_keeps_images */

        const auto start_time = std::chrono::steady_clock::now();

        if (Stereo::Replay::ConsistencyChecker::run_variant(options,
                                                            variant,
                                                           &sink_vec.back(),
                                                           &error) == false)
        {
            fprintf(stdout,
                    "Replaying the frames %s failed: %s\n",
                    variant.name_ptr,
                    error.c_str() );

            exit_code = Stereo::Replay::ConsistencyChecker::ExitCode::REPLAY_FAILED;

            goto end;
        }

        fprintf(stdout,
                "%-34s %zu images in %.1f s\n",
                variant.name_ptr,
                sink_vec.back().get_hash_map().size(),
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count() );
        fflush(stdout);
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
