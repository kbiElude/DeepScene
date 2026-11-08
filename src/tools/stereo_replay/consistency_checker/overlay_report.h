/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The page of the overlay probe: what was found of each draw, over the frames in which it was looked at. The draws are listed on the left, each by its key
 * with a dot for every frame it was looked at in (green: it changes the picture; orange: it makes pixels that what comes after covers; red: it makes none
 * at all; grey: its pass makes none), and a click on one goes to the right, where the report is: a section for each draw, with its frames one after the other
 * in the order of time, so that how the draw behaves over the demo can be seen.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_REPORT_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_REPORT_H

#include "overlay_probe.h"
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace ConsistencyChecker
        {
            class OverlayReport
            {
            public:
                static std::string escape_html(const std::string& in_text);

                /* The key as 0x and sixteen digits. */
                static std::string get_hex_text(const uint64_t& in_key);

                /* What a draw that changes no pixel does instead, as a sentence (see OverlayProbeEntry::zero_cause); empty for no cause. */
                static std::string get_zero_cause_text(const std::string& in_zero_cause);

                /* Writes index.html in the directory. */
                static bool write_page(const std::string&                    in_directory,
                                       const std::vector<OverlayProbeEntry>& in_entry_vec,
                                       std::string*                          out_error_ptr);

                static bool write_text_file(const std::string& in_path,
                                            const std::string& in_text);

            private:
                /* The entries of each draw (by the key of its structure), as places in the list, in the order of the frames and then of the passes. */
                typedef std::map<uint64_t, std::vector<size_t> > DrawEntryMap;

                static DrawEntryMap get_draw_entry_map(const std::vector<OverlayProbeEntry>& in_entry_vec);

                /* The draws in the order that they are first seen in: by their first frame, and then by their keys. */
                static std::vector<uint64_t> get_draw_order(const std::vector<OverlayProbeEntry>& in_entry_vec,
                                                            const DrawEntryMap&                   in_draw_entry_map);

                /* What an entry came to, as a word for the page: visible, overdrawn_later, not_rasterized, pass_makes_no_pixels, or none (no cause was found). */
                static std::string get_status(const OverlayProbeEntry& in_entry);

                /* One frame of one draw: the animation, the numbers, and why it is a candidate. */
                static std::string make_card(const OverlayProbeEntry& in_entry);

                /* A draw: its heading, a line of dots, and its cards. */
                static std::string make_section(const uint64_t&                       in_structure_key,
                                                const std::vector<size_t>&            in_entry_place_vec,
                                                const std::vector<OverlayProbeEntry>& in_entry_vec);

                /* The entry of a draw in the list on the left. */
                static std::string make_nav_item(const uint64_t&                       in_structure_key,
                                                 const std::vector<size_t>&            in_entry_place_vec,
                                                 const std::vector<OverlayProbeEntry>& in_entry_vec);
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_REPORT_H */
