/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "overlay_report.h"
#include <algorithm>
#include <set>
#include <stdio.h>

std::string Stereo::Replay::ConsistencyChecker::OverlayReport::escape_html(const std::string& in_text)
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

Stereo::Replay::ConsistencyChecker::OverlayReport::DrawEntryMap Stereo::Replay::ConsistencyChecker::OverlayReport::get_draw_entry_map(const std::vector<OverlayProbeEntry>& in_entry_vec)
{
    DrawEntryMap result;

    for (size_t n_entry = 0;
                n_entry < in_entry_vec.size();
              ++n_entry)
    {
        result[in_entry_vec.at(n_entry).structure_key].push_back(n_entry);
    }

    /* A draw is read from its first frame to its last. */
    for (auto& draw_pair : result)
    {
        const auto is_earlier_func = [&in_entry_vec](const size_t& in_a,
                                                     const size_t& in_b)
        {
            const auto& a = in_entry_vec.at(in_a);
            const auto& b = in_entry_vec.at(in_b);

            return (a.frame_index != b.frame_index) ? (a.frame_index < b.frame_index)
                                                    : (a.pass_id      < b.pass_id);
        };

        std::stable_sort(draw_pair.second.begin(),
                         draw_pair.second.end  (),
                         is_earlier_func);
    }

    return result;
}

std::vector<uint64_t> Stereo::Replay::ConsistencyChecker::OverlayReport::get_draw_order(const std::vector<OverlayProbeEntry>& in_entry_vec,
                                                                                        const DrawEntryMap&                   in_draw_entry_map)
{
    std::vector<uint64_t> result_vec;

    for (const auto& draw_pair : in_draw_entry_map)
    {
        result_vec.push_back(draw_pair.first);
    }

    const auto is_seen_earlier_func = [&in_entry_vec, &in_draw_entry_map](const uint64_t& in_a,
                                                                          const uint64_t& in_b)
    {
        const uint32_t first_frame_a = in_entry_vec.at(in_draw_entry_map.at(in_a).front() ).frame_index;
        const uint32_t first_frame_b = in_entry_vec.at(in_draw_entry_map.at(in_b).front() ).frame_index;

        return (first_frame_a != first_frame_b) ? (first_frame_a < first_frame_b)
                                                : (in_a          < in_b);
    };

    std::sort(result_vec.begin(),
              result_vec.end  (),
              is_seen_earlier_func);

    return result_vec;
}

std::string Stereo::Replay::ConsistencyChecker::OverlayReport::get_hex_text(const uint64_t& in_key)
{
    char text[19];

    ::snprintf(text,
               sizeof(text),
               "0x%016llx",
               static_cast<unsigned long long>(in_key) );

    return text;
}

std::string Stereo::Replay::ConsistencyChecker::OverlayReport::get_status(const OverlayProbeEntry& in_entry)
{
    std::string result = "none";

    if (in_entry.n_changed_pixels != 0)
    {
        result = "visible";
    }
    else
    if (in_entry.zero_cause.empty() == false)
    {
        result = in_entry.zero_cause;
    }

    return result;
}

std::string Stereo::Replay::ConsistencyChecker::OverlayReport::get_zero_cause_text(const std::string& in_zero_cause)
{
    std::string result;

    if (in_zero_cause == "overdrawn_later")
    {
        result = "It makes pixels when it is drawn, and what is drawn after it covers them.";
    }
    else
    if (in_zero_cause == "not_rasterized")
    {
        result = "It makes no pixels when it is drawn: its colors may be transparent, or it may be outside the picture, clipped, or hidden by the depth test.";
    }
    else
    if (in_zero_cause == "pass_makes_no_pixels")
    {
        result = "Its pass makes no pixels of the picture, so neither does it.";
    }

    return result;
}

