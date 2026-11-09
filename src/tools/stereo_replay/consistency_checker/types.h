/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the replay check tool.
 */
#ifndef STEREO_REPLAY_CONSISTENCYCHECKER_TYPES_H
#define STEREO_REPLAY_CONSISTENCYCHECKER_TYPES_H

#include "analysis/recipe/types.h"
#include "tools/stereo_replay/engine_dll/types.h"
#include "utils/images/types.h"
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
            /* What the tool exits with. */
            enum class ExitCode
            {
                BAD_ARGUMENTS     = 1,
                DIFFERENCES_FOUND = 3, /* the last way of replaying gave other images than the frames as they are */
                REPLAY_FAILED     = 2,
                SUCCESS           = 0
            };

            struct Options
            {
                std::string              breadcrumb_filename;                         /* the call that the replay is making is written here, for a crash to leave it (see CrashBreadcrumb) */
                uint32_t                 contact_sheet_n_frames         = 0;          /* the frames to show in the contact sheet; 0: no sheet */
                std::string              dump_directory;
                uint32_t                 dump_frame                     = UINT32_MAX; /* the images of this frame are written whether or not it differs */
                float                    eye_separation                 = 0.0f;
                Utils::Images::EyeView   eye_view                       = Utils::Images::EyeView::BOTH;
                uint32_t                 first_frame                    = 0;
                std::vector<Stereo::Replay::EngineDLL::FrameVariant> frame_variant_vec;  /* the last frame is played once for each of these, in the mode VARIANTS (see replay_variants_to_frame) */
                bool                     has_eye_view                   = false;     /* the view of the eyes of a screenshot or a sheet is the one that was asked for */
                bool                     has_truncation                 = false;      /* the last frame is cut after the draws of truncate_draw_key in the pass truncate_pass_key (see FrameExecutionOptions) */
                bool                     has_history                    = false;      /* a screenshot replays the frames before it with the recipe too, as a player does, so that what carries over from frame to frame is right (slow without deps) */
                std::string              deps_filename;                               /* the dependencies of the workload: a replay goes to the last frame leaving out the frames which nothing after it reads */
                std::string              image_filename;                              /* the PNG file of a screenshot or a contact sheet */
                std::string              overlay_report_filename;
                bool                     overlay_report_is_exact         = false;      /* each candidate is replayed on its own, from the start, and not with the others of its frame in one run */
                uint32_t                 overlay_report_last_frame       = UINT32_MAX; /* the report looks at the frames of the candidates up to this one */                      /* the list of draws that may be overlays, whose frames are replayed with each draw left out */
                std::string              overlay_report_result_filename;               /* a part of an overlay report writes what it found here, for the one that put the parts together */
                uint32_t                 overlay_report_shard_count      = 0;          /* the parts of the report; 0: this is not one of them */
                uint32_t                 overlay_report_shard_index      = 0;          /* the one that this is */
                bool                     is_parallel                    = false;      /* the ways are replayed at the same time */
                bool                     is_profiling                   = false;      /* the frames are replayed once, and where the time goes is said */
                bool                     is_state_census                = false;      /* the frames are replayed once, and what the dead frames of the seek do to the state is said */
                uint32_t                 last_frame                     = 999;
                uint32_t                 n_jobs                         = 1;          /* the processes that an overlay report is shared between */
                Analysis::Recipe::Recipe recipe;
                std::string              recipe_filename;
                std::string              same_frames_workload_filename;               /* the frames are replayed from this workload too, and compared (a slice of the workload, for one) */
                uint32_t                 screenshot_frame               = UINT32_MAX; /* the frame to write the image of: the replay of the workload stops at it */
                uint64_t                 truncate_draw_key              = 0;
                uint64_t                 truncate_pass_key              = 0;
                std::vector<std::string> way_name_vec;                                /* the ways to replay besides the frames as they are; none: the usual ones */
                std::string              workload_filename;
            };

            /* What the report of overlays needs to play the candidates of a frame in one run (see OverlayReport): a replay of the frame once for each variant, and whether
             * the frame can be played again from the state it leaves. A frame that cannot, or a replay that fails, is replayed one candidate at a time.
             */
            struct QuickPath
            {
                /* Whether the frame can be played again and again from one state; the reason if it cannot. */
                std::function<bool(const uint32_t& in_frame_index,
                                   std::string*    out_reason_ptr)> is_repeatable_func;

                /* The image of the left eye of the frame for each variant, in their order (see replay_variants_to_frame()). */
                std::function<bool(const Options&                                              in_options,
                                   const uint32_t&                                             in_frame_index,
                                   const std::vector<Stereo::Replay::EngineDLL::FrameVariant>& in_variant_vec,
                                   std::vector<Utils::Images::Image>*                          out_image_vec_ptr,
                                   std::string*                                                out_error_ptr)> render_variants_func;
            };

            /* What a call of OpenGL does to the state of the context, for the census of what the dead frames of a seek make (see StateCensus). */
            enum class StateCallClass
            {
                BARRIER,  /* something looks at the state or may change it in a way that is not followed: a call of a list, a push or pop of attributes, a query, a context */
                KEYED,    /* gives one piece of state a value, whatever it was before, and nothing else (a capability, a material, a texture parameter)               */
                OTHER,    /* is not told apart                                                                                                                         */
                SEQUENCE  /* depends on the calls before it (the matrices and their stack, a binding of a texture, the making of an object)                           */
            };

            /* One way of replaying the frames of the check. */
            struct Variant
            {
                Stereo::Replay::EngineDLL::FrameExecution mode;
                const char*                               name_ptr;
                bool                                      restores_state;
                bool                                      uses_recipe;
            };
        }
    }
}

#endif /* STEREO_REPLAY_CONSISTENCYCHECKER_TYPES_H */
