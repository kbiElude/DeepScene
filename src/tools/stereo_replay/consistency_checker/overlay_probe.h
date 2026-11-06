/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_PROBE_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_PROBE_H

#include "analysis/api_dump_analysis/json_reader.h"
#include "analysis/api_dump_analysis/types.h"
#include "stereo/replay/frame_images/types.h"
#include "types.h"
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
            struct OverlayProbeEntry
            {
                std::string                                agreement;
                std::string                                animation_filename;          /* the frame and the frame with the draws in magenta, by turns; empty if nothing differs */
                Analysis::APIDumpAnalysis::PassDepthSpread depth_spread;                /* how deep the vertices of its pass are */
                uint32_t                                   frame_index      = 0;
                std::string                                image_filename;              /* relative to the output directory; empty if nothing differs */
                uint32_t                                   index            = 0;         /* of the candidate in the list, which says its files */
                std::string                                live_class;
                uint64_t                                   n_changed_pixels = 0;
                uint64_t                                   n_frames         = 0;        /* the candidate is drawn in */
                uint64_t                                   n_picture_pixels = 0;
                uint64_t                                   pass_id          = 0;
                uint64_t                                   pass_key         = 0;
                std::vector<std::string>                   reason_vec;
                std::string                                replay_class;
                uint64_t                                   signature_key    = 0;
                uint64_t                                   structure_key    = 0;
                std::vector<std::string>                   warning_vec;                 /* what makes a person look twice at a draw that the recipe leaves alone */
            };

            /* Finds out which of the draws that may be 2D overlays (the list of RecipeBake --overlays) are drawn in the picture: the frame in which a candidate is
             * first seen is replayed twice for the left eye, with the draw and with the draw left out (the recipe skips it in its pass only), and the images are compared. The result
             * is a directory with an image for each candidate that changes the picture (the frame as it is, the pixels that the draw makes in magenta), a list
             * of them as JSON and a page that shows them.
             */
            class OverlayProbe
            {
            public:
                /* Replays the workload up to the frame with the options (its recipe), and gives the image of the left eye. */
                typedef std::function<bool(const Options&                          in_options,
                                           const uint32_t&                         in_frame_index,
                                           Stereo::Replay::FrameImages::Image*     out_image_ptr,
                                           std::string*                            out_error_ptr)> RenderFunc;

                /* Runs the parts of the probe, each in a process of its own: the part k of n_shards looks at the candidates of the frames that are the k-th, the (k+n)-th
                 * and so on of the frames of the list, leaves the list of what it found in the k-th file, and writes the images. Gives false if one of them fails. */
                typedef std::function<bool(const uint32_t&                 in_n_shards,
                                           const std::vector<std::string>& in_result_filename_vec,
                                           std::string*                    out_error_ptr)> RunShardsFunc;

                /* With more than one job (Options::n_jobs) and frames to share between them, the work is done by the parts (if there is a function to run them) and
                 * their results are put together; else it is done here. */
                static bool run(const Options&       in_options,
                                const RenderFunc&    in_render_func,
                                const RunShardsFunc& in_run_shards_func,
                                std::string*         out_error_ptr);

            private:
                /* Says what to look at twice in a candidate that the recipe leaves alone: vertices at many depths, and a picture that is mostly its own. */
                static void add_warnings(OverlayProbeEntry* inout_entry_ptr);

                /* How many parts the work has: the jobs, but no more than there are groups of draws (a pass in a frame) to look at. */
                static uint32_t get_n_shards(const Options&                         in_options,
                                             const std::vector<OverlayProbeEntry>& in_entry_vec);

                /* The draws of each pass in each frame, as the places of the candidates in the list: what is looked at together. */
                static std::map<std::pair<uint32_t, uint64_t>, std::vector<size_t> > get_pass_group_map(const std::vector<OverlayProbeEntry>& in_entry_vec);

                /* The part that each candidate is looked at by (by its place in the list): the work is shared out by what the replays cost. */
                static std::vector<uint32_t> get_shard_of_entry_vec(const std::vector<OverlayProbeEntry>& in_entry_vec,
                                                                    const uint32_t&                       in_n_shards);

                /* The frame with the pixels that differ from the other image in magenta; the rest of it dimmed, or as it is. */
                static Stereo::Replay::FrameImages::Image make_highlighted_image(const Stereo::Replay::FrameImages::Image& in_baseline_image,
                                                                                 const Stereo::Replay::FrameImages::Image& in_probe_image,
                                                                                 const bool&                               in_is_rest_dimmed);

                static std::string make_image_filename(const size_t&   in_n_candidate,
                                                       const uint32_t& in_frame_index,
                                                       const char*     in_extension_ptr);

                /* The candidates of the list that has the draws, in its order, as far as they can be seen (they have a frame). */
                static bool parse_candidates(const Options&                 in_options,
                                             std::vector<OverlayProbeEntry>* out_entry_vec_ptr,
                                             std::string*                    out_error_ptr);

                static bool parse_hex_key(const std::string& in_text,
                                          uint64_t*          out_key_ptr);

                /* What a part of the work found, as write_list() writes it. */
                static bool parse_result_entries(const std::string&              in_filename,
                                                 std::vector<OverlayProbeEntry>* inout_entry_vec_ptr,
                                                 std::string*                    out_error_ptr);

                /* Replays the frames of the candidates (of this part, if the options say one) with and without each, and says what they make of the picture. */
                static bool probe_entries(const Options&                  in_options,
                                          const RenderFunc&               in_render_func,
                                          std::vector<OverlayProbeEntry>* inout_entry_vec_ptr,
                                          std::string*                    out_error_ptr);

                static void read_depth_spread(const Analysis::APIDumpAnalysis::JsonValue& in_depth,
                                              Analysis::APIDumpAnalysis::PassDepthSpread*  out_spread_ptr);

                /* The still (the frame dimmed, with the pixels of the draws in magenta) and the animation (the frame as it is, and with them in magenta, by turns). */
                static bool write_images(const std::string&                        in_directory,
                                         const std::string&                        in_still_filename,
                                         const std::string&                        in_animation_filename,
                                         const Stereo::Replay::FrameImages::Image& in_baseline_image,
                                         const Stereo::Replay::FrameImages::Image& in_probe_image);

                static bool write_list(const std::string&                    in_filename,
                                       const std::vector<OverlayProbeEntry>& in_entry_vec,
                                       std::string*                          out_error_ptr);

                static bool write_page(const std::string&                    in_directory,
                                       const std::vector<OverlayProbeEntry>& in_entry_vec,
                                       std::string*                          out_error_ptr);
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_PROBE_H */
