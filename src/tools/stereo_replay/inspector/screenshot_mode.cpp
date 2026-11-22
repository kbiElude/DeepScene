/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/workload_index.h"
#include "command_line.h"
#include "contact_sheet_frame_picker.h"
#include "frame_hash_sink.h"
#include "replay_runner.h"
#include "screenshot_mode.h"
#include "utils/images/image_composer.h"
#include "utils/images/png_writer.h"
#include <cmath>
#include <set>
#include <stdio.h>
#include <vector>

namespace
{
    /* The image of a frame as the options ask to see it: the frame as it is, or with a recipe the eye view of both eyes. The image is empty if the sink has none of the
     * frame. Returns false if the images of the eyes cannot be put together.
     */
    bool make_frame_image(const Stereo::Replay::Inspector::Options&       in_options,
                          const Stereo::Replay::Inspector::FrameHashSink& in_sink,
                          const uint32_t&                                 in_frame,
                          Utils::Images::Image*                           out_image_ptr)
    {
        const bool uses_recipe = (in_options.recipe_filename.empty() == false);
        bool       result      = true;

        *out_image_ptr = in_sink.get_image({in_frame, 0});

        if (out_image_ptr->width != 0 &&
            uses_recipe)
        {
            const Utils::Images::Image& right_eye_image = in_sink.get_image({in_frame, 1});
            Utils::Images::Image        view_image;

            result = Utils::Images::ImageComposer::make_eye_view(in_options.eye_view,
                                                                 *out_image_ptr,
                                                                 right_eye_image,
                                                                 &view_image);

            if (result)
            {
                *out_image_ptr = view_image;
            }
        }

        return result;
    }
}

