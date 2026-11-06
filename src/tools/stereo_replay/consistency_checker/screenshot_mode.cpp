/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "screenshot_mode.h"
#include "Capture/APIDumpLoader/WorkloadIndex.h"
#include "frame_hash_sink.h"
#include "frame_sampler.h"
#include "replay_runner.h"
#include "utils/images/image_composer.h"
#include "utils/images/png_writer.h"
#include <cmath>
#include <set>
#include <stdio.h>
#include <vector>

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
