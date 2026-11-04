/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef RECIPEBAKE_TYPES_H
#define RECIPEBAKE_TYPES_H

#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace RecipeBake
    {
        /* What baking found, for the person who bakes. */
        struct BakeReport
        {
            uint32_t n_ambiguous_signatures     = 0; /* signatures which passes of different classes draw: told apart by the view matrix   */
            uint32_t n_conditional_rules        = 0;
            uint32_t n_convergence_distances    = 0; /* signatures of affected draws which have a distance                                 */
            uint32_t n_convergence_groups       = 0; /* sets of passes that share a signature, which have one distance                     */
            uint32_t n_per_eye_textures         = 0; /* the textures that each eye has a copy of                                           */
            uint32_t n_rules                    = 0;
            uint32_t n_scene_distances          = 0; /* kinds of frames (scenes) which have a distance                                     */
            uint32_t n_scene_limits             = 0; /* kinds of frames whose nearest surfaces limit how far apart the eyes may be         */
            uint32_t n_scenes_without_depth     = 0; /* no affected pass of theirs had a draw in front of the camera                       */
            uint32_t n_signatures               = 0;
            uint32_t n_signatures_without_depth = 0; /* affected, but no pass that draws them had a draw in front of the camera            */
            bool     uses_dependencies          = false; /* the textures of each eye are told by the dependencies, not by the totals of the analysis */
        };

        /* What the live converter does with the draws of a signature, over the cameras that its pass has (see RecipeApplier::classify_draw). */
        enum class LiveClass
        {
            AFFECTED,
            MIXED,     /* by the camera: some of the cameras of the pass are unaffected, and some are not */
            SKIPPED,
            UNAFFECTED
        };

        /* Whether the recipe and the hint (a camera that never changes) agree that a draw is one of a 2D overlay. */
        enum class OverlayAgreement
        {
            BOTH,
            HEURISTIC_ONLY,
            RECIPE_ONLY
        };

        /* A draw that is a candidate for a 2D overlay: the recipe leaves it alone, or the camera of its pass never changes (see OverlayCandidateFinder). */
        struct OverlayCandidate
        {
            OverlayAgreement         agreement             = OverlayAgreement::BOTH;
            uint64_t                 first_frame_index     = 0;
            bool                     has_first_frame       = false;
            bool                     is_replay_unaffected  = false; /* what a replay does: the keys of the draw and of its pass (see RecipeApplier::is_draw_affected) */
            bool                     is_replay_skipped     = false;
            LiveClass                live_class            = LiveClass::AFFECTED;
            uint64_t                 n_frames              = 0;
            uint32_t                 pass_id               = 0;
            uint64_t                 pass_key              = 0;
            std::vector<std::string> reason_vec;
            uint64_t                 signature_key         = 0;
            uint64_t                 structure_key         = 0;
        };

        /* A kind of frame (a shape) which the recipe does not know. */
        struct UnknownShape
        {
            uint64_t first_frame_index = 0;
            uint64_t n_frames          = 0;
            uint64_t shape_key         = 0;
        };

        /* A draw signature which the recipe has no rule for. */
        struct UnknownSignature
        {
            uint64_t n_pass_structures = 0; /* the kinds of passes that draw it */
            uint64_t signature_key     = 0;
        };

        /* What a baked recipe leaves to chance in a workload (see RecipeChecker). */
        struct CoverageReport
        {
            uint64_t                      n_frames                    = 0;
            uint64_t                      n_frames_of_unknown_shapes  = 0;
            uint64_t                      n_shapes                    = 0;
            uint64_t                      n_signatures                = 0;
            std::vector<UnknownShape>     unknown_shape_vec;
            std::vector<UnknownSignature> unknown_signature_vec;
        };

        struct Options
        {
            bool        checks_only     = false; /* the recipe is checked against the workload, and not written */
            std::string deps_filename;   /* the dependencies of the workload (a .deps file); none if empty */
            bool        lists_overlays  = false; /* the draws that may be 2D overlays are listed (JSON), and nothing is written */
            std::string output_filename; /* the recipe itself if empty */
            std::string passes_filename;
            std::string recipe_filename;
        };
    }
}

#endif /* RECIPEBAKE_TYPES_H */