std::string Stereo::Replay::ConsistencyChecker::OverlayReport::make_card(const OverlayProbeEntry& in_entry)
{
    const bool        is_agreed  = (in_entry.agreement == "both");
    const bool        is_suspect = (in_entry.warning_vec.empty() == false);
    const std::string status     = get_status(in_entry);
    std::string       result;

    result += "<div class=\"card";
    result += (in_entry.n_changed_pixels == 0) ? " none"
                                               : "";
    result += (is_suspect) ? " suspect"
                           : "";
    result += "\" id=\"c-";
    result += std::to_string(in_entry.index);
    result += "\" data-visible=\"";
    result += (in_entry.n_changed_pixels != 0) ? "1"
                                               : "0";
    result += "\" data-status=\"";
    result += status;
    result += "\" data-disagree=\"";
    result += (is_agreed) ? "0"
                          : "1";
    result += "\" data-suspect=\"";
    result += (is_suspect) ? "1"
                           : "0";
    result += "\">";

    if (in_entry.animation_filename.empty() == false)
    {
        /* The animation shows the frame and the frame with the draws in magenta, by turns; the still dims the rest of the frame. */
        result += "<a href=\"";
        result += escape_html(in_entry.animation_filename);
        result += "\"><img src=\"";
        result += escape_html(in_entry.animation_filename);
        result += "\" loading=\"lazy\"></a><div><a href=\"";
        result += escape_html(in_entry.image_filename);
        result += "\">still</a></div>";
    }

    result += "<div class=\"frame\">frame ";
    result += std::to_string(in_entry.frame_index);
    result += ", pass ";
    result += std::to_string(in_entry.pass_id);
    result += ", in ";
    result += std::to_string(in_entry.n_frames);
    result += " frames</div><div>";
    result += std::to_string(in_entry.n_changed_pixels);
    result += " pixels change; opinions: ";
    result += escape_html(in_entry.agreement);
    result += " (replay ";
    result += escape_html(in_entry.replay_class);
    result += ", live ";
    result += escape_html(in_entry.live_class);
    result += ")</div>";

    if (in_entry.n_changed_pixels == 0)
    {
        const std::string cause_text = get_zero_cause_text(in_entry.zero_cause);

        if (cause_text.empty() == false)
        {
            result += "<div class=\"cause ";
            result += status;
            result += "\">";
            result += escape_html(cause_text);
            result += "</div>";
        }
    }

    if (in_entry.n_picture_pixels != 0)
    {
        char fraction_text[32];

        ::snprintf(fraction_text,
                   sizeof(fraction_text),
                   "%.1f",
                   100.0 * static_cast<double>(in_entry.n_changed_pixels) / static_cast<double>(in_entry.n_picture_pixels) );

        result += "<div>makes ";
        result += fraction_text;
        result += "% of the picture</div>";
    }

    if (in_entry.depth_spread.has_depth)
    {
        char depth_text[96];

        ::snprintf(depth_text,
                   sizeof(depth_text),
                   "vertices of its pass: depth %.1f to %.1f, %.2f times (%s)",
                   in_entry.depth_spread.depth_near,
                   in_entry.depth_spread.depth_far,
                   in_entry.depth_spread.far_to_near_ratio,
                   (in_entry.depth_spread.is_flat) ? "flat"
                                                   : "deep");

        result += "<div>";
        result += depth_text;
        result += "</div>";
    }

    for (const auto& warning : in_entry.warning_vec)
    {
        result += "<span class=\"badge\">";
        result += escape_html(warning);
        result += "</span>";
    }

    for (const auto& reason : in_entry.reason_vec)
    {
        result += "<div>- ";
        result += escape_html(reason);
        result += "</div>";
    }

    result += "</div>\n";

    return result;
}

std::string Stereo::Replay::ConsistencyChecker::OverlayReport::make_nav_item(const uint64_t&                       in_structure_key,
                                                                             const std::vector<size_t>&            in_entry_place_vec,
                                                                             const std::vector<OverlayProbeEntry>& in_entry_vec)
{
    const std::string key_text = get_hex_text(in_structure_key);
    size_t            n_visible = 0;
    std::string       result;

    result += "<a href=\"#d-";
    result += key_text;
    result += "\" data-target=\"d-";
    result += key_text;
    result += "\"><code>";
    result += key_text;
    result += "</code> <span>";

    for (const auto n_entry : in_entry_place_vec)
    {
        const auto& entry = in_entry_vec.at(n_entry);

        n_visible += (entry.n_changed_pixels != 0) ? 1
                                                   : 0;

        result += "<i class=\"dot ";
        result += get_status(entry);
        result += "\"></i>";
    }

    result += "</span><small>";
    result += std::to_string(n_visible);
    result += " of ";
    result += std::to_string(in_entry_place_vec.size() );
    result += " frames change the picture</small></a>\n";

    return result;
}

