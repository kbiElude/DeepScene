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
        /* The keys of the draws that the baker lists in the baked section, by signature and size (the cover key); a draw is in one of the layer lists at the most. */
        struct DrawKeyLists
        {
            std::vector<uint64_t> covering_draw_key_vec;      /* cover the whole of the view                                                                           */
            std::vector<uint64_t> eye_space_draw_key_vec;     /* the object is at the camera and the vertices are in front of it, at a real depth                      */
            std::vector<uint64_t> flat_layer_draw_key_vec;    /* made in the space of the picture, and not always over a 3D scene                                      */
            bool                  has_automatic_layers = false; /* whether the layer lists are written: only for a recipe that asks (Comfort::uses_automatic_layers)  */
            std::vector<uint64_t> overlay_draw_key_vec;       /* made in the space of the picture, always over a 3D scene, and not on a copy of the picture         */
            std::vector<uint64_t> side_reaching_draw_key_vec; /* flat, and reach from the left edge of the picture to the right one                                   */
        };

        /* A kind of draw (a signature and a size) whose draws are not all of one kind by the facts of the analysis: some are made in the space of the picture and others are
         * not, some are over a 3D scene and others are not, or some have their vertices in the space of the eye and others have not. The automatic layers (see
         * RecipeBaker::get_automatic_layers()) leave such a key out, so the recipe has to say what it is, if it has to be one thing.
         */
        struct MixedDraw
        {
            uint64_t                             cover_key = 0;
            Analysis::APIDumpAnalysis::DrawFacts facts;
        };

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

        /* What the analysis says of a kind of frame (a scene), before the comfort of the recipe is held against it: how deep what its affected passes draw is. */
        struct SceneMeasure
        {
            float    distance         = 0.0f; /* the median depth of all the vertices                                              */
            float    far_depth        = 0.0f; /* the depth of the farthest surfaces (the last near_fraction of them)                */
            bool     has_depth        = false;
            float    near_depth       = 0.0f; /* the depth of the nearest surfaces (the first near_fraction of them, see Comfort)   */
            float    projection_scale = 0.0f; /* 1 / tan(half the angle of view of the width of the picture); 0: not known           */
            uint32_t shape_id         = 0;
            uint64_t shape_key        = 0;
        };

        /* What the recipe makes of a SceneMeasure: where the eyes converge, and how far apart they may be (0: no limit), and which of the two limits it is. */
        struct SceneLimits
        {
            float distance           = 0.0f;
            bool  is_far_limit       = false; /* the limit is the one of the farthest surfaces */
            bool  is_overridden      = false; /* the recipe says something of this scene       */
            float max_separation     = 0.0f;
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
            OverlayAgreement                           agreement            = OverlayAgreement::BOTH;
            Analysis::APIDumpAnalysis::PassDepthSpread depth_spread;        /* of its pass: how deep the vertices are */
            Analysis::APIDumpAnalysis::DrawFacts       draw_facts;          /* of its structure, in all the passes: how it blends and where it draws */
            std::map<uint64_t, Analysis::APIDumpAnalysis::DrawFacts> draw_group_map; /* the same for each size of its draws (the cover key), where they are not all alike */
            uint64_t                                   first_frame_index    = 0;
            bool                                       has_draw_facts       = false; /* false for an analysis file that has no draw facts */
            bool                                       has_first_frame      = false;
            bool                                       is_replay_unaffected = false; /* what a replay does: the keys of the draw and of its pass (see RecipeApplier::is_draw_affected) */
            bool                                       is_replay_skipped    = false;
            LiveClass                                  live_class           = LiveClass::AFFECTED;
            uint64_t                                   n_frames             = 0;
            uint32_t                                   pass_id              = 0;
            uint64_t                                   pass_key             = 0;
            std::vector<std::string>                   reason_vec;
            std::vector<uint64_t>                      sample_frame_index_vec; /* the frames to look at it in: spread over the frames that its pass is in */
            uint64_t                                   signature_key        = 0;
            uint64_t                                   structure_key        = 0;
        };

        /* What the checker finds wrong with a pass that the recipe leaves alone. */
        enum class UnaffectedWarningKind
        {
            DEEP_PASS,               /* the vertices of the pass are at many depths: a 3D scene, which would be pushed as if it were flat    */
            REPLAY_AND_LIVE_DISAGREE /* a replay leaves it alone, and the converter that plays the application as it runs does not        */
        };

        /* A pass that the recipe leaves alone and that is not likely an overlay, or that the two ways of playing do not treat alike. */
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
