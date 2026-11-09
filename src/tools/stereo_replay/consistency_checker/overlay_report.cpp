/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "overlay_report.h"
#include "overlay_report_page.h"
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

void Stereo::Replay::ConsistencyChecker::OverlayReport::add_warnings(OverlayReportEntry* inout_entry_ptr)
{
    /* An overlay is a part of the picture: a draw that makes most of it is either a fade, or a scene which is taken for an overlay. */
    constexpr double MUCH_OF_PICTURE_FRACTION = 0.4;

    const bool is_left_alone = (inout_entry_ptr->replay_class == "unaffected" ||
                                inout_entry_ptr->live_class   == "unaffected" ||
                                inout_entry_ptr->live_class   == "mixed");

    if (is_left_alone)
    {
        if (inout_entry_ptr->depth_spread.has_depth          &&
            inout_entry_ptr->depth_spread.is_flat   == false)
        {
            inout_entry_ptr->warning_vec.push_back("deep_depth");
        }

        if (inout_entry_ptr->n_picture_pixels                      != 0                                                                                 &&
            static_cast<double>(inout_entry_ptr->n_changed_pixels) >  MUCH_OF_PICTURE_FRACTION * static_cast<double>(inout_entry_ptr->n_picture_pixels) )
        {
            inout_entry_ptr->warning_vec.push_back("covers_much_of_picture");
        }
    }
}

bool Stereo::Replay::ConsistencyChecker::OverlayReport::find_zero_cause(const Options&           in_options,
                                                                       const Options&           in_probe_options,
                                                                       const RenderFunc&        in_render_func,
                                                                       const OverlayReportEntry& in_entry,
                                                                       std::string*             out_cause_ptr,
                                                                       std::string*             out_error_ptr)
{
    Options              cut_options       = in_options;
    Options              cut_probe_options = in_probe_options;
    Utils::Images::Image with_image;
    Utils::Images::Image without_image;
    bool                 result            = false;

    /* The frame is cut after the last call of the draw, so that what it made can be seen before what comes after covers it. */
    cut_options.has_truncation          = true;
    cut_options.truncate_draw_key       = in_entry.structure_key;
    cut_options.truncate_pass_key       = in_entry.pass_key;
    cut_probe_options.has_truncation    = true;
    cut_probe_options.truncate_draw_key = in_entry.structure_key;
    cut_probe_options.truncate_pass_key = in_entry.pass_key;

    if (in_render_func(cut_options,
                       in_entry.frame_index,
                      &with_image,
                       out_error_ptr) == false)
    {
        goto end;
    }

    if (in_render_func(cut_probe_options,
                       in_entry.frame_index,
                      &without_image,
                       out_error_ptr) == false)
    {
        goto end;
    }

    {
        const auto difference = Utils::Images::ImageComparer::compare(with_image,
                                                                      without_image);

        *out_cause_ptr = (difference.n_differing_pixels != 0) ? "overdrawn_later"
                                                              : "not_rasterized";
    }

    result = true;
end:
    return result;
}

uint32_t Stereo::Replay::ConsistencyChecker::OverlayReport::get_n_shards(const Options&                        in_options,
                                                                        const std::vector<OverlayReportEntry>& in_entry_vec)
{
    return std::max<uint32_t>(1,
                              std::min<uint32_t>(in_options.n_jobs,
                                                 static_cast<uint32_t>(get_pass_group_map(in_entry_vec).size() ) ) );
}

std::map<std::pair<uint32_t, uint64_t>, std::vector<size_t> > Stereo::Replay::ConsistencyChecker::OverlayReport::get_pass_group_map(const std::vector<OverlayReportEntry>& in_entry_vec)
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

