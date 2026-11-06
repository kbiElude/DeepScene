/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "overlay_probe.h"
#include "analysis/api_dump_analysis/query_commands.h"
#include "utils/images/gif_writer.h"
#include "utils/images/image_comparer.h"
#include "utils/images/png_writer.h"
#include "utils/json/json_reader.h"
#include "utils/json/json_writer.h"
#include <algorithm>
#include <map>
#include <set>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
    std::string escape_html(const std::string& in_text)
    {
        std::string result;

        for (const char character : in_text)
        {
            switch (character)
            {
                case '&':
                {
                    result += "&amp;";

                    break;
                }

                case '<':
                {
                    result += "&lt;";

                    break;
                }

                case '>':
                {
                    result += "&gt;";

                    break;
                }

                case '"':
                {
                    result += "&quot;";

                    break;
                }

                default:
                {
                    result += character;

                    break;
                }
            }
        }

        return result;
    }

    std::string get_hex_text(const uint64_t& in_key)
    {
        char text[19];

        ::snprintf(text,
                   sizeof(text),
                   "0x%016llx",
                   static_cast<unsigned long long>(in_key) );

        return text;
    }

    bool write_text_file(const std::string& in_path,
                         const std::string& in_text)
    {
        FILE*      file_ptr = ::fopen(in_path.c_str(),
                                      "wb");
        const bool result   = (file_ptr != nullptr);

        if (result)
        {
            ::fwrite(in_text.data(),
                     1,
                     in_text.size(),
                     file_ptr);
            ::fclose(file_ptr);
        }

        return result;
    }
}

void Stereo::Replay::ConsistencyChecker::OverlayProbe::add_warnings(OverlayProbeEntry* inout_entry_ptr)
{
    /* An overlay is a part of the picture: a draw that makes most of it is either a fade, or a scene which is taken for an overlay. */
    constexpr double MUCH_OF_PICTURE_FRACTION = 0.4;

    const bool is_left_alone = (inout_entry_ptr->replay_class == "unaffected" ||
                                inout_entry_ptr->live_class   == "unaffected" ||
                                inout_entry_ptr->live_class   == "mixed");

    if (is_left_alone)
    {
        if (inout_entry_ptr->depth_spread.has_depth        &&
            inout_entry_ptr->depth_spread.is_flat == false)
        {
            inout_entry_ptr->warning_vec.push_back("deep_depth");
        }

        if (inout_entry_ptr->n_picture_pixels != 0 &&
            static_cast<double>(inout_entry_ptr->n_changed_pixels) > MUCH_OF_PICTURE_FRACTION * static_cast<double>(inout_entry_ptr->n_picture_pixels) )
        {
            inout_entry_ptr->warning_vec.push_back("covers_much_of_picture");
        }
    }
}

uint32_t Stereo::Replay::ConsistencyChecker::OverlayProbe::get_n_shards(const Options&                        in_options,
                                                                        const std::vector<OverlayProbeEntry>& in_entry_vec)
{
    return std::max<uint32_t>(1,
                              std::min<uint32_t>(in_options.n_jobs,
                                                 static_cast<uint32_t>(get_pass_group_map(in_entry_vec).size() ) ) );
}

std::map<std::pair<uint32_t, uint64_t>, std::vector<size_t> > Stereo::Replay::ConsistencyChecker::OverlayProbe::get_pass_group_map(const std::vector<OverlayProbeEntry>& in_entry_vec)
{
    std::map<std::pair<uint32_t, uint64_t>, std::vector<size_t> > result;

    for (size_t n_entry = 0;
                n_entry < in_entry_vec.size();
              ++n_entry)
    {
        result[std::make_pair(in_entry_vec.at(n_entry).frame_index,
                              in_entry_vec.at(n_entry).pass_key)].push_back(n_entry);
    }

    return result;
}

