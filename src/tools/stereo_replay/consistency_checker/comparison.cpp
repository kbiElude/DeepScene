/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "comparison.h"
#include "replay_runner.h"
#include "tools/stereo_replay/engine_dll/entrypoints.h"
#include "utils/images/image_comparer.h"
#include "utils/images/png_writer.h"
#include <stdio.h>
#include <vector>

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