std::string Stereo::Replay::ConsistencyChecker::OverlayReport::make_section(const uint64_t&                       in_structure_key,
                                                                            const std::vector<size_t>&            in_entry_place_vec,
                                                                            const std::vector<OverlayProbeEntry>& in_entry_vec)
{
    const std::string key_text = get_hex_text(in_structure_key);
    std::set<uint64_t> pass_id_set;
    size_t             n_visible = 0;
    std::string        result;

    for (const auto n_entry : in_entry_place_vec)
    {
        pass_id_set.insert(in_entry_vec.at(n_entry).pass_id);

        n_visible += (in_entry_vec.at(n_entry).n_changed_pixels != 0) ? 1
                                                                      : 0;
    }

    result += "<section class=\"group\" id=\"d-";
    result += key_text;
    result += "\"><h2>draw <code>";
    result += key_text;
    result += "</code><button onclick=\"navigator.clipboard.writeText('";
    result += key_text;
    result += "')\">copy key</button></h2><div class=\"summary\">looked at in ";
    result += std::to_string(in_entry_place_vec.size() );
    result += " frames, ";
    result += std::to_string(n_visible);
    result += " of them change the picture; in the passes";

    for (const auto pass_id : pass_id_set)
    {
        result += " ";
        result += std::to_string(pass_id);
    }

    result += "</div><div class=\"timeline\">";

    /* The frames in the order of time, each a dot that goes to its card. */
    for (const auto n_entry : in_entry_place_vec)
    {
        const auto& entry = in_entry_vec.at(n_entry);

        result += "<a href=\"#c-";
        result += std::to_string(entry.index);
        result += "\"><i class=\"dot ";
        result += get_status(entry);
        result += "\"></i>";
        result += std::to_string(entry.frame_index);
        result += "</a>";
    }

    result += "</div><div class=\"grid\">\n";

    for (const auto n_entry : in_entry_place_vec)
    {
        result += make_card(in_entry_vec.at(n_entry) );
    }

    result += "</div></section>\n";

    return result;
}

