/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The page of the overlay report: what was found for each draw, over the frames in which it was examined. The draws are listed on the left,
 * each by its key, with a dot for every frame it was examined in (green: it changes the picture; orange: it makes pixels that later draws
 * cover; red: it makes no pixels; grey: its pass makes no pixels). A click on a draw shows its report on the right: a section for the draw,
 * with its frames in time order, so that the behavior of the draw over the demo can be seen.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_REPORT_PAGE_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_REPORT_PAGE_H

#include "overlay_report.h"
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
            class OverlayReportPage
            {
            public:
                static std::string escape_html(const std::string& in_text);

                /* The key as 0x and sixteen digits. */
                static std::string get_hex_text(const uint64_t& in_key);

                /* The sentence that tells what a draw that changes no pixel does instead (see OverlayReportEntry::zero_cause); empty if there is no cause. */
                static std::string get_zero_cause_text(const std::string& in_zero_cause);

                /* Writes index.html in the directory. */
                static bool write_page(const std::string&                    in_directory,
                                       const std::vector<OverlayReportEntry>& in_entry_vec,
                                       std::string*                          out_error_ptr);

                static bool write_text_file(const std::string& in_path,
                                            const std::string& in_text);

            private:
                /* For each draw (by the key of its structure), the list positions of its entries, ordered by frame and then by pass. */
                typedef std::map<uint64_t, std::vector<size_t> > DrawEntryMap;

                static DrawEntryMap get_draw_entry_map(const std::vector<OverlayReportEntry>& in_entry_vec);

                /* The draws in the order in which they first appear: by their first frame, then by their keys. */
                static std::vector<uint64_t> get_draw_order(const std::vector<OverlayReportEntry>& in_entry_vec,
                                                            const DrawEntryMap&                   in_draw_entry_map);

                /* The status of an entry as a word for the page: visible, overdrawn_later, not_rasterized, pass_makes_no_pixels, or none (no cause was found). */
                static std::string get_status(const OverlayReportEntry& in_entry);

                /* One frame of one draw: the animation, the numbers, and why it is a candidate. */
                static std::string make_card(const OverlayReportEntry& in_entry);

                /* A draw: its heading, a line of dots, and its cards. */
                static std::string make_section(const uint64_t&                       in_structure_key,
                                                const std::vector<size_t>&            in_entry_place_vec,
                                                const std::vector<OverlayReportEntry>& in_entry_vec);

                /* The item for a draw in the navigation list on the left. */
                static std::string make_nav_item(const uint64_t&                       in_structure_key,
                                                 const std::vector<size_t>&            in_entry_place_vec,
                                                 const std::vector<OverlayReportEntry>& in_entry_vec);
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_REPORT_PAGE_H */
