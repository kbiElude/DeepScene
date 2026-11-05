/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "overlay_probe.h"
#include "Analysis/APIDumpAnalysis/json_reader.h"
#include "Analysis/APIDumpAnalysis/json_writer.h"
#include "Stereo/Replay/FrameImages/image_comparer.h"
#include "Stereo/Replay/FrameImages/png_writer.h"
#include <map>
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

std::string Stereo::Replay::ConsistencyChecker::OverlayProbe::make_image_filename(const size_t&   in_n_candidate,
                                                                                  const uint32_t& in_frame_index)
{
    char text[64];

    ::snprintf(text,
               sizeof(text),
               "candidate_%03zu_frame_%u.png",
               in_n_candidate,
               in_frame_index);

    return text;
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

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::run(const Options&    in_options,
                                                           const RenderFunc& in_render_func,
                                                           std::string*      out_error_ptr)
{
    std::map<uint32_t, Stereo::Replay::FrameImages::Image> baseline_image_map;
    Analysis::APIDumpAnalysis::JsonValue                   candidates;
    std::vector<OverlayProbeEntry>                         entry_vec;
    bool                                                   result = false;
    std::string                                            text;

    /* The directory of the output is made if it is not there (one level). */
    ::CreateDirectoryA(in_options.dump_directory.c_str(),
                       nullptr);

    if (Analysis::APIDumpAnalysis::JsonReader::read_file(in_options.overlay_probe_filename,
                                                        &text) == false)
    {
        *out_error_ptr = "The file " + in_options.overlay_probe_filename + " cannot be read.";

        goto end;
    }

    if (Analysis::APIDumpAnalysis::JsonReader::parse(Analysis::APIDumpAnalysis::JsonReader::strip_byte_order_mark(text),
                                                    &candidates,
                                                    out_error_ptr) == false)
    {
        goto end;
    }

    for (const auto& candidate : candidates.at("candidates").item_vec)
    {
        OverlayProbeEntry entry;

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
        entry.live_class   = candidate.at("live_class").string_value;
        entry.n_frames     = static_cast<uint64_t>(candidate.at("n_frames").number_value);
        entry.pass_id      = static_cast<uint64_t>(candidate.at("pass_id").number_value);
        entry.replay_class = candidate.at("replay_class").string_value;

        for (const auto& reason : candidate.at("reasons").item_vec)
        {
            entry.reason_vec.push_back(reason.string_value);
        }

        entry_vec.push_back(entry);
    }

    for (size_t n_entry = 0;
                n_entry < entry_vec.size();
              ++n_entry)
    {
        auto&                              entry = entry_vec.at(n_entry);
        Options                            probe_options = in_options;
        Stereo::Replay::FrameImages::Image probe_image;

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

        probe_options.recipe.skipped.draw_key_vec.push_back(entry.structure_key);

        if (in_render_func(probe_options,
                           entry.frame_index,
                          &probe_image,
                           out_error_ptr) == false)
        {
            goto end;
        }

        {
            const auto& baseline_image = baseline_image_map.at(entry.frame_index);
            const auto  difference     = Stereo::Replay::FrameImages::ImageComparer::compare(baseline_image,
                                                                                              probe_image);

            entry.n_changed_pixels = difference.n_differing_pixels;

            if (difference.n_differing_pixels != 0)
            {
                entry.image_filename = make_image_filename(n_entry,
                                                           entry.frame_index);

                if (write_images(in_options.dump_directory,
                                 entry.image_filename,
                                 baseline_image,
                                 probe_image) == false)
                {
                    *out_error_ptr = "The image " + entry.image_filename + " cannot be written to " + in_options.dump_directory + ".";

                    goto end;
                }
            }
        }

        fprintf(stdout,
                "%zu of %zu: draws %s of the frame %u: %llu pixels change without them\n",
                n_entry + 1,
                entry_vec.size(),
                get_hex_text(entry.structure_key).c_str(),
                entry.frame_index,
                static_cast<unsigned long long>(entry.n_changed_pixels) );
    }

    if (write_list(in_options.dump_directory,
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

    result = true;
end:
    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::write_images(const std::string&                        in_directory,
                                                                    const std::string&                        in_filename,
                                                                    const Stereo::Replay::FrameImages::Image& in_baseline_image,
                                                                    const Stereo::Replay::FrameImages::Image& in_probe_image)
{
    constexpr size_t N_CHANNELS = 4; /* B, G, R, A */

    Stereo::Replay::FrameImages::Image highlighted_image = in_baseline_image;

    /* The pixels which the draws make are magenta; the rest is the frame, dimmed so that they show. */
    for (size_t n_pixel = 0;
                n_pixel < static_cast<size_t>(highlighted_image.width) * highlighted_image.height;
              ++n_pixel)
    {
        uint8_t* const pixel_ptr  = highlighted_image.pixel_u8_vec.data() + n_pixel * N_CHANNELS;
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
        {
            pixel_ptr[0] /= 2;
            pixel_ptr[1] /= 2;
            pixel_ptr[2] /= 2;
        }
    }

    return Stereo::Replay::FrameImages::PngWriter::write_file(in_directory + "/" + in_filename,
                                                              highlighted_image);
}

bool Stereo::Replay::ConsistencyChecker::OverlayProbe::write_list(const std::string&                    in_directory,
                                                                  const std::vector<OverlayProbeEntry>& in_entry_vec,
                                                                  std::string*                          out_error_ptr)
{
    bool                                  result = false;
    Analysis::APIDumpAnalysis::JsonWriter writer(true); /* in_is_pretty */

    writer.begin_object();
    writer.write_key("candidates");
    writer.begin_array();

    for (const auto& entry : in_entry_vec)
    {
        writer.begin_object();

        writer.write_key    ("signature_key");
        writer.write_hex_u64(entry.signature_key);
        writer.write_key    ("structure_key");
        writer.write_hex_u64(entry.structure_key);
        writer.write_key    ("pass_id");
        writer.write_u64    (entry.pass_id);
        writer.write_key    ("n_frames");
        writer.write_u64    (entry.n_frames);
        writer.write_key    ("probed_frame_index");
        writer.write_u64    (entry.frame_index);
        writer.write_key    ("n_changed_pixels");
        writer.write_u64    (entry.n_changed_pixels);
        writer.write_key    ("image");
        writer.write_string (entry.image_filename);
        writer.write_key    ("agreement");
        writer.write_string (entry.agreement);
        writer.write_key    ("replay_class");
        writer.write_string (entry.replay_class);
        writer.write_key    ("live_class");
        writer.write_string (entry.live_class);

        writer.write_key("reasons");
        writer.begin_inline_array();

        for (const auto& reason : entry.reason_vec)
        {
            writer.write_string(reason);
        }

        writer.end_array();
        writer.end_object();
    }

    writer.end_array();
    writer.end_object();

    if (writer.has_failed() ||
        write_text_file(in_directory + "/candidates.json",
                        writer.get_text() ) == false)
    {
        *out_error_ptr = "The file candidates.json cannot be written to " + in_directory + ".";

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
           ".card.none{opacity:.55}code{color:#9cf}button{margin-left:6px}</style></head><body>\n"
           "<h1>Draws that may be 2D overlays</h1>\n"
           "<p>For each candidate the frame it is first drawn in was replayed (left eye) with it and without it. Magenta is what the draws make, the rest of the frame is dimmed. "
           "A candidate that changes no pixel is not drawn in that frame (or draws what is hidden).</p>\n"
           "<p><label>Show: <select id=\"filter\"><option value=\"all\">all</option><option value=\"visible\">changes the picture</option>"
           "<option value=\"disagree\">the opinions disagree</option></select></label></p>\n<div class=\"grid\">\n";

    for (const auto& entry : in_entry_vec)
    {
        const bool is_agreed = (entry.agreement == "both");

        page += "<div class=\"card" + std::string((entry.n_changed_pixels == 0) ? " none" : "") + "\" data-visible=\"" + ((entry.n_changed_pixels != 0) ? "1" : "0") +
                "\" data-disagree=\"" + ((is_agreed) ? "0" : "1") + "\">";

        if (entry.image_filename.empty() == false)
        {
            page += "<a href=\"" + escape_html(entry.image_filename) + "\"><img src=\"" + escape_html(entry.image_filename) + "\" loading=\"lazy\"></a>";
        }

        page += "<div>draw <code>" + get_hex_text(entry.structure_key) + "</code><button onclick=\"navigator.clipboard.writeText('" + get_hex_text(entry.structure_key) +
                "')\">copy key</button></div>"
                "<div>frame " + std::to_string(entry.frame_index) + ", in " + std::to_string(entry.n_frames) + " frames, pass " + std::to_string(entry.pass_id) + "</div>"
                "<div>" + std::to_string(entry.n_changed_pixels) + " pixels change; opinions: " + escape_html(entry.agreement) + " (replay " + escape_html(entry.replay_class) +
                ", live " + escape_html(entry.live_class) + ")</div>";

        for (const auto& reason : entry.reason_vec)
        {
            page += "<div>- " + escape_html(reason) + "</div>";
        }

        page += "</div>\n";
    }

    page += "</div>\n<script>document.getElementById('filter').onchange=function(){var v=this.value;"
            "document.querySelectorAll('.card').forEach(function(c){c.style.display=(v=='all'||(v=='visible'&&c.dataset.visible=='1')||(v=='disagree'&&c.dataset.disagree=='1'))?'':'none';});};"
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