bool Stereo::Replay::ConsistencyChecker::OverlayReport::write_page(const std::string&                    in_directory,
                                                                   const std::vector<OverlayProbeEntry>& in_entry_vec,
                                                                   std::string*                          out_error_ptr)
{
    const DrawEntryMap          draw_entry_map = get_draw_entry_map(in_entry_vec);
    const std::vector<uint64_t> draw_order_vec = get_draw_order(in_entry_vec,
                                                                draw_entry_map);
    std::string                 page;
    bool                        result         = false;

    page += R"(<!doctype html>
<html><head><meta charset="utf-8"><title>Overlay candidates</title>
<style>
html,body{height:100%;margin:0}
body{font:14px sans-serif;background:#222;color:#ddd;display:flex}
#nav{width:310px;flex:none;overflow:auto;background:#1a1a1a;padding:8px;box-sizing:border-box;border-right:1px solid #444}
#nav h2{margin:4px 6px 8px;font-size:15px}
#nav a{display:block;padding:4px 6px;color:#ddd;text-decoration:none;border-radius:4px}
#nav a:hover,#nav a.active{background:#383838}
#nav small{display:block;color:#999}
#main{flex:1;overflow:auto;padding:16px;scroll-behavior:smooth}
.group{margin-bottom:30px;border-top:1px solid #444;padding-top:8px}
.group h2{margin:8px 0}
.summary{color:#aaa;margin-bottom:6px}
.timeline{margin:6px 0 10px}
.timeline a{display:inline-block;margin-right:10px;color:#9cf;text-decoration:none}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:2px;background:#666}
.dot.visible{background:#4c4}.dot.overdrawn_later{background:#e93}.dot.not_rasterized{background:#d44}.dot.pass_makes_no_pixels{background:#888}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(340px,1fr));gap:12px}
.card{background:#333;padding:8px;border-radius:6px}.card img{width:100%;image-rendering:pixelated}
.card.none{opacity:.75}.card.suspect{outline:2px solid #e55}
.card .frame{font-weight:bold}
.cause{color:#fc9}
code{color:#9cf}button{margin-left:6px}
.badge{display:inline-block;margin:2px 4px 2px 0;padding:1px 6px;border-radius:4px;background:#a33;color:#fff;font-size:12px}
</style></head><body>
<div id="nav"><h2>Draws</h2>
)";

    for (const auto structure_key : draw_order_vec)
    {
        page += make_nav_item(structure_key,
                              draw_entry_map.at(structure_key),
                              in_entry_vec);
    }

    page += R"(</div>
<div id="main">
<h1>Draws that may be 2D overlays</h1>
<p>Each draw is a draw structure (its state, topology and projection, whatever its vertices) in a pass. It is looked at in some of the frames it is in, one after the other in the order of time:
each frame was replayed (left eye) with the draw and without it. The image alternates between the frame as it is and the same frame with what the draw makes in magenta (click it for its size;
the link below it is a still with the rest dimmed). A draw that changes no pixel says why: <em>overdrawn later</em> (it makes pixels when it is drawn, which what is drawn after it covers),
<em>not rasterized</em> (it makes none when it is drawn: transparent, clipped, outside the picture or hidden by the depth test), or its whole pass makes none. A draw that is visible in some frames and not in others
is one whose behavior changes over the demo. A red outline is a draw that the recipe leaves alone and that is not flat: <em>deep_depth</em> (the vertices of its pass are at many depths: a 3D scene) or
<em>covers_much_of_picture</em> (it makes more than two fifths of the picture).</p>
<p><label>Show: <select id="filter"><option value="all">all</option><option value="visible">changes the picture</option><option value="zero">changes nothing</option>
<option value="overdrawn_later">overdrawn later</option><option value="not_rasterized">not rasterized</option>
<option value="suspect">to look at twice</option><option value="disagree">the opinions disagree</option></select></label></p>
)";

    for (const auto structure_key : draw_order_vec)
    {
        page += make_section(structure_key,
                             draw_entry_map.at(structure_key),
                             in_entry_vec);
    }

    page += R"(</div>
<script>
var main = document.getElementById('main');
var nav = document.getElementById('nav');
var cards = Array.prototype.slice.call(document.querySelectorAll('.card'));
var groups = Array.prototype.slice.call(document.querySelectorAll('.group'));
function matches(card, v) {
  var d = card.dataset;
  return v == 'all' || (v == 'visible' && d.visible == '1') || (v == 'zero' && d.visible == '0') || (v == 'overdrawn_later' && d.status == 'overdrawn_later') ||
         (v == 'not_rasterized' && d.status == 'not_rasterized') || (v == 'suspect' && d.suspect == '1') || (v == 'disagree' && d.disagree == '1');
}
document.getElementById('filter').onchange = function () {
  var v = this.value;
  cards.forEach(function (c) { c.style.display = matches(c, v) ? '' : 'none'; });
  groups.forEach(function (g) {
    var any = Array.prototype.some.call(g.querySelectorAll('.card'), function (c) { return c.style.display != 'none'; });
    g.style.display = any ? '' : 'none';
    var link = nav.querySelector('a[data-target="' + g.id + '"]');
    if (link) { link.style.display = any ? '' : 'none'; }
  });
};
function setActive(id) {
  Array.prototype.forEach.call(nav.querySelectorAll('a'), function (a) { a.classList.toggle('active', a.dataset.target == id); });
}
nav.addEventListener('click', function (e) {
  var a = e.target.closest('a');
  if (!a) { return; }
  e.preventDefault();
  var target = document.getElementById(a.dataset.target);
  if (target) { target.scrollIntoView({behavior: 'smooth', block: 'start'}); setActive(a.dataset.target); }
});
if ('IntersectionObserver' in window) {
  var observer = new IntersectionObserver(function (entries) {
    entries.forEach(function (en) { if (en.isIntersecting) { setActive(en.target.id); } });
  }, {root: main, rootMargin: '0px 0px -75% 0px'});
  groups.forEach(function (g) { observer.observe(g); });
}
</script></body></html>
)";

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

bool Stereo::Replay::ConsistencyChecker::OverlayReport::write_text_file(const std::string& in_path,
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
