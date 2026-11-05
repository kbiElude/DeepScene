/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_PROBE_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_OVERLAY_PROBE_H

#include "Analysis/APIDumpAnalysis/types.h"
#include "Stereo/Replay/FrameImages/types.h"
#include "types.h"
#include <functional>
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
                Analysis::APIDumpAnalysis::PassDepthSpread depth_spread;                /* how deep the vertices of its pass are */
                uint32_t                                   frame_index      = 0;
                std::string                                image_filename;              /* relative to the output directory; empty if nothing differs */
                std::string                                live_class;
                uint64_t                                   n_changed_pixels = 0;
                uint64_t                                   n_frames         = 0;        /* the candidate is drawn in */
                uint64_t                                   n_picture_pixels = 0;
                uint64_t                                   pass_id          = 0;
                std::vector<std::string>                   reason_vec;
                std::string                                replay_class;
                uint64_t                                   signature_key    = 0;
                uint64_t                                   structure_key    = 0;
                std::vector<std::string>                   warning_vec;                 /* what makes a person look twice at a draw that the recipe leaves alone */
            };

            /* Finds out which of the draws that may be 2D overlays (the list of RecipeBake --overlays) are drawn in the picture: the frame in which a candidate is
             * first seen is replayed twice for the left eye, with the draw and with the draw left out (the recipe skips it), and the images are compared. The result
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

                static bool run(const Options&    in_options,
                                const RenderFunc& in_render_func,
                                std::string*      out_error_ptr);

            private:
                /* Says what to look at twice in a candidate that the recipe leaves alone: vertices at many depths, and a picture that is mostly its own. */
                static void add_warnings(OverlayProbeEntry* inout_entry_ptr);

                static std::string make_image_filename(const size_t&   in_n_candidate,
                                                       const uint32_t& in_frame_index);

                static bool parse_hex_key(const std::string& in_text,
                                          uint64_t*          out_key_ptr);

                static bool write_images(const std::string&                        in_directory,
                                         const std::string&                        in_filename,
                                         const Stereo::Replay::FrameImages::Image& in_baseline_image,
                                         const Stereo::Replay::FrameImages::Image& in_probe_image);

                static bool write_list(const std::string&                    in_directory,
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
