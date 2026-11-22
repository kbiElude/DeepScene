/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of the replay check tool.
 */
#ifndef STEREO_REPLAY_INSPECTOR_TYPES_H
#define STEREO_REPLAY_INSPECTOR_TYPES_H

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
        namespace Inspector
        {
            /* The exit codes of the tool. */
            enum class ExitCode
            {
                BAD_ARGUMENTS     = 1,
                DIFFERENCES_FOUND = 3, /* the last way of replaying (or the other workload) gave images that differ from the frames as they are */
                REPLAY_FAILED     = 2,
                SUCCESS           = 0
            };

            struct Options
            {
                std::string              breadcrumb_filename;                         /* the call that the replay is making is written here so that a crash leaves it behind (see CrashBreadcrumb) */
                uint32_t                 contact_sheet_n_frames         = 0;          /* the frames to show in the contact sheet; 0: no sheet */
                std::string              dump_directory;
                uint32_t                 dump_frame                     = UINT32_MAX; /* the images of this frame are written whether or not it differs */
                float                    eye_separation                 = 0.0f;
                Utils::Images::EyeView   eye_view                       = Utils::Images::EyeView::BOTH;
                uint32_t                 first_frame                    = 0;
                std::vector<Stereo::Replay::EngineDLL::FrameVariant> frame_variant_vec;  /* the last frame is played once for each of these, in the mode VARIANTS (see replay_variants_to_frame) */
                bool                     has_eye_view                   = false;     /* a screenshot or a sheet uses the eye view that was asked for                  */
                bool                     has_truncation                 = false;      /* the last frame is cut after the draws of truncate_draw_key in the pass truncate_pass_key (see FrameExecutionOptions) */
                bool                     has_history                    = false;      /* a screenshot also replays the frames before it with the recipe, as a player does, so that what carries over from frame to frame is right                    */
                std::string              deps_filename;                               /* the dependencies of the workload; a replay uses them to skip the frames that nothing after them reads                 */
                std::string              image_filename;                              /* the PNG file of a screenshot or a contact sheet */
                std::string              overlay_report_filename;                     /* the list of draws that may be overlays; their frames are replayed with each draw left out */
                bool                     overlay_report_is_exact         = false;      /* each candidate is replayed on its own, from the start, and not with the others of its frame in one run */
                uint32_t                 overlay_report_last_frame       = UINT32_MAX; /* the report looks at the frames of the candidates up to this one */
                uint32_t                 overlay_report_shard_entries    = 40;         /* an overlay report process handles at most this many candidates, because the replays use up the memory of a 32-bit process; 0: no limit                */
                std::string              overlay_report_result_filename;               /* a part of an overlay report writes its findings here for the process that merges the parts     */
                uint32_t                 overlay_report_shard_count      = 0;          /* the number of parts of the report; 0: this process is not one of them */
                uint32_t                 overlay_report_shard_index      = 0;          /* the index of this part */
                bool                     is_making_index                = false;      /* the index of the workload is made and said, nothing is replayed */
                bool                     is_parallel                    = false;      /* the ways are replayed at the same time */
                bool                     is_profiling                   = false;      /* the frames are replayed once, and where the time goes is reported */
                bool                     is_redundant_state_count                = false;      /* the frames are replayed once, and what the dead frames of the seek do to the state is reported */
                uint32_t                 last_frame                     = 999;
                uint32_t                 n_jobs                         = 1;          /* the number of processes that share an overlay report   */
                Analysis::Recipe::Recipe recipe;
                std::string              recipe_filename;
                std::string              same_frames_workload_filename;               /* the frames are also replayed from this workload and compared (a slice of the workload, for example) */
                std::vector<uint32_t>    screenshot_frame_vec;                        /* the frames to write the image of, in order and without repeats; the replay of the workload stops at the last */
                uint64_t                 truncate_draw_key              = 0;
                uint64_t                 truncate_pass_key              = 0;
                std::vector<std::string> way_name_vec;                                /* the ways to replay besides the frames as they are; none: the default ways */
                std::string              workload_filename;
            };

            /* A file of a screenshot, and the frame that it shows. */
            struct ScreenshotFile
            {
                std::string filename;
                uint32_t    frame = 0;
            };

            /* What the screenshots made: the files, and the frames that were asked for and that the replay did not reach (no file was written for them). */
            struct ScreenshotReport
            {
                std::vector<uint32_t>       skipped_frame_vec;
                std::vector<ScreenshotFile> written_vec;
            };

            /* What the overlay report needs to play the candidates of a frame in one run (see OverlayReport): a replay of the frame once per variant,
             * and whether the frame can be played again from the state it leaves. If the frame cannot, or a replay fails, the candidates are replayed
             * one at a time.
             */
            struct FrameVariantReplayPlan
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

            /* How an OpenGL call affects the state of the context; used by the census of what the dead frames of a seek make (see RedundantStateCounter). */
            enum class StateCallClass
            {
                BARRIER,  /* something looks at the state or may change it in a way that is not followed: a call of a list, a push or pop of attributes, a query, a context */
                KEYED,    /* gives one piece of state a value, whatever it was before, and nothing else (a capability, a material, a texture parameter)               */
                OTHER,    /* is not told apart                                                                                                                         */
                SEQUENCE  /* depends on the calls before it (the matrices and their stack, a binding of a texture, the making of an object)                           */
            };

            /* One way of replaying the frames of the check. */
            struct ReplayWay
            {
                Stereo::Replay::EngineDLL::FrameExecution mode;
                const char*                               name_ptr;
                bool                                      restores_state;
                bool                                      uses_recipe;
            };
        }
    }
}

#endif /* STEREO_REPLAY_INSPECTOR_TYPES_H */