std::vector<uint32_t> Stereo::Replay::ConsistencyChecker::OverlayProbe::get_shard_of_entry_vec(const std::vector<OverlayProbeEntry>& in_entry_vec,
                                                                                               const uint32_t&                       in_n_shards)
{
    /* A replay costs the frames it goes through, and a few more for the device it makes. */
    constexpr uint64_t N_FRAMES_OF_SETUP = 1000;

    struct Group
    {
        uint32_t            frame_index = 0;
        std::vector<size_t> n_entry_vec;
    };

    std::vector<Group>                      group_vec;
    std::vector<std::set<uint32_t> >        frame_set_vec(in_n_shards);
    std::vector<uint64_t>                   load_vec(in_n_shards,
                                                     0);
    std::vector<uint32_t>                   result(in_entry_vec.size(),
                                                   0);

    for (const auto& group_pair : get_pass_group_map(in_entry_vec) )
    {
        Group group;

        group.frame_index = group_pair.first.first;
        group.n_entry_vec = group_pair.second;

        group_vec.push_back(group);
    }

    /* The frames that cost most go first, each group to the shard that would be done with it soonest, which is the shard with the least to do unless the frame is
     * made there already (the frame as it is is made once for each shard that has a draw of it). Only the list says it, so every process comes to the same.
     */
    std::stable_sort(group_vec.begin(),
                     group_vec.end  (),
                     [](const Group& in_group_a,
                        const Group& in_group_b)
                     {
                         return (in_group_a.frame_index > in_group_b.frame_index);
                     });

    for (const auto& group : group_vec)
    {
        const uint64_t cost_of_replay = group.frame_index + N_FRAMES_OF_SETUP;
        const uint64_t n_replays      = group.n_entry_vec.size() + ((group.n_entry_vec.size() > 1) ? 1 : 0); /* the draws, and the pass if it has more than one */
        uint32_t       n_best_shard   = 0;
        uint64_t       best_load      = 0;

        for (uint32_t n_shard = 0;
                      n_shard < in_n_shards;
                    ++n_shard)
        {
            const uint64_t n_baselines = (frame_set_vec.at(n_shard).count(group.frame_index) != 0) ? 0
                                                                                                     : 1;
            const uint64_t load        = load_vec.at(n_shard) + cost_of_replay * (n_replays + n_baselines);

            if (n_shard == 0 ||
                load    <  best_load)
            {
                n_best_shard = n_shard;
                best_load    = load;
            }
        }

        load_vec.at(n_best_shard) = best_load;

        frame_set_vec.at(n_best_shard).insert(group.frame_index);

        for (const size_t n_entry : group.n_entry_vec)
        {
            result.at(n_entry) = n_best_shard;
        }
    }

    return result;
}

Utils::Images::Image Stereo::Replay::ConsistencyChecker::OverlayProbe::make_highlighted_image(const Utils::Images::Image& in_baseline_image,
                                                                                              const Utils::Images::Image& in_probe_image,
                                                                                              const bool&                 in_is_rest_dimmed)
{
    constexpr size_t N_CHANNELS = 4; /* B, G, R, A */

    Utils::Images::Image result = in_baseline_image;

    for (size_t n_pixel = 0;
                n_pixel < static_cast<size_t>(result.width) * result.height;
              ++n_pixel)
    {
        uint8_t* const pixel_ptr  = result.pixel_u8_vec.data() + n_pixel * N_CHANNELS;
        const bool     is_changed = (::memcmp(pixel_ptr,
                                              in_probe_image.pixel_u8_vec.data() + n_pixel * N_CHANNELS,
                                              N_CHANNELS) != 0);

        if (is_changed)
        {
            pixel_ptr[0] = 255;
            pixel_ptr[1] = 0;
            pixel_ptr[2] = 255;
        }
        else
        if (in_is_rest_dimmed)
        {
            pixel_ptr[0] /= 2;
            pixel_ptr[1] /= 2;
            pixel_ptr[2] /= 2;
        }
    }

    return result;
}

