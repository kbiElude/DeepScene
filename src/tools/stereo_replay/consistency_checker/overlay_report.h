/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_REPORT_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_REPORT_H

#include "analysis/api_dump_analysis/types.h"
#include "types.h"
#include "utils/images/types.h"
#include "utils/json/json_reader.h"
#include <functional>
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
            /* What leaving a candidate out of its frame does to the image of the left eye. */
            struct OverlayReportEntry
            {
                std::string                                agreement;
                std::string                                animation_filename;          /* the frame and the frame with the draws in magenta, by turns; empty if nothing differs */
                Analysis::APIDumpAnalysis::PassDepthSpread depth_spread;                /* how deep the vertices of its pass are */
                Analysis::APIDumpAnalysis::DrawFacts       draw_facts;                  /* what the draws of its structure did (blending, sampling a copy of the picture, where they draw) */
                uint32_t                                   frame_index      = 0;
                bool                                       has_draw_facts   = false;    /* false if the list does not carry them */
                std::string                                image_filename;              /* relative to the output directory; empty if nothing differs */
                uint32_t                                   index            = 0;         /* position of the candidate in the list; its file names are derived from it */
                std::string                                live_class;
                uint64_t                                   n_changed_pixels = 0;
                uint64_t                                   n_frames         = 0;        /* the number of frames that the candidate is drawn in */
                uint64_t                                   n_picture_pixels = 0;
                uint64_t                                   pass_id          = 0;
                uint64_t                                   pass_key         = 0;
                std::vector<std::string>                   reason_vec;
                std::string                                replay_class;
                uint64_t                                   signature_key    = 0;
                uint64_t                                   structure_key    = 0;
                std::vector<std::string>                   warning_vec;                 /* reasons to take a closer look at a draw that the recipe leaves alone  */
                std::string                                zero_cause;                  /* why a draw changes no pixel: overdrawn_later (it makes pixels when drawn, but what comes after covers them), not_rasterized
                                                                                         * (it makes no pixels when drawn), or pass_makes_no_pixels; empty if it changes pixels or the cause was not looked for */
            };

            /* Finds out which of the draws that may be 2D overlays (the list written by RecipeBake --overlays) show up in the picture. For each
             * candidate it replays some of its frames twice for the left eye, once with the draw and once with the draw left out (the recipe skips it
             * in its own pass only), and compares the images. The frames are those named in the list, or else the first frame of the candidate. The
             * result is a directory with an image for each candidate that changes the picture (the frame as it is, and the pixels that the draw makes
             * in magenta), a list of those candidates as JSON and a page that shows them.
             */
            class OverlayReport
            {
            public:
                /* Replays the workload up to the frame, using the options (including the recipe), and returns the image of the left eye. */
                typedef std::function<bool(const Options&        in_options,
                                           const uint32_t&       in_frame_index,
                                           Utils::Images::Image* out_image_ptr,
                                           std::string*          out_error_ptr)> RenderFunc;

                /* Runs the parts of the report, each in its own process. Part k of n_shards handles the candidates of the k-th, (k+n)-th, ... frames of
                 * the list, writes its findings to the k-th result file and writes the images. Returns false if one of the parts fails. */
                typedef std::function<bool(const uint32_t&                 in_n_shards,
                                           const std::vector<std::string>& in_result_filename_vec,
                                           std::string*                    out_error_ptr)> RunShardsFunc;

                /* If more than one job is configured (Options::n_jobs), there are frames to share and a function that runs the parts is available, the work
                 * is split into parts and their results are merged. Otherwise the work is done in this process. */
                static bool run(const Options&       in_options,
                                const RenderFunc&    in_render_func,
                                const RunShardsFunc& in_run_shards_func,
                                std::string*         out_error_ptr,
                                const QuickPath*     in_quick_path_ptr = nullptr);

            private:
                /* Lists the reasons to take a closer look at a candidate that the recipe leaves alone: its vertices are at many depths, or it makes most
                 * of the picture. */
                static void add_warnings(OverlayReportEntry* inout_entry_ptr);

                /* Finds why a draw that changes no pixel when left out changes none. The frame is cut after the last call of the draw and played with and
                 * without the draw. If the two images differ, the draw makes pixels when it is drawn and what comes after covers them (overdrawn_later).
                 * If they do not differ, the draw makes no pixels (not_rasterized). */
                static bool find_zero_cause(const Options&           in_options,
                                            const Options&           in_probe_options,
                                            const RenderFunc&        in_render_func,
                                            const OverlayReportEntry& in_entry,
                                            std::string*             out_cause_ptr,
                                            std::string*             out_error_ptr);

                /* The number of parts that the work is split into. It is the number of jobs, or more if a part would otherwise get more candidates than
                 * Options::overlay_report_shard_entries (at most as many parts run at a time as there are jobs). It is never more than the number of
                 * groups of draws (a pass in a frame). */
                static uint32_t get_n_shards(const Options&                        in_options,
                                             const std::vector<OverlayReportEntry>& in_entry_vec);

                /* Groups the candidates by frame and pass. Each group holds the list positions of the draws of one pass in one frame; they are examined
                 * together. */
                static std::map<std::pair<uint32_t, uint64_t>, std::vector<size_t> > get_pass_group_map(const std::vector<OverlayReportEntry>& in_entry_vec);

                /* The part that handles each candidate, by its position in the list. The work is shared out by the cost of the replays. */
                static std::vector<uint32_t> get_shard_of_entry_vec(const std::vector<OverlayReportEntry>& in_entry_vec,
                                                                    const uint32_t&                       in_n_shards);

                /* The frame with the pixels that differ from the other image drawn in magenta. The rest of the frame is dimmed or left as it is. */
                static Utils::Images::Image make_highlighted_image(const Utils::Images::Image& in_baseline_image,
                                                                   const Utils::Images::Image& in_probe_image,
                                                                   const bool&                 in_is_rest_dimmed);

                static std::string make_image_filename(const size_t&   in_n_candidate,
                                                       const uint32_t& in_frame_index,
                                                       const char*     in_extension_ptr);

                /* The candidates in the list of draws, in list order, that can be shown (each has a frame). */
                static bool parse_candidates(const Options&                  in_options,
                                             std::vector<OverlayReportEntry>* out_entry_vec_ptr,
                                             std::string*                    out_error_ptr);

                static bool parse_hex_key(const std::string& in_text,
                                          uint64_t*          out_key_ptr);

                /* What a part of the work found, as write_list() writes it. */
                static bool parse_result_entries(const std::string&              in_filename,
                                                 std::vector<OverlayReportEntry>* inout_entry_vec_ptr,
                                                 std::string*                    out_error_ptr);

                /* The result for one candidate. */
                static void print_entry(const OverlayReportEntry& in_entry,
                                        const size_t&             in_n_entries);

                /* Replays the frames of the candidates (only those of this part, if the options name a part) with and without each candidate, and reports
                 * what each does to the picture. The candidates of a frame are played in one run of the frame if the quick path says that the frame can be
                 * played again from the state it leaves, and the options do not ask for one replay per candidate (--overlay-report-exact). A frame that
                 * cannot, or that the quick path fails to play, is replayed one candidate at a time.
                 */
                static bool process_entries(const Options&                  in_options,
                                          const RenderFunc&               in_render_func,
                                          const QuickPath*                in_quick_path_ptr,
                                          std::vector<OverlayReportEntry>* inout_entry_vec_ptr,
                                          std::string*                    out_error_ptr);

                /* Examines one candidate on its own. The frame is replayed as it is (once per frame), then with the draw left out. For a pass with more
                 * than one candidate in the frame, the whole pass is also left out (once per pass and frame) to see whether the pass makes any pixels.
                 * If no pixel differs, the frame is cut after the draw and played with and without it. Each of these is a replay of the workload from
                 * its start. */
                static bool process_entry(const Options&                                         in_options,
                                          const RenderFunc&                                      in_render_func,
                                          const std::map<std::pair<uint32_t, uint64_t>, size_t>& in_n_draws_of_pass_map,
                                          const size_t&                                          in_n_entries,
                                          OverlayReportEntry*                                    inout_entry_ptr,
                                          std::map<uint32_t, Utils::Images::Image>*              inout_baseline_image_map_ptr,
                                          std::map<std::pair<uint32_t, uint64_t>, bool>*         inout_is_pass_visible_map_ptr,
                                          std::string*                                           out_error_ptr);

                /* Examines the candidates of a frame in one run of the frame, with one play per variant: the frame as it is, each pass with more than one
                 * candidate left out, each candidate left out, and, for the candidates that change no pixel, the frame cut after the draw with and
                 * without it. Returns false, before reporting anything about the candidates, if the frame cannot be played this way (the reason is in the
                 * error).
                 */
                static bool process_frame_quick(const Options&                                         in_options,
                                                const RenderFunc&                                      in_render_func,
                                                const QuickPath&                                       in_quick_path,
                                                const uint32_t&                                        in_frame_index,
                                                const std::vector<size_t>&                             in_entry_index_vec,
                                                const std::map<std::pair<uint32_t, uint64_t>, size_t>& in_n_draws_of_pass_map,
                                                std::vector<OverlayReportEntry>*                       inout_entry_vec_ptr,
                                                std::string*                                           out_error_ptr);

                static void read_depth_spread(const Utils::Json::JsonValue&               in_depth,
                                              Analysis::APIDumpAnalysis::PassDepthSpread* out_spread_ptr);
                static void read_draw_facts  (const Utils::Json::JsonValue&               in_facts,
                                              Analysis::APIDumpAnalysis::DrawFacts*       out_facts_ptr);

                /* The still (the dimmed frame with the pixels of the draws in magenta) and the animation (the frame as it is and with those pixels in
                 * magenta, alternating). */
                static bool write_images(const std::string&          in_directory,
                                         const std::string&          in_still_filename,
                                         const std::string&          in_animation_filename,
                                         const Utils::Images::Image& in_baseline_image,
                                         const Utils::Images::Image& in_probe_image);

                /* Writes the still and the animation of a candidate that changes pixels, and stores their file names in the entry. */
                static bool write_entry_images(const Options&              in_options,
                                               OverlayReportEntry*         inout_entry_ptr,
                                               const Utils::Images::Image& in_baseline_image,
                                               const Utils::Images::Image& in_probe_image,
                                               std::string*                out_error_ptr);

                static bool write_list(const std::string&                    in_filename,
                                       const std::vector<OverlayReportEntry>& in_entry_vec,
                                       std::string*                          out_error_ptr);

            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_REPORT_H */