bool Stereo::Replay::Inspector::take_contact_sheet(const Stereo::Replay::Inspector::Options& in_options,
                                                   std::string*                                       out_error_ptr)
{
    constexpr uint32_t N_INDEX_READER_THREADS = 2;

    uint32_t                                    n_columns       = 0;
    Analysis::APIDumpIndex::WorkloadIndex       index;
    bool                                        is_index_built  = false;
    std::string                                 index_error;
    const bool                                  uses_recipe     = (in_options.recipe_filename.empty() == false);
    Stereo::Replay::Inspector::Options options                  = in_options;
    bool                                        result          = false;
    std::set<uint32_t>                          frame_set;
    std::vector<uint32_t>                       frame_vec;
    std::vector<uint32_t>                       shown_frame_vec;
    Utils::Images::Image                        sheet_image;
    std::vector<Utils::Images::Image>           tile_image_vec;

    Stereo::Replay::Inspector::FrameHashSink sink(true); /* in_keeps_images */

    /* The last frame of the workload; the range of the options may extend past it. */
    const bool is_index_loaded = Analysis::APIDumpIndex::WorkloadIndex::load_or_build(in_options.workload_filename.c_str(),
                                                                                      N_INDEX_READER_THREADS,
                                                                                     &index,
                                                                                     &is_index_built,
                                                                                     &index_error);

    if (is_index_loaded)
    {
        const uint64_t n_index_frames = index.get_n_frames();

        if (n_index_frames != 0)
        {
            options.last_frame = static_cast<uint32_t>(std::min<uint64_t>(options.last_frame,
                                                                          n_index_frames - 1) );
        }
    }

    frame_vec = Stereo::Replay::Inspector::ContactSheetFramePicker::sample(options.first_frame,
                                                                           options.last_frame,
                                                                           in_options.contact_sheet_n_frames);

    if (frame_vec.empty() )
    {
        *out_error_ptr = "The range of frames has none to show.";

        goto end;
    }

    frame_set.insert       (frame_vec.begin(),
                            frame_vec.end() );
    sink.set_frames_to_keep(frame_set);

    options.first_frame = frame_vec.front();
    options.last_frame  = frame_vec.back();

    if (run_variant(options,
                    REPLAY_WAY_ARRAY[(uses_recipe) ? 4 : 0],
                   &sink,
                    out_error_ptr) == false)
    {
        goto end;
    }

    for (const auto n_frame : frame_vec)
    {
        Utils::Images::Image tile_image;

        if (make_frame_image(in_options,
                             sink,
                             n_frame,
                            &tile_image) == false)
        {
            *out_error_ptr = "The images of the eyes cannot be put together.";

            goto end;
        }

        if (tile_image.width == 0)
        {
            continue;
        }

        shown_frame_vec.push_back(n_frame);
        tile_image_vec.push_back(tile_image);
    }

    if (tile_image_vec.empty() )
    {
        *out_error_ptr = "The workload has none of the frames.";

        goto end;
    }

    /* Use about as many tiles across as down. A tile with both eyes is twice as wide as a tile with one eye. */
    n_columns = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(tile_image_vec.size() ) ) ) );

    if (uses_recipe                                                                               &&
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
        *out_error_ptr = "The file "               +
                         in_options.image_filename +
                         " cannot be written.";

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

bool Stereo::Replay::Inspector::take_screenshot(const Stereo::Replay::Inspector::Options& in_options,
                                                Stereo::Replay::Inspector::ScreenshotReport* out_report_ptr,
                                                std::string*                               out_error_ptr)
{
    constexpr uint32_t N_INDEX_READER_THREADS = 2;

    const auto&                                  frame_vec        = in_options.screenshot_frame_vec;
    Analysis::APIDumpIndex::WorkloadIndex        index;
    std::string                                  index_error;
    bool                                         is_index_built   = false;
    const bool                                   is_numbered      = (frame_vec.size() > 1);
    bool                                         is_replayed      = false;
    std::vector<uint32_t>                        reached_frame_vec;
    bool                                         result           = false;
    Stereo::Replay::Inspector::FrameHashSink     sink(true); /* in_keeps_images */

    *out_report_ptr = ScreenshotReport();

    /* A frame past the end of the workload is not asked of the replay: it would play the whole workload to find that out. */
    const bool     is_index_loaded = Analysis::APIDumpIndex::WorkloadIndex::load_or_build(in_options.workload_filename.c_str(),
                                                                                          N_INDEX_READER_THREADS,
                                                                                         &index,
                                                                                         &is_index_built,
                                                                                         &index_error);
    const uint64_t n_frames        = (is_index_loaded) ? index.get_n_frames()
                                                       : 0;

    for (const auto n_frame : frame_vec)
    {
        if (n_frames != 0 &&
            n_frame  >= n_frames)
        {
            out_report_ptr->skipped_frame_vec.push_back(n_frame);
        }
        else
        {
            reached_frame_vec.push_back(n_frame);
        }
    }

    if (reached_frame_vec.empty() )
    {
        *out_error_ptr = (is_numbered) ? std::string("The workload has none of the frames.")
                                       : "The workload has no frame " + std::to_string(frame_vec.front() ) + ".";

        goto end;
    }

    if (is_numbered)
    {
        is_replayed = replay_to_frames(in_options,
                                       reached_frame_vec,
                                      &sink,
                                       out_error_ptr);
    }
    else
    {
        is_replayed = replay_to_frame(in_options,
                                      frame_vec.front(),
                                     &sink,
                                      out_error_ptr);
    }

    if (is_replayed == false)
    {
        goto end;
    }

    for (const auto n_frame : reached_frame_vec)
    {
        Utils::Images::Image image;

        if (make_frame_image(in_options,
                             sink,
                             n_frame,
                            &image) == false)
        {
            *out_error_ptr = "The images of the eyes cannot be put together.";

            goto end;
        }

        if (image.width == 0)
        {
            out_report_ptr->skipped_frame_vec.push_back(n_frame);

            continue;
        }

        {
            ScreenshotFile file;

            file.filename = (is_numbered) ? CommandLine::make_numbered_filename(in_options.image_filename,
                                                                                n_frame)
                                          : in_options.image_filename;
            file.frame    = n_frame;

            if (Utils::Images::PngWriter::write_file(file.filename,
                                                     image) == false)
            {
                *out_error_ptr = "The file "      +
                                 file.filename    +
                                 " cannot be written.";

                goto end;
            }

            out_report_ptr->written_vec.push_back(file);
        }
    }

    if (out_report_ptr->written_vec.empty() )
    {
        *out_error_ptr = (is_numbered) ? std::string("The workload has none of the frames.")
                                       : "The workload has no frame " + std::to_string(frame_vec.front() ) + ".";

        goto end;
    }

    result = true;
end:
    return result;
}