std::vector<uint32_t> Stereo::Replay::ConsistencyChecker::OverlayReport::get_shard_of_entry_vec(const std::vector<OverlayReportEntry>& in_entry_vec,
                                                                                               const uint32_t&                       in_n_shards)
{
    /* A replay costs the frames it goes through, and a few more for the device it makes. */
    constexpr uint64_t N_FRAMES_OF_SETUP = 1000;

    struct Group
    {
        uint32_t            frame_index = 0;
        std::vector<size_t> n_entry_vec;
    };

    std::vector<std::set<uint32_t> > frame_set_vec(in_n_shards);
    std::vector<Group>               group_vec;
    std::vector<uint64_t>            load_vec     (in_n_shards,
                                                   0);
    std::vector<uint32_t>            result       (in_entry_vec.size(),
                                                   0);

    for (const auto& group_pair : get_pass_group_map(in_entry_vec) )
    {
        Group group;

        group.frame_index = group_pair.first.first;
        group.n_entry_vec = group_pair.second;

        group_vec.push_back(group);
    }

    {
        const auto is_group_later_func = [](const Group& in_group_a,
                                            const Group& in_group_b)
        {
            return (in_group_a.frame_index > in_group_b.frame_index);
        };

        /* The frames that cost most go first, each group to the shard that would be done with it soonest, which is the shard with the least to do unless the frame is
         * made there already (the frame as it is is made once for each shard that has a draw of it). Only the list says it, so every process comes to the same.
         */
        std::stable_sort(group_vec.begin(),
                         group_vec.end  (),
                         is_group_later_func);
    }

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

            if (n_shard == 0         ||
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

Utils::Images::Image Stereo::Replay::ConsistencyChecker::OverlayReport::make_highlighted_image(const Utils::Images::Image& in_baseline_image,
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

std::string Stereo::Replay::ConsistencyChecker::OverlayReport::make_image_filename(const size_t&   in_n_candidate,
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

bool Stereo::Replay::ConsistencyChecker::OverlayReport::parse_candidates(const Options&                  in_options,
                                                                        std::vector<OverlayReportEntry>* out_entry_vec_ptr,
                                                                        std::string*                    out_error_ptr)
{
    Utils::Json::JsonValue candidates;
    bool                   result     = false;
    std::string            text;

    out_entry_vec_ptr->clear();

    if (Utils::Json::JsonReader::read_file(in_options.overlay_report_filename,
                                           &text) == false)
    {
        *out_error_ptr = "The file " + in_options.overlay_report_filename + " cannot be read.";

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
        OverlayReportEntry entry;

        /* A draw is left out in its own pass only, so the list must say which pass that is (the lists that RecipeBake --overlays made before did not). */
        const auto* pass_key_ptr      = candidate.find_ptr("pass_key");
        bool        is_pass_key_valid = false;

        if (pass_key_ptr != nullptr)
        {
            is_pass_key_valid = parse_hex_key(pass_key_ptr->string_value,
                                             &entry.pass_key);
        }

        if (is_pass_key_valid == false)
        {
            *out_error_ptr = "A candidate of " + in_options.overlay_report_filename + " has no pass_key: make the list again with RecipeBake --overlays.";

            goto end;
        }

        /* A candidate which no frame shows cannot be seen. */
        const auto* first_frame_ptr    = candidate.find_ptr("first_frame_index");
        bool        is_candidate_shown = false;

        if (first_frame_ptr != nullptr)
        {
            const std::string& signature_key_text = candidate.at ("signature_key").string_value;
            const std::string& structure_key_text = candidate.at ("structure_key").string_value;
            const bool         is_signature_valid = parse_hex_key(signature_key_text,
                                                                 &entry.signature_key);

            if (is_signature_valid)
            {
                is_candidate_shown = parse_hex_key(structure_key_text,
                                                  &entry.structure_key);
            }
        }

        if (is_candidate_shown == false)
        {
            continue;
        }

        entry.agreement    = candidate.at         ("agreement").string_value;
        entry.live_class   = candidate.at         ("live_class").string_value;
        entry.n_frames     = static_cast<uint64_t>(candidate.at("n_frames").number_value);
        entry.pass_id      = static_cast<uint64_t>(candidate.at("pass_id").number_value);
        entry.replay_class = candidate.at         ("replay_class").string_value;

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

        /* The frames to look at: the ones the list says, spread over the frames that the draw is in, or the first of them. */
        std::vector<uint32_t> frame_index_vec;
        const auto*           sample_ptr = candidate.find_ptr("sample_frame_indices");

        if (sample_ptr != nullptr)
        {
            for (const auto& sample : sample_ptr->item_vec)
            {
                frame_index_vec.push_back(static_cast<uint32_t>(sample.number_value) );
            }
        }
        else
        {
            frame_index_vec.push_back(static_cast<uint32_t>(candidate.at("first_frame_index").number_value) );
        }

        for (const auto frame_index : frame_index_vec)
        {
            if (frame_index <= in_options.overlay_report_last_frame)
            {
                entry.frame_index = frame_index;
                entry.index       = static_cast<uint32_t>(out_entry_vec_ptr->size() );

                out_entry_vec_ptr->push_back(entry);
            }
        }
    }

    result = true;
end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayReport::parse_hex_key(const std::string& in_text,
                                                                     uint64_t*          out_key_ptr)
{
    char*        end_ptr      = nullptr;
    const size_t n_characters = in_text.size();
    bool         result       = false;

    if (n_characters > 2)
    {
        const int prefix_difference = in_text.compare(0,
                                                      2,
                                                      "0x");

        result = (prefix_difference == 0);
    }

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

bool Stereo::Replay::ConsistencyChecker::OverlayReport::parse_result_entries(const std::string&              in_filename,
                                                                            std::vector<OverlayReportEntry>* inout_entry_vec_ptr,
                                                                            std::string*                    out_error_ptr)
{
    Utils::Json::JsonValue list;
    bool                   result = false;
    std::string            text;

    if (Utils::Json::JsonReader::read_file(in_filename,
                                           &text) == false)
    {
        *out_error_ptr = "The result " + in_filename + " of a part of the report cannot be read.";

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
        OverlayReportEntry entry;

        const std::string& pass_key_text      = item.at("pass_key").string_value;
        const std::string& signature_key_text = item.at("signature_key").string_value;
        const std::string& structure_key_text = item.at("structure_key").string_value;

        const bool is_pass_key_valid      = parse_hex_key(pass_key_text,
                                                         &entry.pass_key);
        const bool is_signature_key_valid = parse_hex_key(signature_key_text,
                                                         &entry.signature_key);
        const bool is_structure_key_valid = parse_hex_key(structure_key_text,
                                                         &entry.structure_key);

        if (is_pass_key_valid      == false ||
            is_signature_key_valid == false ||
            is_structure_key_valid == false)
        {
            *out_error_ptr = "The result "                                         +
                             in_filename                                           +
                             " of a part of the report has a key which is not one.";

            goto end;
        }

        entry.agreement          = item.at              ("agreement").string_value;
        entry.animation_filename = item.at              ("animation").string_value;
        entry.frame_index        = static_cast<uint32_t>(item.at("reported_frame_index").number_value);
        entry.image_filename     = item.at              ("image").string_value;
        entry.index              = static_cast<uint32_t>(item.at("index").number_value);
        entry.live_class         = item.at              ("live_class").string_value;
        entry.n_changed_pixels   = static_cast<uint64_t>(item.at("n_changed_pixels").number_value);
        entry.n_frames           = static_cast<uint64_t>(item.at("n_frames").number_value);
        entry.n_picture_pixels   = static_cast<uint64_t>(item.at("n_picture_pixels").number_value);
        entry.pass_id            = static_cast<uint64_t>(item.at("pass_id").number_value);
        entry.replay_class       = item.at              ("replay_class").string_value;

        const auto* cause_ptr = item.find_ptr("zero_cause");

        if (cause_ptr != nullptr)
        {
            entry.zero_cause = cause_ptr->string_value;
        }

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

void Stereo::Replay::ConsistencyChecker::OverlayReport::print_entry(const OverlayReportEntry& in_entry,
                                                                    const size_t&             in_n_entries)
{
    fprintf(stdout,
            "%u of %zu: draws %s of the frame %u: %llu pixels change without them%s%s\n",
            in_entry.index + 1,
            in_n_entries,
            OverlayReportPage::get_hex_text(in_entry.structure_key).c_str(),
            in_entry.frame_index,
            static_cast<unsigned long long>(in_entry.n_changed_pixels),
            (in_entry.zero_cause.empty() ) ? "" : " (",
            (in_entry.zero_cause.empty() ) ? "" : (in_entry.zero_cause + ")").c_str() );
}

bool Stereo::Replay::ConsistencyChecker::OverlayReport::process_entries(const Options&                  in_options,
                                                                     const RenderFunc&               in_render_func,
                                                                     const QuickPath*                in_quick_path_ptr,
                                                                     std::vector<OverlayReportEntry>* inout_entry_vec_ptr,
                                                                     std::string*                    out_error_ptr)
{
    std::map<uint32_t, Utils::Images::Image>        baseline_image_map;
    std::map<uint32_t, std::vector<size_t> >        entry_index_by_frame_map;
    std::map<std::pair<uint32_t, uint64_t>, bool>   is_pass_visible_map; /* by the frame and the key of the pass */
    std::map<std::pair<uint32_t, uint64_t>, size_t> n_draws_of_pass_map;
    bool                                            result              = false;

    /* The candidates go to the shards by what they cost (the draws of a pass in a frame stay together), so that the shards are done at about the same time. */
    if (in_options.overlay_report_shard_count != 0)
    {
        const auto                     shard_of_entry_vec = get_shard_of_entry_vec(*inout_entry_vec_ptr,
                                                                                   in_options.overlay_report_shard_count);
        std::vector<OverlayReportEntry> shard_entry_vec;

        for (const auto& entry : *inout_entry_vec_ptr)
        {
            if (shard_of_entry_vec.at(entry.index) == in_options.overlay_report_shard_index)
            {
                shard_entry_vec.push_back(entry);
            }
        }

        *inout_entry_vec_ptr = shard_entry_vec;
    }

    for (size_t n_entry = 0;
                n_entry < inout_entry_vec_ptr->size();
              ++n_entry)
    {
        const auto& entry = inout_entry_vec_ptr->at(n_entry);

        n_draws_of_pass_map[std::make_pair(entry.frame_index,
                                           entry.pass_key)]++;

        entry_index_by_frame_map[entry.frame_index].push_back(n_entry);
    }

    const bool is_quick_possible = (in_quick_path_ptr                          != nullptr &&
                                    in_quick_path_ptr->is_repeatable_func      != nullptr &&
                                    in_quick_path_ptr->render_variants_func    != nullptr &&
                                    in_options.overlay_report_is_exact         == false);

    /* A frame at a time, so that what is the same for the candidates of a frame is done once for it. */
    for (const auto& frame_pair : entry_index_by_frame_map)
    {
        bool is_played_quick = false;

        if (is_quick_possible)
        {
            std::string reason;

            if (in_quick_path_ptr->is_repeatable_func(frame_pair.first,
                                                     &reason) )
            {
                is_played_quick = process_frame_quick(in_options,
                                                      in_render_func,
                                                     *in_quick_path_ptr,
                                                      frame_pair.first,
                                                      frame_pair.second,
                                                      n_draws_of_pass_map,
                                                      inout_entry_vec_ptr,
                                                     &reason);
            }

            if (is_played_quick == false)
            {
                fprintf(stdout,
                        "Frame %u: its candidates are replayed one at a time: %s.\n",
                        frame_pair.first,
                        reason.c_str() );
            }
        }

        if (is_played_quick == false)
        {
            for (const size_t n_entry : frame_pair.second)
            {
                if (process_entry(in_options,
                                  in_render_func,
                                  n_draws_of_pass_map,
                                  inout_entry_vec_ptr->size(),
                                 &inout_entry_vec_ptr->at(n_entry),
                                 &baseline_image_map,
                                 &is_pass_visible_map,
                                  out_error_ptr) == false)
                {
                    goto end;
                }
            }
        }
    }

    result = true;
end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayReport::process_entry(const Options&                                         in_options,
                                                                      const RenderFunc&                                      in_render_func,
                                                                      const std::map<std::pair<uint32_t, uint64_t>, size_t>& in_n_draws_of_pass_map,
                                                                      const size_t&                                          in_n_entries,
                                                                      OverlayReportEntry*                                    inout_entry_ptr,
                                                                      std::map<uint32_t, Utils::Images::Image>*              inout_baseline_image_map_ptr,
                                                                      std::map<std::pair<uint32_t, uint64_t>, bool>*         inout_is_pass_visible_map_ptr,
                                                                      std::string*                                           out_error_ptr)
{
    OverlayReportEntry&  entry         = *inout_entry_ptr;
    const auto           pass_pair     = std::make_pair(entry.frame_index,
                                                        entry.pass_key);
    Options              probe_options = in_options;
    Utils::Images::Image probe_image;
    bool                 result        = false;

    /* The frame as it is: once for each frame, whichever candidates are seen in it. */
    if (inout_baseline_image_map_ptr->count(entry.frame_index) == 0)
    {
        if (in_render_func(in_options,
                           entry.frame_index,
                          &(*inout_baseline_image_map_ptr)[entry.frame_index],
                           out_error_ptr) == false)
        {
            goto end;
        }
    }

    {
        const auto& baseline_image = inout_baseline_image_map_ptr->at(entry.frame_index);

        entry.n_picture_pixels = static_cast<uint64_t>(baseline_image.width) * baseline_image.height;

        /* A pass that makes no difference left out whole makes none draw by draw: the first of its draws in the frame finds that out, for all of them. */
        if (in_n_draws_of_pass_map.at(pass_pair) > 1)
        {
            if (inout_is_pass_visible_map_ptr->count(pass_pair) == 0)
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

                (*inout_is_pass_visible_map_ptr)[pass_pair] = (Utils::Images::ImageComparer::compare(baseline_image,
                                                                                                     pass_image).n_differing_pixels != 0);
            }

            if (inout_is_pass_visible_map_ptr->at(pass_pair) == false)
            {
                add_warnings(&entry);

                entry.zero_cause = "pass_makes_no_pixels";

                fprintf(stdout,
                        "%u of %zu: draws %s of the frame %u: the pass makes no pixels, and so neither do they\n",
                        entry.index + 1,
                        in_n_entries,
                        OverlayReportPage::get_hex_text(entry.structure_key).c_str(),
                        entry.frame_index);

                result = true;

                goto end;
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

            if (difference.n_differing_pixels == 0)
            {
                if (find_zero_cause(in_options,
                                    probe_options,
                                    in_render_func,
                                    entry,
                                   &entry.zero_cause,
                                    out_error_ptr) == false)
                {
                    goto end;
                }
            }

            if (write_entry_images(in_options,
                                   &entry,
                                   baseline_image,
                                   probe_image,
                                   out_error_ptr) == false)
            {
                goto end;
            }
        }

        print_entry(entry,
                    in_n_entries);

        result = true;
    }

end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayReport::process_frame_quick(const Options&                                         in_options,
                                                                            const RenderFunc&                                      in_render_func,
                                                                            const QuickPath&                                       in_quick_path,
                                                                            const uint32_t&                                        in_frame_index,
                                                                            const std::vector<size_t>&                             in_entry_index_vec,
                                                                            const std::map<std::pair<uint32_t, uint64_t>, size_t>& in_n_draws_of_pass_map,
                                                                            std::vector<OverlayReportEntry>*                       inout_entry_vec_ptr,
                                                                            std::string*                                           out_error_ptr)
{
    std::vector<size_t>                                  cause_entry_vec;        /* by place in in_entry_index_vec: those that change no pixel */
    std::vector<size_t>                                  entry_variant_vec;      /* by place in in_entry_index_vec: the variant that leaves the draw out */
    std::vector<bool>                                    is_done_vec(in_entry_index_vec.size(),
                                                                     false);     /* said already: the pass makes no pixels */
    std::vector<Utils::Images::Image>                    image_vec;
    std::map<uint64_t, size_t>                           pass_variant_map;       /* by the key of the pass: the variant that leaves it out */
    bool                                                 result = false;
    std::vector<Stereo::Replay::EngineDLL::FrameVariant> variant_vec;

    /* The variants: the frame as it is, each pass that has more than one candidate left out, and each candidate left out. */
    variant_vec.push_back(Stereo::Replay::EngineDLL::FrameVariant() );

    for (const size_t n_entry : in_entry_index_vec)
    {
        const auto& entry     = inout_entry_vec_ptr->at(n_entry);
        const auto  pass_pair = std::make_pair(entry.frame_index,
                                               entry.pass_key);

        if (in_n_draws_of_pass_map.at(pass_pair) > 1 &&
            pass_variant_map.count(entry.pass_key)  == 0)
        {
            Stereo::Replay::EngineDLL::FrameVariant variant;

            variant.skipped_pass_key_vec.push_back(entry.pass_key);

            pass_variant_map[entry.pass_key] = variant_vec.size();

            variant_vec.push_back(variant);
        }
    }

    for (const size_t n_entry : in_entry_index_vec)
    {
        const auto&                             entry = inout_entry_vec_ptr->at(n_entry);
        Stereo::Replay::EngineDLL::FrameVariant variant;
        Analysis::Recipe::PassDraw              pass_draw;

        pass_draw.draw_key = entry.structure_key;
        pass_draw.pass_key = entry.pass_key;

        variant.skipped_pass_draw_vec.push_back(pass_draw);

        entry_variant_vec.push_back(variant_vec.size() );

        variant_vec.push_back(variant);
    }

    if (in_quick_path.render_variants_func(in_options,
                                           in_frame_index,
                                           variant_vec,
                                          &image_vec,
                                           out_error_ptr) == false)
    {
        goto end;
    }

    if (image_vec.size() != variant_vec.size() )
    {
        *out_error_ptr = "the frame gave " + std::to_string(image_vec.size() ) + " images for " + std::to_string(variant_vec.size() ) + " variants";

        goto end;
    }

    for (const auto& image : image_vec)
    {
        if (image.width  == 0 ||
            image.height == 0)
        {
            *out_error_ptr = "an image of the frame is missing";

            goto end;
        }
    }

    {
        const Utils::Images::Image& baseline_image = image_vec.at(0);

        /* What each candidate does to the picture, and which of them change none of it. */
        for (size_t n_place = 0;
                    n_place < in_entry_index_vec.size();
                  ++n_place)
        {
            auto&      entry     = inout_entry_vec_ptr->at(in_entry_index_vec.at(n_place) );
            const auto pass_pair = std::make_pair(entry.frame_index,
                                                  entry.pass_key);

            entry.n_picture_pixels = static_cast<uint64_t>(baseline_image.width) * baseline_image.height;

            /* A pass that makes no difference left out whole makes none draw by draw. */
            if (in_n_draws_of_pass_map.at(pass_pair) > 1)
            {
                const auto& pass_image = image_vec.at(pass_variant_map.at(entry.pass_key) );

                if (Utils::Images::ImageComparer::compare(baseline_image,
                                                          pass_image).n_differing_pixels == 0)
                {
                    add_warnings(&entry);

                    entry.zero_cause = "pass_makes_no_pixels";

                    is_done_vec.at(n_place) = true;

                    continue;
                }
            }

            const auto& probe_image = image_vec.at(entry_variant_vec.at(n_place) );
            const auto  difference  = Utils::Images::ImageComparer::compare(baseline_image,
                                                                            probe_image);

            entry.n_changed_pixels = difference.n_differing_pixels;

            add_warnings(&entry);

            if (difference.n_differing_pixels == 0)
            {
                cause_entry_vec.push_back(n_place);
            }
        }

        /* Why the draws that change no pixel change none: the frame cut after the draw, with it and without it, for all of them in one more run of the frame. */
        if (cause_entry_vec.empty() == false)
        {
            std::vector<Stereo::Replay::EngineDLL::FrameVariant> cut_variant_vec;
            std::vector<Utils::Images::Image>                    cut_image_vec;
            std::string                                          cut_error;

            for (const size_t n_place : cause_entry_vec)
            {
                const auto&                             entry = inout_entry_vec_ptr->at(in_entry_index_vec.at(n_place) );
                Stereo::Replay::EngineDLL::FrameVariant with_variant;

                with_variant.truncate_draw_key    = entry.structure_key;
                with_variant.truncate_pass_key    = entry.pass_key;
                with_variant.truncates_after_draw = true;

                Stereo::Replay::EngineDLL::FrameVariant without_variant = variant_vec.at(entry_variant_vec.at(n_place) );

                without_variant.truncate_draw_key    = entry.structure_key;
                without_variant.truncate_pass_key    = entry.pass_key;
                without_variant.truncates_after_draw = true;

                cut_variant_vec.push_back(with_variant);
                cut_variant_vec.push_back(without_variant);
            }

            const bool is_cut_played = (in_quick_path.render_variants_func(in_options,
                                                                           in_frame_index,
                                                                           cut_variant_vec,
                                                                          &cut_image_vec,
                                                                          &cut_error)      &&
                                        cut_image_vec.size() == cut_variant_vec.size() );

            for (size_t n_cause = 0;
                        n_cause < cause_entry_vec.size();
                      ++n_cause)
            {
                auto& entry = inout_entry_vec_ptr->at(in_entry_index_vec.at(cause_entry_vec.at(n_cause) ) );

                if (is_cut_played)
                {
                    const auto difference = Utils::Images::ImageComparer::compare(cut_image_vec.at(2 * n_cause),
                                                                                  cut_image_vec.at(2 * n_cause + 1) );

                    entry.zero_cause = (difference.n_differing_pixels != 0) ? "overdrawn_later"
                                                                            : "not_rasterized";
                }
                else
                {
                    /* The cut frames could not be played together: the cause is looked for with a replay for each. */
                    Options probe_options = in_options;

                    probe_options.recipe.skipped.pass_draw_vec.push_back(variant_vec.at(entry_variant_vec.at(cause_entry_vec.at(n_cause) ) ).skipped_pass_draw_vec.at(0) );

                    if (find_zero_cause(in_options,
                                        probe_options,
                                        in_render_func,
                                        entry,
                                       &entry.zero_cause,
                                        out_error_ptr) == false)
                    {
                        goto end;
                    }
                }
            }
        }

        /* The images and what is said of each, in the order of the candidates. */
        for (size_t n_place = 0;
                    n_place < in_entry_index_vec.size();
                  ++n_place)
        {
            auto& entry = inout_entry_vec_ptr->at(in_entry_index_vec.at(n_place) );

            if (is_done_vec.at(n_place) )
            {
                fprintf(stdout,
                        "%u of %zu: draws %s of the frame %u: the pass makes no pixels, and so neither do they\n",
                        entry.index + 1,
                        inout_entry_vec_ptr->size(),
                        OverlayReportPage::get_hex_text(entry.structure_key).c_str(),
                        entry.frame_index);

                continue;
            }

            if (write_entry_images(in_options,
                                   &entry,
                                   baseline_image,
                                   image_vec.at(entry_variant_vec.at(n_place) ),
                                   out_error_ptr) == false)
            {
                /* The images are what cannot be written; that is not for another way of playing the frame to mend. */
                result = true;

                goto end;
            }

            print_entry(entry,
                        inout_entry_vec_ptr->size() );
        }
    }

    result = true;
end:
    return result;
}


void Stereo::Replay::ConsistencyChecker::OverlayReport::read_depth_spread(const Utils::Json::JsonValue&               in_depth,
                                                                         Analysis::APIDumpAnalysis::PassDepthSpread* out_spread_ptr)
{
    out_spread_ptr->has_depth = in_depth.at("has_depth").bool_value;

    if (out_spread_ptr->has_depth)
    {
        out_spread_ptr->depth_far         = static_cast<float>(in_depth.at("far").number_value);
        out_spread_ptr->depth_median      = static_cast<float>(in_depth.at("median").number_value);
        out_spread_ptr->depth_near        = static_cast<float>(in_depth.at("near").number_value);
        out_spread_ptr->far_to_near_ratio = static_cast<float>(in_depth.at("far_to_near_ratio").number_value);
        out_spread_ptr->is_flat           = in_depth.at       ("is_flat").bool_value;
    }
}

bool Stereo::Replay::ConsistencyChecker::OverlayReport::run(const Options&       in_options,
                                                           const RenderFunc&    in_render_func,
                                                           const RunShardsFunc& in_run_shards_func,
                                                           std::string*         out_error_ptr,
                                                           const QuickPath*     in_quick_path_ptr)
{
    std::vector<OverlayReportEntry> entry_vec;
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
    if (in_options.overlay_report_shard_count == 0 &&
        n_shards                             >  1 &&
        in_run_shards_func)
    {
        std::vector<std::string> part_filename_vec;

        for (uint32_t n_shard = 0;
                      n_shard < n_shards;
                    ++n_shard)
        {
            part_filename_vec.push_back(in_options.dump_directory +
                                        "/part_"                  +
                                        std::to_string(n_shard)   +
                                        ".json");
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

        const auto is_entry_less_func = [](const OverlayReportEntry& in_entry_a,
                                           const OverlayReportEntry& in_entry_b)
        {
            return (in_entry_a.index < in_entry_b.index);
        };

        std::sort(entry_vec.begin(),
                  entry_vec.end  (),
                  is_entry_less_func);
    }
    else
    if (process_entries(in_options,
                      in_render_func,
                      in_quick_path_ptr,
                     &entry_vec,
                      out_error_ptr) == false)
    {
        goto end;
    }

    /* A part of the work leaves its results to the one that put the parts together. */
    if (in_options.overlay_report_result_filename.empty() == false)
    {
        if (write_list(in_options.overlay_report_result_filename,
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

        if (OverlayReportPage::write_page(in_options.dump_directory,
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

bool Stereo::Replay::ConsistencyChecker::OverlayReport::write_entry_images(const Options&              in_options,
                                                                          OverlayReportEntry*         inout_entry_ptr,
                                                                          const Utils::Images::Image& in_baseline_image,
                                                                          const Utils::Images::Image& in_probe_image,
                                                                          std::string*                out_error_ptr)
{
    bool result = true;

    if (inout_entry_ptr->n_changed_pixels != 0)
    {
        inout_entry_ptr->animation_filename = make_image_filename(inout_entry_ptr->index,
                                                                  inout_entry_ptr->frame_index,
                                                                  "gif");
        inout_entry_ptr->image_filename     = make_image_filename(inout_entry_ptr->index,
                                                                  inout_entry_ptr->frame_index,
                                                                  "png");

        if (write_images(in_options.dump_directory,
                         inout_entry_ptr->image_filename,
                         inout_entry_ptr->animation_filename,
                         in_baseline_image,
                         in_probe_image) == false)
        {
            *out_error_ptr = "The image " + inout_entry_ptr->image_filename + " cannot be written to " + in_options.dump_directory + ".";

            result = false;
        }
    }

    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayReport::write_images(const std::string&          in_directory,
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

    const std::string animation_path = in_directory         +
                                       "/"                  +
                                       in_animation_filename;
    const std::string still_path     = in_directory         +
                                       "/"                  +
                                       in_still_filename;
    bool              result         = false;

    const bool is_still_written = Utils::Images::PngWriter::write_file(still_path,
                                                                       still_image);

    if (is_still_written)
    {
        const std::vector<Utils::Images::Image> frame_vec = {in_baseline_image, marked_image};

        result = Utils::Images::GifWriter::write_file(animation_path,
                                                      frame_vec,
                                                      ANIMATION_FRAME_DELAY_N_MILLISECONDS);
    }

    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayReport::write_list(const std::string&                    in_filename,
                                                                  const std::vector<OverlayReportEntry>& in_entry_vec,
                                                                  std::string*                          out_error_ptr)
{
    bool                    is_written = false;
    bool                    result     = false;
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
        writer.write_key    ("reported_frame_index");
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
        writer.write_key    ("zero_cause");
        writer.write_string (entry.zero_cause);

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

    if (writer.has_failed() == false)
    {
        const std::string& text = writer.get_text();

        is_written = OverlayReportPage::write_text_file(in_filename,
                                     text);
    }

    if (is_written == false)
    {
        *out_error_ptr = "The file " + in_filename + " cannot be written.";

        goto end;
    }

    result = true;
end:
    return result;
}