std::string Stereo::Replay::ConsistencyChecker::OverlayProbe::make_image_filename(const size_t&   in_n_candidate,
                                                                                  const uint32_t& in_frame_index,
                                                                                  const char*     in_extension_ptr)
{
    char text[64];

    ::snprintf(text,
               sizeof(text),
               "candidate_%03zu_frame_%u.%s",
               in_n_candidate,
               in_frame_index,
               in_extension_ptr);

    return text;
}

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::parse_candidates(const Options&                  in_options,
                                                                        std::vector<OverlayProbeEntry>* out_entry_vec_ptr,
                                                                        std::string*                    out_error_ptr)
{
    Utils::Json::JsonValue candidates;
    bool                   result     = false;
    std::string            text;

    out_entry_vec_ptr->clear();

    if (Utils::Json::JsonReader::read_file(in_options.overlay_probe_filename,
                                           &text) == false)
    {
        *out_error_ptr = "The file " + in_options.overlay_probe_filename + " cannot be read.";

        goto end;
    }

    if (Utils::Json::JsonReader::parse(Utils::Json::JsonReader::strip_byte_order_mark(text),
                                       &candidates,
                                       out_error_ptr) == false)
    {
        goto end;
    }

    for (const auto& candidate : candidates.at("candidates").item_vec)
    {
        OverlayProbeEntry entry;

        /* A draw is left out in its own pass only, so the list must say which pass that is (the lists that RecipeBake --overlays made before did not). */
        if (candidate.find_ptr("pass_key") == nullptr ||
            parse_hex_key(candidate.at("pass_key").string_value,
                         &entry.pass_key) == false)
        {
            *out_error_ptr = "A candidate of " + in_options.overlay_probe_filename + " has no pass_key: make the list again with RecipeBake --overlays.";

            goto end;
        }

        /* A candidate which no frame shows cannot be seen. */
        if (candidate.find_ptr("first_frame_index") == nullptr                                                        ||
            parse_hex_key(candidate.at("signature_key").string_value,
                         &entry.signature_key)      == false                                                          ||
            parse_hex_key(candidate.at("structure_key").string_value,
                         &entry.structure_key)      == false)
        {
            continue;
        }

        entry.agreement    = candidate.at("agreement").string_value;
        entry.frame_index  = static_cast<uint32_t>(candidate.at("first_frame_index").number_value);
        entry.index        = static_cast<uint32_t>(out_entry_vec_ptr->size() );
        entry.live_class   = candidate.at("live_class").string_value;
        entry.n_frames     = static_cast<uint64_t>(candidate.at("n_frames").number_value);
        entry.pass_id      = static_cast<uint64_t>(candidate.at("pass_id").number_value);
        entry.replay_class = candidate.at("replay_class").string_value;

        /* How deep the vertices of the pass of the candidate are, if the list says. */
        const auto* depth_ptr = candidate.find_ptr("depth");

        if (depth_ptr != nullptr)
        {
            read_depth_spread(*depth_ptr,
                             &entry.depth_spread);
        }

        for (const auto& reason : candidate.at("reasons").item_vec)
        {
            entry.reason_vec.push_back(reason.string_value);
        }

        out_entry_vec_ptr->push_back(entry);
    }

    result = true;
end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::parse_hex_key(const std::string& in_text,
                                                                     uint64_t*          out_key_ptr)
{
    char* end_ptr = nullptr;
    bool  result  = (in_text.size() > 2 &&
                     in_text.compare(0,
                                     2,
                                     "0x") == 0);

    if (result)
    {
        *out_key_ptr = ::strtoull(in_text.c_str() + 2,
                                  &end_ptr,
                                  16);
        result       = (end_ptr != nullptr &&
                       *end_ptr == '\0');
    }

    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::parse_result_entries(const std::string&              in_filename,
                                                                            std::vector<OverlayProbeEntry>* inout_entry_vec_ptr,
                                                                            std::string*                    out_error_ptr)
{
    Utils::Json::JsonValue list;
    bool                   result = false;
    std::string            text;

    if (Utils::Json::JsonReader::read_file(in_filename,
                                           &text) == false)
    {
        *out_error_ptr = "The result " + in_filename + " of a part of the probe cannot be read.";

        goto end;
    }

    if (Utils::Json::JsonReader::parse(Utils::Json::JsonReader::strip_byte_order_mark(text),
                                       &list,
                                       out_error_ptr) == false)
    {
        goto end;
    }

    for (const auto& item : list.at("candidates").item_vec)
    {
        OverlayProbeEntry entry;

        if (parse_hex_key(item.at("signature_key").string_value,
                         &entry.signature_key) == false ||
            parse_hex_key(item.at("structure_key").string_value,
                         &entry.structure_key) == false ||
            parse_hex_key(item.at("pass_key").string_value,
                         &entry.pass_key)      == false)
        {
            *out_error_ptr = "The result " + in_filename + " of a part of the probe has a key which is not one.";

            goto end;
        }

        entry.agreement          = item.at("agreement").string_value;
        entry.animation_filename = item.at("animation").string_value;
        entry.frame_index        = static_cast<uint32_t>(item.at("probed_frame_index").number_value);
        entry.image_filename     = item.at("image").string_value;
        entry.index              = static_cast<uint32_t>(item.at("index").number_value);
        entry.live_class         = item.at("live_class").string_value;
        entry.n_changed_pixels   = static_cast<uint64_t>(item.at("n_changed_pixels").number_value);
        entry.n_frames           = static_cast<uint64_t>(item.at("n_frames").number_value);
        entry.n_picture_pixels   = static_cast<uint64_t>(item.at("n_picture_pixels").number_value);
        entry.pass_id            = static_cast<uint64_t>(item.at("pass_id").number_value);
        entry.replay_class       = item.at("replay_class").string_value;

        read_depth_spread(item.at("depth"),
                         &entry.depth_spread);

        for (const auto& reason : item.at("reasons").item_vec)
        {
            entry.reason_vec.push_back(reason.string_value);
        }

        for (const auto& warning : item.at("warnings").item_vec)
        {
            entry.warning_vec.push_back(warning.string_value);
        }

        inout_entry_vec_ptr->push_back(entry);
    }

    result = true;
end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::probe_entries(const Options&                  in_options,
                                                                     const RenderFunc&               in_render_func,
                                                                     std::vector<OverlayProbeEntry>* inout_entry_vec_ptr,
                                                                     std::string*                    out_error_ptr)
{
    std::map<uint32_t, Utils::Images::Image> baseline_image_map;
    std::map<std::pair<uint32_t, uint64_t>, bool>          is_pass_visible_map; /* by the frame and the key of the pass */
    std::map<std::pair<uint32_t, uint64_t>, size_t>        n_draws_of_pass_map;
    bool                                                   result = false;

    /* The candidates go to the shards by what they cost (the draws of a pass in a frame stay together), so that the shards are done at about the same time. */
    if (in_options.overlay_probe_shard_count != 0)
    {
        const auto                     shard_of_entry_vec = get_shard_of_entry_vec(*inout_entry_vec_ptr,
                                                                                   in_options.overlay_probe_shard_count);
        std::vector<OverlayProbeEntry> shard_entry_vec;

        for (const auto& entry : *inout_entry_vec_ptr)
        {
            if (shard_of_entry_vec.at(entry.index) == in_options.overlay_probe_shard_index)
            {
                shard_entry_vec.push_back(entry);
            }
        }

        *inout_entry_vec_ptr = shard_entry_vec;
    }

    for (const auto& entry : *inout_entry_vec_ptr)
    {
        n_draws_of_pass_map[std::make_pair(entry.frame_index,
                                           entry.pass_key)]++;
    }

    for (size_t n_entry = 0;
                n_entry < inout_entry_vec_ptr->size();
              ++n_entry)
    {
        auto&                entry         = inout_entry_vec_ptr->at(n_entry);
        const auto           pass_pair     = std::make_pair(entry.frame_index,
                                                            entry.pass_key);
        Options              probe_options = in_options;
        Utils::Images::Image probe_image;

        /* The frame as it is: once for each frame, whichever candidates are seen in it. */
        if (baseline_image_map.count(entry.frame_index) == 0)
        {
            if (in_render_func(in_options,
                               entry.frame_index,
                              &baseline_image_map[entry.frame_index],
                               out_error_ptr) == false)
            {
                goto end;
            }
        }

        const auto& baseline_image = baseline_image_map.at(entry.frame_index);

        entry.n_picture_pixels = static_cast<uint64_t>(baseline_image.width) * baseline_image.height;

        /* A pass that makes no difference left out whole makes none draw by draw: the first of its draws in the frame finds that out, for all of them. */
        if (n_draws_of_pass_map.at(pass_pair) > 1)
        {
            if (is_pass_visible_map.count(pass_pair) == 0)
            {
                Options              pass_options = in_options;
                Utils::Images::Image pass_image;

                pass_options.recipe.skipped.pass_key_vec.push_back(entry.pass_key);

                if (in_render_func(pass_options,
                                   entry.frame_index,
                                  &pass_image,
                                   out_error_ptr) == false)
                {
                    goto end;
                }

                is_pass_visible_map[pass_pair] = (Utils::Images::ImageComparer::compare(baseline_image,
                                                                                        pass_image).n_differing_pixels != 0);
            }

            if (is_pass_visible_map.at(pass_pair) == false)
            {
                add_warnings(&entry);

                fprintf(stdout,
                        "%u of %zu: draws %s of the frame %u: the pass makes no pixels, and so neither do they\n",
                        entry.index + 1,
                        inout_entry_vec_ptr->size(),
                        get_hex_text(entry.structure_key).c_str(),
                        entry.frame_index);

                continue;
            }
        }

        /* The draw is left out in its pass, not in the other passes which have a draw of its structure (a pass that draws a scene may be one of them). */
        Analysis::Recipe::PassDraw pass_draw;

        pass_draw.draw_key = entry.structure_key;
        pass_draw.pass_key = entry.pass_key;

        probe_options.recipe.skipped.pass_draw_vec.push_back(pass_draw);

        if (in_render_func(probe_options,
                           entry.frame_index,
                          &probe_image,
                           out_error_ptr) == false)
        {
            goto end;
        }

        {
            const auto difference = Utils::Images::ImageComparer::compare(baseline_image,
                                                                           probe_image);

            entry.n_changed_pixels = difference.n_differing_pixels;

            add_warnings(&entry);

            if (difference.n_differing_pixels != 0)
            {
                entry.animation_filename = make_image_filename(entry.index,
                                                               entry.frame_index,
                                                               "gif");
                entry.image_filename     = make_image_filename(entry.index,
                                                               entry.frame_index,
                                                               "png");

                if (write_images(in_options.dump_directory,
                                 entry.image_filename,
                                 entry.animation_filename,
                                 baseline_image,
                                 probe_image) == false)
                {
                    *out_error_ptr = "The image " + entry.image_filename + " cannot be written to " + in_options.dump_directory + ".";

                    goto end;
                }
            }
        }

        fprintf(stdout,
                "%u of %zu: draws %s of the frame %u: %llu pixels change without them\n",
                entry.index + 1,
                inout_entry_vec_ptr->size(),
                get_hex_text(entry.structure_key).c_str(),
                entry.frame_index,
                static_cast<unsigned long long>(entry.n_changed_pixels) );
    }

    result = true;
end:
    return result;
}

void Stereo::Replay::ConsistencyChecker::OverlayProbe::read_depth_spread(const Utils::Json::JsonValue&               in_depth,
                                                                         Analysis::APIDumpAnalysis::PassDepthSpread* out_spread_ptr)
{
    out_spread_ptr->has_depth = in_depth.at("has_depth").bool_value;

    if (out_spread_ptr->has_depth)
    {
        out_spread_ptr->depth_far         = static_cast<float>(in_depth.at("far").number_value);
        out_spread_ptr->depth_median      = static_cast<float>(in_depth.at("median").number_value);
        out_spread_ptr->depth_near        = static_cast<float>(in_depth.at("near").number_value);
        out_spread_ptr->far_to_near_ratio = static_cast<float>(in_depth.at("far_to_near_ratio").number_value);
        out_spread_ptr->is_flat           = in_depth.at("is_flat").bool_value;
    }
}

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::run(const Options&       in_options,
                                                           const RenderFunc&    in_render_func,
                                                           const RunShardsFunc& in_run_shards_func,
                                                           std::string*         out_error_ptr)
{
    std::vector<OverlayProbeEntry> entry_vec;
    uint32_t                       n_shards  = 1;
    bool                           result    = false;

    /* The directory of the output is made if it is not there (one level). */
    ::CreateDirectoryA(in_options.dump_directory.c_str(),
                       nullptr);

    if (parse_candidates(in_options,
                        &entry_vec,
                         out_error_ptr) == false)
    {
        goto end;
    }

    n_shards = get_n_shards(in_options,
                            entry_vec);

    /* The parts of the work are done by processes of their own (each replay of the workload decodes it from its start, which is most of the time), and the
     * results are put together here.
     */
    if (in_options.overlay_probe_shard_count == 0 &&
        n_shards                             >  1 &&
        in_run_shards_func)
    {
        std::vector<std::string> part_filename_vec;

        for (uint32_t n_shard = 0;
                      n_shard < n_shards;
                    ++n_shard)
        {
            part_filename_vec.push_back(in_options.dump_directory + "/part_" + std::to_string(n_shard) + ".json");
        }

        if (in_run_shards_func(n_shards,
                               part_filename_vec,
                               out_error_ptr) == false)
        {
            goto end;
        }

        entry_vec.clear();

        for (const auto& part_filename : part_filename_vec)
        {
            const bool is_read = parse_result_entries(part_filename,
                                                     &entry_vec,
                                                      out_error_ptr);

            ::remove(part_filename.c_str() );

            if (is_read == false)
            {
                goto end;
            }
        }

        std::sort(entry_vec.begin(),
                  entry_vec.end  (),
                  [](const OverlayProbeEntry& in_entry_a,
                     const OverlayProbeEntry& in_entry_b)
                  {
                      return (in_entry_a.index < in_entry_b.index);
                  });
    }
    else
    if (probe_entries(in_options,
                      in_render_func,
                     &entry_vec,
                      out_error_ptr) == false)
    {
        goto end;
    }

    /* A part of the work leaves its results to the one that put the parts together. */
    if (in_options.overlay_probe_result_filename.empty() == false)
    {
        if (write_list(in_options.overlay_probe_result_filename,
                       entry_vec,
                       out_error_ptr) == false)
        {
            goto end;
        }
    }
    else
    {
        if (write_list(in_options.dump_directory + "/candidates.json",
                       entry_vec,
                       out_error_ptr) == false)
        {
            goto end;
        }

        if (write_page(in_options.dump_directory,
                       entry_vec,
                       out_error_ptr) == false)
        {
            goto end;
        }
    }

    result = true;
end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::write_images(const std::string&          in_directory,
                                                                    const std::string&          in_still_filename,
                                                                    const std::string&          in_animation_filename,
                                                                    const Utils::Images::Image& in_baseline_image,
                                                                    const Utils::Images::Image& in_probe_image)
{
    constexpr uint32_t ANIMATION_FRAME_DELAY_N_MILLISECONDS = 700;

    /* The still has the pixels which the draws make in magenta, and the rest of the frame dimmed so that they show. The animation shows the frame as it is, and the
     * same frame with those pixels in magenta, by turns, so that what is magenta can be told from what is under it.
     */
    const auto marked_image = make_highlighted_image(in_baseline_image,
                                                     in_probe_image,
                                                     false); /* in_is_rest_dimmed */
    const auto still_image  = make_highlighted_image(in_baseline_image,
                                                     in_probe_image,
                                                     true); /* in_is_rest_dimmed */

    return (Utils::Images::PngWriter::write_file(in_directory + "/" + in_still_filename,
                                                 still_image) &&
            Utils::Images::GifWriter::write_file(in_directory + "/" + in_animation_filename,
                                                 {in_baseline_image, marked_image},
                                                 ANIMATION_FRAME_DELAY_N_MILLISECONDS) );
}

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::write_list(const std::string&                    in_filename,
                                                                  const std::vector<OverlayProbeEntry>& in_entry_vec,
                                                                  std::string*                          out_error_ptr)
{
    bool                                  result = false;
    Utils::Json::JsonWriter writer(true); /* in_is_pretty */

    writer.begin_object();
    writer.write_key("candidates");
    writer.begin_array();

    for (const auto& entry : in_entry_vec)
    {
        writer.begin_object();

        writer.write_key    ("index");
        writer.write_u64    (entry.index);
        writer.write_key    ("signature_key");
        writer.write_hex_u64(entry.signature_key);
        writer.write_key    ("structure_key");
        writer.write_hex_u64(entry.structure_key);
        writer.write_key    ("pass_id");
        writer.write_u64    (entry.pass_id);
        writer.write_key    ("pass_key");
        writer.write_hex_u64(entry.pass_key);
        writer.write_key    ("n_frames");
        writer.write_u64    (entry.n_frames);
        writer.write_key    ("probed_frame_index");
        writer.write_u64    (entry.frame_index);
        writer.write_key    ("n_changed_pixels");
        writer.write_u64    (entry.n_changed_pixels);
        writer.write_key    ("image");
        writer.write_string (entry.image_filename);
        writer.write_key    ("animation");
        writer.write_string (entry.animation_filename);
        writer.write_key    ("agreement");
        writer.write_string (entry.agreement);
        writer.write_key    ("replay_class");
        writer.write_string (entry.replay_class);
        writer.write_key    ("live_class");
        writer.write_string (entry.live_class);

        Analysis::APIDumpAnalysis::QueryCommands::write_depth_spread(entry.depth_spread,
                                                                      &writer);

        writer.write_key    ("n_picture_pixels");
        writer.write_u64    (entry.n_picture_pixels);

        writer.write_key("reasons");
        writer.begin_inline_array();

        for (const auto& reason : entry.reason_vec)
        {
            writer.write_string(reason);
        }

        writer.end_array();

        writer.write_key("warnings");
        writer.begin_inline_array();

        for (const auto& warning : entry.warning_vec)
        {
            writer.write_string(warning);
        }

        writer.end_array();
        writer.end_object();
    }

    writer.end_array();
    writer.end_object();

    if (writer.has_failed() ||
        write_text_file(in_filename,
                        writer.get_text() ) == false)
    {
        *out_error_ptr = "The file " + in_filename + " cannot be written.";

        goto end;
    }

    result = true;
end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::write_page(const std::string&                    in_directory,
                                                                  const std::vector<OverlayProbeEntry>& in_entry_vec,
                                                                  std::string*                          out_error_ptr)
{
    std::string page;
    bool        result = false;

    page = "<!doctype html>\n<html><head><meta charset=\"utf-8\"><title>Overlay candidates</title>\n"
           "<style>body{font:14px sans-serif;margin:16px;background:#222;color:#ddd}"
           ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(340px,1fr));gap:12px}"
           ".card{background:#333;padding:8px;border-radius:6px}.card img{width:100%;image-rendering:pixelated}"
           ".card.none{opacity:.55}.card.suspect{outline:2px solid #e55}code{color:#9cf}button{margin-left:6px}"
           ".badge{display:inline-block;margin:2px 4px 2px 0;padding:1px 6px;border-radius:4px;background:#a33;color:#fff;font-size:12px}</style></head><body>\n"
           "<h1>Draws that may be 2D overlays</h1>\n"
           "<p>For each candidate the frame it is first drawn in was replayed (left eye) with it and without it. Each image alternates between the frame as it is and the same frame with what the draws make in magenta (click it for its size; the link below it is a still with the rest dimmed). "
           "A candidate that changes no pixel is not drawn in that frame (or draws what is hidden). An overlay is flat: its vertices are at one depth, and it makes a part of the "
           "picture. A red outline is a draw that the recipe leaves alone and that is not like that: <em>deep_depth</em> (the vertices of its pass are at many depths: a 3D scene) "
           "or <em>covers_much_of_picture</em> (it makes more than two fifths of the picture). Look at the image: if it is the scene, the recipe is wrong.</p>\n"
           "<p><label>Show: <select id=\"filter\"><option value=\"all\">all</option><option value=\"visible\">changes the picture</option>"
           "<option value=\"suspect\">to look at twice</option><option value=\"disagree\">the opinions disagree</option></select></label></p>\n<div class=\"grid\">\n";

    for (const auto& entry : in_entry_vec)
    {
        const bool is_agreed = (entry.agreement == "both");

        const bool is_suspect = (entry.warning_vec.empty() == false);

        page += "<div class=\"card" + std::string((entry.n_changed_pixels == 0) ? " none" : "") + std::string((is_suspect) ? " suspect" : "") + "\" data-visible=\"" +
                ((entry.n_changed_pixels != 0) ? "1" : "0") + "\" data-disagree=\"" + ((is_agreed) ? "0" : "1") + "\" data-suspect=\"" + ((is_suspect) ? "1" : "0") + "\">";

        if (entry.animation_filename.empty() == false)
        {
            /* The animation shows the frame and the frame with the draws in magenta, by turns; the still dims the rest of the frame. */
            page += "<a href=\"" + escape_html(entry.animation_filename) + "\"><img src=\"" + escape_html(entry.animation_filename) + "\" loading=\"lazy\"></a>"
                    "<div><a href=\"" + escape_html(entry.image_filename) + "\">still</a></div>";
        }

        page += "<div>draw <code>" + get_hex_text(entry.structure_key) + "</code><button onclick=\"navigator.clipboard.writeText('" + get_hex_text(entry.structure_key) +
                "')\">copy key</button></div>"
                "<div>frame " + std::to_string(entry.frame_index) + ", in " + std::to_string(entry.n_frames) + " frames, pass " + std::to_string(entry.pass_id) + "</div>"
                "<div>" + std::to_string(entry.n_changed_pixels) + " pixels change; opinions: " + escape_html(entry.agreement) + " (replay " + escape_html(entry.replay_class) +
                ", live " + escape_html(entry.live_class) + ")</div>";

        if (entry.n_picture_pixels != 0)
        {
            char fraction_text[32];

            ::snprintf(fraction_text,
                       sizeof(fraction_text),
                       "%.1f",
                       100.0 * static_cast<double>(entry.n_changed_pixels) / static_cast<double>(entry.n_picture_pixels) );

            page += "<div>makes " + std::string(fraction_text) + "% of the picture</div>";
        }

        if (entry.depth_spread.has_depth)
        {
            char depth_text[96];

            ::snprintf(depth_text,
                       sizeof(depth_text),
                       "vertices of its pass: depth %.1f to %.1f, %.2f times (%s)",
                       entry.depth_spread.depth_near,
                       entry.depth_spread.depth_far,
                       entry.depth_spread.far_to_near_ratio,
                       (entry.depth_spread.is_flat) ? "flat" : "deep");

            page += "<div>" + std::string(depth_text) + "</div>";
        }

        for (const auto& warning : entry.warning_vec)
        {
            page += "<span class=\"badge\">" + escape_html(warning) + "</span>";
        }

        for (const auto& reason : entry.reason_vec)
        {
            page += "<div>- " + escape_html(reason) + "</div>";
        }

        page += "</div>\n";
    }

    page += "</div>\n<script>document.getElementById('filter').onchange=function(){var v=this.value;"
            "document.querySelectorAll('.card').forEach(function(c){c.style.display=(v=='all'||(v=='visible'&&c.dataset.visible=='1')||(v=='suspect'&&c.dataset.suspect=='1')||(v=='disagree'&&c.dataset.disagree=='1'))?'':'none';});};"
            "</script></body></html>\n";

    if (write_text_file(in_directory + "/index.html",
                        page) == false)
    {
        *out_error_ptr = "The file index.html cannot be written to " + in_directory + ".";

        goto end;
    }

    result = true;
end:
    return result;
}
