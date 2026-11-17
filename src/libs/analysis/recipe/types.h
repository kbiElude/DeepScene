/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Types of a recipe (JSON, see recipe_loader.h): which calls to change, and how, to render each eye.
 * Frames and passes are named by match keys (see MatchKeyBuilder), which are stable across runs, unlike indices and analysis IDs.
 */
#ifndef RECIPE_TYPES_H
#define RECIPE_TYPES_H

#include "common/types.h"
#include <array>
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace Recipe
    {
        /* The axes of a translation: the first three components of the last row of a matrix move along them. */
        enum class Axis
        {
            X,
            Y,
            Z
        };

        /* What a rule does to the matrix of a call. */
        enum class ActionKind
        {
            EYE_CONVERGENCE, /* shears the projection to converge at the baked or replay-set distance; none: no change              */
            EYE_OFFSET,      /* translates along an axis by +/- half the eye separation (a replay setting)                          */
            MULTIPLY,        /* the matrix of the recipe is multiplied with the matrix of the call                                  */
            REPLACE          /* the matrix of the recipe replaces the matrix of the call                                            */
        };

        /* What the recipe does to a draw. */
        enum class DrawClass
        {
            AFFECTED,  /* the rules apply to it                                                            */
            SKIPPED,   /* it is not played at all, in either eye (the calls that set its state are played) */
            UNAFFECTED /* the rules leave it alone                                                         */
        };

        /* The eye that the application is rendered for. */
        enum class Eye
        {
            LEFT,
            RIGHT
        };

        /* Restricts a rule to a kind of matrix. Direct3D matrices are row-major and multiply row vectors. A perspective projection has the
         * last column (0, 0, 1, 0). An orthographic projection has the last column (0, 0, 0, 1).
         */
        /* How a converter keeps time for an application when no program that shows the images does. */
        enum class FramePacingMode
        {
            HOLD_TO_RATE, /* the application is held to a number of frames a second                     */
            SCREEN_ONLY   /* only a program that shows the images keeps time; the application runs free */
        };

        enum class MatrixKind
        {
            ANY,
            ORTHOGRAPHIC,
            PERSPECTIVE
        };

        /* The order of a product, where M is the matrix of the call and X is the matrix of the recipe. */
        enum class MultiplyOrder
        {
            POST, /* M * X */
            PRE   /* X * M */
        };

        /* The API of a function that a rule can be about. Each API has its own names for its transform states. */
        enum class RuleApi
        {
            D3D7,
            D3D8
        };

        /* What to do with a frame whose shape the recipe does not know. */
        enum class UnknownFramePolicy
        {
            FAIL, /* stop with an error                              */
            MONO, /* play it as it is, for both eyes                 */
            SKIP  /* do not play it                                  */
        };

        /* A rule applies only to calls in which an argument has this value. */
        struct ArgumentCondition
        {
            uint32_t argument_index = 0; /* position in the call's arguments; 0 is the object it is called on       */
            uint32_t value          = 0;
        };

        /* Author overrides for one scene (identified by its shape key). 0 means unset. A set value wins over measured and recipe-wide values. */
        struct SceneComfort
        {
            float    convergence_distance = 0.0f; /* in the units of the camera of the application                                                          */
            float    max_far_parallax     = 0.0f; /* as Comfort::max_far_parallax                                                                           */
            float    max_near_parallax    = 0.0f; /* as Comfort::max_near_parallax                                                                          */
            float    max_separation       = 0.0f; /* the limit on how far apart the eyes may be, in the units of the camera; wins over the two limits above */
            uint64_t shape_key            = 0;
        };

        /* Where the eyes converge for a scene and how far apart they may be (0 means no limit). */
        struct ComfortLimits
        {
            float distance       = 0.0f;
            bool  is_far_limit   = false; /* the limit comes from the farthest surfaces */
            float max_separation = 0.0f;
        };

        /* Author limits on how much the eyes are asked to do; they do not depend on the scene. */
        struct Comfort
        {
            /* Also use the baked layers (flat_layer_, overlay_ and eye_space_draw_key_vec). Off by default. */
            bool uses_automatic_layers = false;

            /* Whether the convergence distance fits the scene depth into both parallax limits (needs max_near_parallax and max_far_parallax),
             * instead of being the median vertex depth. A distance set by the scene (see SceneComfort) is kept.
             */
            bool balances_convergence = false;

            /* A draw whose origin is nearer than this fraction of the convergence distance counts as attached to the camera and is drawn the
             * same for both eyes. 0 means none.
             */
            float camera_attached_depth = 0.0f;

            /* Whether a converter eases the eyes by elapsed time (frame-rate independent) instead of by a fixed fraction per frame. Off by default. */
            bool eases_by_time = false;

            /* Keys (signature and size) of draws whose vertices are already in eye space. Not treated as attached to the camera. */
            std::vector<uint64_t> eye_space_cover_key_vec;

            /* 0 to 1: how much a depth strength above 1 also raises the scene limits (see SceneSeparation). 0 (default): the limits are a
             * ceiling; 1: they grow in proportion. A strength below 1 lowers the limits regardless.
             */
            float limits_follow_strength = 0.0f;

            /* Largest distance between the eye pictures at the farthest surface of a scene, as a fraction of the picture width. 0 means no limit. */
            float max_far_parallax = 0.0f;

            /* Largest distance between the eye pictures at the nearest surface, as a fraction of the picture width; the separation is reduced
             * to fit. 0 means no limit.
             */
            float max_near_parallax = 0.0f;

            /* Whether a converter that can (see RecipeStereoPlanner::on_frame_depth) measures the surface depth of each frame live and checks it
             * against the limits, instead of using the baked values (see SceneSeparation). Author values (see SceneComfort) are kept.
             */
            bool measures_depth = false;

            /* Fraction of the surfaces of a scene (counted by vertices) that counts as its nearest. */
            float near_fraction = 0.001f;

            /* Push toward the viewer of the draws the rules leave alone, as a fraction of the picture width. Only draws to the presented
             * image are pushed, not draws to a texture. 0 means no push.
             */
            float overlay_parallax = 0.0f;

            /* Push toward the viewer of protruding object parts named by DrawSelector::part_cover_key_vec, as a fraction of the picture
             * width. 0 means the same as overlay_parallax.
             */
            float part_parallax = 0.0f;

            /* Whether the recipe's pushed draws (see pushes_unaffected and DrawSelector) are pushed while no switch has been set (a run
             * without the VR player). Once the switch is set, it decides.
             */
            bool pushes_by_default = false;

            /* Whether all draws the rules leave alone are pushed by overlay_parallax, not only those listed in DrawSelector. For
             * applications whose left-alone draws are all overlay.
             */
            bool pushes_unaffected = false;

            /* What the recipe says about single scenes, identified by their shape keys. */
            std::vector<SceneComfort> scene_override_vec;

            /* Whether a converter jumps to the new scene distance and limit at a cut (target over 2x or under 0.5x the previous) instead
             * of easing. Off by default.
             */
            bool snaps_at_cuts = false;
        };

        /* Parallax for pre-transformed vertices (positions in pixels plus 1 / depth), which no matrix moves. The recipe supplies the angle
         * of view and the convergence distance. A vertex moves sideways, right for the left eye and left for the right eye, by
         *   (width / 2) / tan(field_of_view / 2) * (eye separation / 2) * (1 / depth - 1 / convergence_distance) pixels.
         * A vertex at the convergence distance is not moved.
         */
        struct ScreenParallaxSettings
        {
            float convergence_distance  = 0.0f;  /* in the units of the camera of the application; content at this distance has no parallax */
            float field_of_view_degrees = 90.0f; /* the angle of view across the width of the picture                                       */
            bool  is_enabled            = false; /* whether the recipe has the section                                                      */
        };

        /* Convergence distance for a draw (by signature), in camera units. RecipeBake measures it (see DepthHistogram). */
        struct DrawConvergence
        {
            float    distance      = 0.0f;
            uint64_t signature_key = 0;
        };

        /* What the recipe decides about one draw (see RecipeApplier::decide_draw). */
        struct DrawDecision
        {
            DrawClass draw_class = DrawClass::AFFECTED;
            bool      is_pushed  = false; /* the draw is moved toward the viewer */
        };

        /* What a caller knows about a draw. A replay knows the pass (has_pass); a live converter does not, so it gives the signature and
         * the view matrix instead. The cover key is known in both cases.
         */
        struct DrawQuery
        {
            uint64_t              cover_key            = 0;
            uint64_t              draw_key             = 0;     /* of the draw structure; used if has_pass                                              */
            bool                  has_pass             = false;
            bool                  is_cover_hold_waived = false; /* the caller cannot leave a draw alone because of its cover key (see decide_draw) */
            uint64_t              pass_key             = 0;     /* of the pass structure; used if has_pass                                              */
            uint64_t              signature_key        = 0;     /* used if has_pass is false                                                            */
            std::array<float, 16> view_matrix          = {};    /* used if has_pass is false                                                            */
        };

        /* Classifies draws by their signature at issue time (see MatchKeyBuilder::compute_draw_signature_key), which is all a live
         * converter knows. RecipeBake builds the rules. Tried in order; a draw no rule matches is affected.
         */
        struct DrawRule
        {
            DrawClass                          draw_class    = DrawClass::AFFECTED;
            bool                               is_pushed     = false;       /* the draw is moved toward the viewer (Recipe::pushed) */
            uint64_t                           signature_key = 0;
            std::vector<std::array<float, 16>> view_matrix_vec; /* if not empty, the rule applies only while the view matrix is one of these */
        };

        /* A draw in one pass only. A live converter does not know the passes, so it cannot tell this pair from the plain draw. */
        struct PassDraw
        {
            uint64_t draw_key = 0; /* of a draw structure */
            uint64_t pass_key = 0; /* of a pass structure */
        };

        /* Names passes and draws by their match keys (see MatchKeyBuilder), which are the same in every run. */
        struct DrawSelector
        {
            std::vector<uint64_t> cover_key_vec; /* keys by signature and size (MatchKeyBuilder::compute_draw_cover_key); only "pushed" has them;
                                                  * tell apart draws of one structure; drawn flat, once for both eyes                                           */
            std::vector<uint64_t> draw_key_vec;  /* keys of draw structures                                                                                     */
            std::vector<uint64_t> part_cover_key_vec; /* like cover_key_vec, for the parts of an object that protrude from it; pushed by Comfort::part_parallax */
            std::vector<PassDraw> pass_draw_vec; /* a draw in one pass only: the most specific name there is                                                    */
            std::vector<uint64_t> pass_key_vec;  /* keys of pass structures                                                                                     */
        };

        /* One change to a matrix. Applies when every condition holds and the pass and frame match (no keys means any). Rules apply in
         * recipe order, each on the previous rule's result.
         */
        struct MatrixRule
        {
            ActionKind                     action_kind            = ActionKind::MULTIPLY;
            std::vector<ArgumentCondition> argument_condition_vec;
            APIInterceptor::APIFunction    function               = APIInterceptor::APIFunction::APIFUNCTION_UNKNOWN;
            std::array<float, 16>          left_matrix_fp32vec16  = {};
            uint32_t                       matrix_argument_index  = 0;                                                    /* argument holding the matrix */
            MatrixKind                     matrix_kind            = MatrixKind::ANY;
            MultiplyOrder                  multiply_order         = MultiplyOrder::POST;
            std::string                    name;                                                                          /* optional          */
            Axis                           offset_axis            = Axis::X;                                              /* EYE_OFFSET only   */
            std::vector<uint64_t>          pass_key_vec;
            std::array<float, 16>          right_matrix_fp32vec16 = {};
            std::vector<uint64_t>          shape_key_vec;
        };

        /* A texture of which each eye has its own copy. Identified by how it was made and the count of earlier textures made the same
         * way (see ResourceIdentity). RecipeBake measures it from the textures that frames carry over.
         */
        struct PerEyeTexture
        {
            uint64_t description_key = 0;
            uint32_t ordinal         = 0;
        };

        /* Convergence distance for a scene (by shape key), measured by RecipeBake. Used when the previous frame's shape is known;
         * DrawConvergence is the fallback.
         */
        struct SceneConvergence
        {
            float    distance  = 0.0f;
            uint64_t shape_key = 0;
        };

        /* Largest eye separation for a scene (by shape key, as in SceneConvergence), in camera units; measured by RecipeBake from
         * surface depth and Comfort. An unlisted scene has no limit.
         */
        struct SceneSeparation
        {
            float    max_separation = 0.0f;
            uint64_t shape_key      = 0;
        };

        /* Parallax when the camera is built by many calls on one matrix (OpenGL model-view stack). The call that begins the camera
         * (DrawInstance::view_anchor_call_index: the last glLoadIdentity or glLoadMatrix) loads the eye shift on top of what it loads.
         * In camera space (looking down -z) the shift is x + h + (h / d) * z for the left eye, with h half the eye separation and d the
         * convergence distance; the right eye reverses the sign of h. Distance 0 applies only the translation.
         */
        struct ViewAnchorShift
        {
            float convergence_distance        = 0.0f;  /* where the eyes converge when the scene gives no distance; in the units of the camera of the application; 0 means none */
            bool  is_enabled                  = false; /* whether the recipe has the section                                                                                    */
            /* Whether an affected draw whose camera no call begins is played as it is. Otherwise it is refused. */
            bool  leaves_draws_without_anchor = false;
        };

        /* A draw is affected unless `unaffected` names it. A draw key beats a pass key. The same kind of key in both `affected` and
         * `unaffected` is refused. A draw named by `skipped`, or in a skipped pass, is not played in either eye; its state calls are played.
         */
        /* What a converter does when no screen keeps time, for applications that tie things to time. The same for every API. */
        struct FramePacing
        {
            float           frames_per_second = 60.0f; /* for HOLD_TO_RATE */
            FramePacingMode mode              = FramePacingMode::HOLD_TO_RATE;
        };

        struct Recipe
        {
            DrawSelector                  affected;
            /* Serves single-pixel reads from one picture read once (see PictureReadbackCache). Off unless the recipe asks. */
            bool                          caches_pixel_reads      = false;
            Comfort                       comfort;                         /* set by the recipe author; see Comfort      */
            std::vector<DrawConvergence>  convergence_vec;                 /* from "baked"; see DrawConvergence          */
            std::vector<DrawRule>         draw_rule_vec;                   /* from "baked"; see DrawRule                 */
            std::vector<uint64_t>         eye_space_draw_key_vec;          /* from "baked": vertices in eye space (see Comfort::uses_automatic_layers) */
            std::vector<uint64_t>         flat_layer_draw_key_vec;         /* from "baked": draws in the space of the picture, never over a 3D scene: left as made */
            FramePacing                   frame_pacing;                    /* what to do when no screen keeps time; see FramePacing */
            bool                          is_baked                = false; /* whether the file has a list of draw rules, even an empty one */
            std::vector<uint64_t>         known_shape_key_vec;             /* if empty, no frame is unknown              */
            std::vector<uint64_t>         covering_draw_key_vec;      /* from "baked"; keys (signature and size) of draws that cover the whole view: not moved sideways */
            std::vector<uint64_t>         known_signature_key_vec;         /* from "baked"; if empty, no draw is unknown */
            uint32_t                      key_scheme_version      = 0;
            std::vector<uint64_t>         overlay_draw_key_vec;            /* from "baked": draws in the space of the picture over a 3D scene: left as made and pushed */
            std::vector<PerEyeTexture>    per_eye_texture_vec;             /* from "baked"; see PerEyeTexture             */
            DrawSelector                  pushed;                          /* draws that are pushed toward the viewer by comfort.overlay_parallax, whatever else they are */
            std::vector<MatrixRule>       rule_vec;
            std::vector<SceneConvergence> scene_convergence_vec;
            ScreenParallaxSettings        screen_parallax;                 /* see ScreenParallaxSettings; not enabled unless the recipe has it */
            std::vector<SceneSeparation>  scene_separation_vec;            /* from "baked"; see SceneSeparation           */
            std::vector<uint64_t>         side_reaching_draw_key_vec;      /* from "baked": flat draws that span from the left edge to the right edge: a push widens their viewport */
            DrawSelector                  skipped;                         /* draws which are not played                 */
            std::string                   target_description;              /* of the application; for people             */
            std::string                   target_name;
            DrawSelector                  unaffected;
            UnknownFramePolicy            unknown_frame_policy    = UnknownFramePolicy::MONO;
            uint32_t                      version                 = 0;
            ViewAnchorShift               view_anchor_shift;               /* see ViewAnchorShift; not enabled unless the recipe has it */
        };
    }
}

#endif /* RECIPE_TYPES_H */
