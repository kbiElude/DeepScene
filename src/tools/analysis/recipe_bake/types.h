/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef RECIPEBAKE_TYPES_H
#define RECIPEBAKE_TYPES_H

#include "analysis/api_dump_analysis/types.h"
#include <map>
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace RecipeBake
    {
        /* The keys (signature and size) that the baker lists in the baked section; a key is in one layer list at most. */
        struct DrawKeyLists
        {
            std::vector<uint64_t> covering_draw_key_vec;      /* cover the whole view */
            std::vector<uint64_t> eye_space_draw_key_vec;     /* at the camera, vertices in front of it */
            std::vector<uint64_t> flat_layer_draw_key_vec;    /* in the space of the picture, not always over a 3D scene */
            bool                  has_automatic_layers = false; /* the layer lists are written (Comfort::uses_automatic_layers) */
            std::vector<uint64_t> overlay_draw_key_vec;       /* in the space of the picture, always over a 3D scene, not on a copy of it */
            std::vector<uint64_t> side_reaching_draw_key_vec; /* flat, from the left edge to the right one */
        };

        /* A key whose draws are of mixed kinds. The automatic layers leave it out, so a recipe has to name it if its draws should be treated alike. */
        struct MixedDraw
        {
            uint64_t                             cover_key = 0;
            Analysis::APIDumpAnalysis::DrawFacts facts;
        };

        /* Statistics of a baking run, for the person who bakes. */
        struct BakeReport
        {
            uint32_t n_ambiguous_signatures     = 0; /* signatures which passes of different classes draw: told apart by the view matrix   */
            uint32_t n_conditional_rules        = 0;
            uint32_t n_convergence_distances    = 0; /* signatures of affected draws which have a distance                                 */
            uint32_t n_convergence_groups       = 0; /* groups of passes that share a signature and therefore share one distance           */
            uint32_t n_per_eye_textures         = 0; /* the textures that each eye has a copy of                                           */
            uint32_t n_rules                    = 0;
            uint32_t n_scene_distances          = 0; /* kinds of frames (scenes) which have a distance                                     */
            uint32_t n_scene_limits             = 0; /* kinds of frames whose nearest surfaces limit how far apart the eyes may be         */
            uint32_t n_scenes_without_depth     = 0; /* scenes in which no affected pass had a draw in front of the camera                 */
            uint32_t n_signatures               = 0;
            uint32_t n_signatures_without_depth = 0; /* affected signatures for which no drawing pass had a draw in front of the camera    */
            bool     uses_dependencies          = false; /* the textures of each eye are told by the dependencies, not by the totals of the analysis */
        };

        /* The measurements of a kind of frame (a scene), before the comfort limits of the recipe are applied: how deep the draws of its
         * affected passes are. */
        struct SceneMeasure
        {
            float    distance         = 0.0f; /* the median depth of all the vertices                                              */
            float    far_depth        = 0.0f; /* the depth of the farthest surfaces (the last near_fraction of them)                */
            bool     has_depth        = false;
            float    near_depth       = 0.0f; /* the depth of the nearest surfaces (the first near_fraction of them, see Comfort)   */
            float    projection_scale = 0.0f; /* 1 / tan(half the angle of view across the width of the picture); 0: not known       */
            uint32_t shape_id         = 0;
            uint64_t shape_key        = 0;
        };

        /* The result of applying the recipe to a SceneMeasure: where the eyes converge, how far apart they may be (0: no limit), and which of the
         * two limits (near or far) applies. */
        struct SceneLimits
        {
            float distance           = 0.0f;
            bool  is_far_limit       = false; /* the limit is the one of the farthest surfaces */
            bool  is_overridden      = false; /* the recipe says something of this scene       */
            float max_separation     = 0.0f;
        };

        /* How the live converter treats the draws of a signature, given the cameras of the pass that draws it (see RecipeApplier::classify_draw). */
        enum class LiveClass
        {
            AFFECTED,
            MIXED,     /* decided by the camera: some cameras of the pass are unaffected and some are not */
            SKIPPED,
            UNAFFECTED
        };

        /* Whether the recipe and the hint (a camera that never changes) agree that a draw is a 2D overlay. */
        enum class OverlayAgreement
        {
            BOTH,
            HEURISTIC_ONLY,
            RECIPE_ONLY
        };

        /* A candidate for a 2D overlay: the recipe leaves the draw alone, or the camera of its pass never changes (see OverlayCandidateFinder). */
        struct OverlayCandidate
        {
            OverlayAgreement                           agreement            = OverlayAgreement::BOTH;
            Analysis::APIDumpAnalysis::PassDepthSpread depth_spread;        /* of its pass: how deep the vertices are */
            Analysis::APIDumpAnalysis::DrawFacts       draw_facts;          /* of its structure, in all the passes: how it blends and where it draws */
            std::map<uint64_t, Analysis::APIDumpAnalysis::DrawFacts> draw_group_map; /* the same per draw size (the cover key), where the draws differ */
            uint64_t                                   first_frame_index    = 0;
            bool                                       has_draw_facts       = false; /* false for an analysis file that has no draw facts */
            bool                                       has_first_frame      = false;
            bool                                       is_replay_unaffected = false; /* whether a replay leaves it alone (see RecipeApplier::is_draw_affected) */
            bool                                       is_replay_skipped    = false;
            LiveClass                                  live_class           = LiveClass::AFFECTED;
            uint64_t                                   n_frames             = 0;
            uint32_t                                   pass_id              = 0;
            uint64_t                                   pass_key             = 0;
            std::vector<std::string>                   reason_vec;
            std::vector<uint64_t>                      sample_frame_index_vec; /* the frames to look at it in, spread over the frames that its pass is in */
            uint64_t                                   signature_key        = 0;
            uint64_t                                   structure_key        = 0;
        };

        /* What the checker finds wrong with a pass that the recipe leaves alone. */
        enum class UnaffectedWarningKind
        {
            DEEP_PASS,               /* the vertices of the pass are at many depths: a 3D scene, which would be pushed as if it were flat    */
            REPLAY_AND_LIVE_DISAGREE /* a replay leaves it alone, and the converter that plays the application as it runs does not        */
        };

        /* A pass that the recipe leaves alone but that is probably not an overlay, or that the replay and the live converter treat differently. */
        struct UnaffectedWarning
        {
            Analysis::APIDumpAnalysis::PassDepthSpread depth_spread;
            uint64_t                                   first_frame_index = 0;
            UnaffectedWarningKind                      kind              = UnaffectedWarningKind::DEEP_PASS;
            uint64_t                                   n_frames          = 0;
            uint32_t                                   pass_id           = 0;
            uint64_t                                   pass_key          = 0;
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
            uint64_t                       n_frames                   = 0;
            uint64_t                       n_frames_of_unknown_shapes = 0;
            uint64_t                       n_shapes                   = 0;
            uint64_t                       n_signatures               = 0;
            std::vector<MixedDraw>         mixed_draw_vec;
            std::vector<UnaffectedWarning> unaffected_warning_vec;
            std::vector<UnknownShape>      unknown_shape_vec;
            std::vector<UnknownSignature>  unknown_signature_vec;
        };

        struct Options
        {
            bool        checks_only     = false; /* the recipe is checked against the workload, and not written */
            std::string deps_filename;   /* the dependencies of the workload (a .deps file); none if empty */
            bool        lists_overlays  = false; /* the draws that may be 2D overlays are listed (JSON), and nothing is written */
            std::string output_filename; /* the recipe itself if empty */
            std::string passes_filename;
            std::string recipe_filename;
            float       screen_degrees  = 90.0f; /* how wide the screen of the headset is, for the angles of --scenes */
            bool        shows_scenes    = false; /* the scenes are listed (JSON) with what the recipe makes of them, and nothing is written */
        };
    }
}

#endif /* RECIPEBAKE_TYPES_H */
