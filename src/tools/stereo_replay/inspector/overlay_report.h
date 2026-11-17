/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_INSPECTOR_OVERLAY_REPORT_H
#define STEREO_REPLAY_INSPECTOR_OVERLAY_REPORT_H

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
        namespace Inspector
        {
            /* What leaving a candidate out of its frame does to the image of the left eye. */
            struct OverlayReportEntry
            {
                std::string                                agreement;
                std::string                                animation_filename;          /* frame and magenta-highlighted frame, alternating; empty if nothing differs */
                Analysis::APIDumpAnalysis::PassDepthSpread depth_spread;                /* depth of the pass vertices */
                Analysis::APIDumpAnalysis::DrawFacts       draw_facts;                  /* what the structure's draws did */
                uint32_t                                   frame_index      = 0;
                bool                                       has_draw_facts   = false;    /* false if the list has none */
                std::string                                image_filename;              /* relative to the output directory; empty if nothing differs */
                uint32_t                                   index            = 0;         /* position in the list; file names derive from it */
                std::string                                live_class;
                uint64_t                                   n_changed_pixels = 0;
                uint64_t                                   n_frames         = 0;        /* frames the candidate is drawn in */
                uint64_t                                   n_picture_pixels = 0;
                uint64_t                                   pass_id          = 0;
                uint64_t                                   pass_key         = 0;
                std::vector<std::string>                   reason_vec;
                std::string                                replay_class;
                uint64_t                                   signature_key    = 0;
                uint64_t                                   structure_key    = 0;
                std::vector<std::string>                   warning_vec;                 /* why the recipe-ignored draw deserves a look */
                std::string                                zero_cause;                  /* why no pixel changes: overdrawn_later, not_rasterized or pass_makes_no_pixels; empty otherwise */
            };

            /* Finds which overlay candidates (list from RecipeBake --overlays) show in the picture by replaying frames of the left eye with and
             * without each draw. Writes images of the changing candidates, a JSON list and a page.
             */
            class OverlayReport
            {
            public:
                /* Replays up to the frame with the options (recipe included); returns the left eye image. */
                typedef std::function<bool(const Options&        in_options,
                                           const uint32_t&       in_frame_index,
                                           Utils::Images::Image* out_image_ptr,
                                           std::string*          out_error_ptr)> RenderFunc;

                /* Runs the parts in separate processes. Part k of n handles every n-th frame from k, writes result file k and the images. False if a part fails. */
                typedef std::function<bool(const uint32_t&                 in_n_shards,
                                           const std::vector<std::string>& in_result_filename_vec,
                                           std::string*                    out_error_ptr)> RunShardsFunc;

                /* Splits the work into parts and merges their results if n_jobs > 1, there are frames to share and a runner is given; else runs in-process. */
                static bool run(const Options&       in_options,
                                const RenderFunc&    in_render_func,
                                const RunShardsFunc& in_run_shards_func,
                                std::string*         out_error_ptr,
                                const FrameVariantReplayPlan*     in_frame_variant_replay_plan_ptr = nullptr);

            private:
                /* Warns when the vertices span many depths or the draw makes most of the picture. */
                static void add_warnings(OverlayReportEntry* inout_entry_ptr);

                /* Cuts the frame after the draw and compares with and without it: differing images mean overdrawn_later, else not_rasterized. */
                static bool find_zero_cause(const Options&           in_options,
                                            const Options&           in_probe_options,
                                            const RenderFunc&        in_render_func,
                                            const OverlayReportEntry& in_entry,
                                            std::string*             out_cause_ptr,
                                            std::string*             out_error_ptr);

                /* Number of jobs, or more if a part would exceed Options::overlay_report_shard_entries; never more than the number of pass groups. */
                static uint32_t get_n_shards(const Options&                        in_options,
                                             const std::vector<OverlayReportEntry>& in_entry_vec);

                /* List positions of the draws of one pass in one frame, by (frame, pass); a group is examined together. */
                static std::map<std::pair<uint32_t, uint64_t>, std::vector<size_t> > get_pass_group_map(const std::vector<OverlayReportEntry>& in_entry_vec);

                /* Part per list position, balanced by replay cost. */
                static std::vector<uint32_t> get_shard_of_entry_vec(const std::vector<OverlayReportEntry>& in_entry_vec,
                                                                    const uint32_t&                       in_n_shards);

                /* Baseline with pixels differing from the probe image in magenta; the rest is dimmed if requested. */
                static Utils::Images::Image make_highlighted_image(const Utils::Images::Image& in_baseline_image,
                                                                   const Utils::Images::Image& in_probe_image,
                                                                   const bool&                 in_is_rest_dimmed);

                static std::string make_image_filename(const size_t&   in_n_candidate,
                                                       const uint32_t& in_frame_index,
                                                       const char*     in_extension_ptr);

                /* Candidates that have a frame, in list order. */
                static bool parse_candidates(const Options&                  in_options,
                                             std::vector<OverlayReportEntry>* out_entry_vec_ptr,
                                             std::string*                    out_error_ptr);

                static bool parse_hex_key(const std::string& in_text,
                                          uint64_t*          out_key_ptr);

                /* Reads a part's result file as written by write_list(). */
                static bool parse_result_entries(const std::string&              in_filename,
                                                 std::vector<OverlayReportEntry>* inout_entry_vec_ptr,
                                                 std::string*                    out_error_ptr);

                /* Prints one candidate's result. */
                static void print_entry(const OverlayReportEntry& in_entry,
                                        const size_t&             in_n_entries);

                /* Replays the candidates' frames (this part's only, if a part is named) with and without each candidate. Uses the quick path per
                 * frame when it applies and --overlay-report-exact is off; otherwise one candidate at a time.
                 */
                static bool process_entries(const Options&                  in_options,
                                          const RenderFunc&               in_render_func,
                                          const FrameVariantReplayPlan*   in_frame_variant_replay_plan_ptr,
                                          std::vector<OverlayReportEntry>* inout_entry_vec_ptr,
                                          std::string*                    out_error_ptr);

                /* Examines one candidate alone: baseline once per frame, draw left out, whole pass left out (multi-candidate passes), and the
                 * cut frame if no pixel differs. Each is a replay from the start.
                 */
                static bool process_entry(const Options&                                         in_options,
                                          const RenderFunc&                                      in_render_func,
                                          const std::map<std::pair<uint32_t, uint64_t>, size_t>& in_n_draws_of_pass_map,
                                          const size_t&                                          in_n_entries,
                                          OverlayReportEntry*                                    inout_entry_ptr,
                                          std::map<uint32_t, Utils::Images::Image>*              inout_baseline_image_map_ptr,
                                          std::map<std::pair<uint32_t, uint64_t>, bool>*         inout_is_pass_visible_map_ptr,
                                          std::string*                                           out_error_ptr);

                /* Examines a frame's candidates with one play per variant (baseline, passes, candidates, cut frames). Returns false before
                 * reporting anything if the frame cannot be played this way.
                 */
                static bool process_frame_quick(const Options&                                         in_options,
                                                const RenderFunc&                                      in_render_func,
                                                const FrameVariantReplayPlan&                          in_frame_variant_replay_plan,
                                                const uint32_t&                                        in_frame_index,
                                                const std::vector<size_t>&                             in_entry_index_vec,
                                                const std::map<std::pair<uint32_t, uint64_t>, size_t>& in_n_draws_of_pass_map,
                                                std::vector<OverlayReportEntry>*                       inout_entry_vec_ptr,
                                                std::string*                                           out_error_ptr);

                static void read_depth_spread(const Utils::Json::JsonValue&               in_depth,
                                              Analysis::APIDumpAnalysis::PassDepthSpread* out_spread_ptr);
                static void read_draw_facts  (const Utils::Json::JsonValue&               in_facts,
                                              Analysis::APIDumpAnalysis::DrawFacts*       out_facts_ptr);

                /* Writes the still (dimmed, magenta pixels) and the animation (baseline and highlighted, alternating). */
                static bool write_images(const std::string&          in_directory,
                                         const std::string&          in_still_filename,
                                         const std::string&          in_animation_filename,
                                         const Utils::Images::Image& in_baseline_image,
                                         const Utils::Images::Image& in_probe_image);

                /* Writes the images of a changing candidate and stores their file names in the entry. */
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

#endif /* STEREO_REPLAY_INSPECTOR_OVERLAY_REPORT_H */
