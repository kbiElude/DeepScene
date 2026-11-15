/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A recipe says which calls to change, and how, to render each eye. It is a JSON file (see recipe_loader.h), and these are the types it loads
 * into. Frames and passes are referred to by match keys (see KeyBuilder). Match keys are the same in every run, unlike frame indices and
 * analysis IDs, which can differ between runs.
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
            EYE_CONVERGENCE, /* shears the projection by half the eye separation, so that the eyes converge at a distance: the distance of the draw
                              * (baked), or one that is set for the replay. With neither, nothing is changed. For a perspective projection (Direct3D,
                              * row vectors), P[2][0] changes by -/+ (separation / 2) * P[0][0] / distance for the left / right eye */
            EYE_OFFSET,      /* translates along an axis by half the eye separation: +separation / 2 for the left eye and -separation / 2 for the
                              * right eye (applied to a view matrix, this moves the camera toward the left for the left eye). The separation is a
                              * setting of the replay, not of the recipe. The translation is multiplied with the matrix of the call in the order that
                              * the rule gives                                                                                      */
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

        /* What the recipe author says about one scene (a kind of frame, identified by its shape key as in SceneConvergence), when the limits of the
         * whole recipe (see Comfort) or the measured distance do not suit it, for example a scene that is all near, or one with a long view. A value
         * of 0 means that nothing is said. What is said wins over what was measured and over what the recipe says for all scenes.
         */
        struct SceneComfort
        {
            float    convergence_distance = 0.0f; /* in the units of the camera of the application                                                          */
            float    max_far_parallax     = 0.0f; /* as Comfort::max_far_parallax                                                                           */
            float    max_near_parallax    = 0.0f; /* as Comfort::max_near_parallax                                                                          */
            float    max_separation       = 0.0f; /* the limit on how far apart the eyes may be, in the units of the camera; wins over the two limits above */
            uint64_t shape_key            = 0;
        };

        /* What the comfort settings of a recipe make of the depth of a scene: where the eyes converge, and how far apart they may be (0 means no limit). */
        struct ComfortLimits
        {
            float distance       = 0.0f;
            bool  is_far_limit   = false; /* the limit comes from the farthest surfaces */
            float max_separation = 0.0f;
        };

        /* What the recipe author says about how much the eyes can be asked to do. The nearest surfaces of a scene (see SceneSeparation) need the
         * eyes to turn the most, and how much the eyes can bear of that does not depend on the scene.
         */
        struct Comfort
        {
            /* Also use the layers that the baker found (flat_layer_draw_key_vec, overlay_draw_key_vec and eye_space_draw_key_vec), in addition to
             * the names in the recipe. Off by default.
             */
            bool uses_automatic_layers = false;

            /* Whether the convergence distance of a scene is chosen to fit its depth into the two parallax limits below, instead of being the
             * median depth of its vertices. With both limits (max_near_parallax and max_far_parallax) set, the eyes converge where the nearest
             * surfaces use the whole near limit and the farthest surfaces use the whole far limit. That is where the eyes may be furthest apart.
             * A scene with a sky behind a near object (where the median depth is the sky) is then not made flat. A distance that a scene sets for
             * itself (see SceneComfort) is kept.
             */
            bool balances_convergence = false;

            /* A draw whose origin is nearer to the camera than this fraction of the convergence distance is treated as attached to the camera (an
             * interface drawn with a perspective projection, a weapon, a sky centred on the viewer). Such a draw is drawn the same for both eyes.
             * The parallax of a draw depends on how far away it is, so a draw this near would get nearly the whole separation, which is more than
             * the surfaces of the scene may have. 0 means that no draw is treated as attached.
             */
            float camera_attached_depth = 0.0f;

            /* Keys of draws (signature and size) whose vertices are already in eye space (the model-view matrix is the identity). They are not
             * treated as attached to the camera, and they get the parallax of the depth of their vertices.
             */
            std::vector<uint64_t> eye_space_cover_key_vec;

            /* How far apart the pictures of the eyes may be at the farthest surface of a scene, as a fraction of the picture width. What lies
             * beyond the convergence distance is seen with eyes that turn toward each other less than at the screen. If the pictures were further
             * apart than the distance between a person's eyes on the screen, the eyes would have to turn away from each other, which eyes cannot
             * do. 0 means no limit.
             */
            float max_far_parallax = 0.0f;

            /* How far apart the pictures of the eyes may be at the nearest surface of a scene, as a fraction of the picture width (0.03 is 3 in
             * 100). The eye separation is reduced so that this limit is not exceeded. 0 means no limit.
             */
            float max_near_parallax = 0.0f;

            /* Whether a converter that can (see StereoCore::on_frame_depth) measures the depth of the surfaces of each frame while the application
             * renders it, and checks that depth against the limits, instead of using what was baked for the scene (see SceneSeparation). The baked
             * values only cover scenes whose depth the analysis can see, not a program that draws with display lists, and not a camera that flies
             * past a near surface. What a recipe says about a scene (see SceneComfort) is kept.
             */
            bool measures_depth = false;

            /* Which fraction of the surfaces of a scene (counted by vertices) counts as its nearest: 0.001 is the nearest thousandth. A scene is
             * rarely near everywhere, and what is very near is often a few large polygons, so the value is small.
             */
            float near_fraction = 0.001f;

            /* How far toward the viewer the draws that the rules leave alone are pushed, as a fraction of the picture width (0.04 is 4 in 100).
             * The two pictures of such a draw are this far apart, so the draw stands in front of the screen, and in front of the surfaces of a
             * scene when the parallax limits above are kept. This applies only to draws that the application makes to what it presents (an
             * interface, text). Draws made to a texture are left as they are, because the texture is used elsewhere. 0 means no push.
             */
            float overlay_parallax = 0.0f;

            /* How far toward the viewer the parts of an object that protrude from it (the eyes of a robot, its siren) are pushed, as a fraction
             * of the picture width. It applies when the recipe names the parts (DrawSelector::part_cover_key_vec). A protruding part is only
             * slightly nearer than the rest of the object, so this value is smaller than overlay_parallax. 0 means the same as overlay_parallax.
             */
            float part_parallax = 0.0f;

            /* What the recipe says about single scenes, identified by their shape keys. */
            std::vector<SceneComfort> scene_override_vec;
        };

        /* How the eyes get parallax in an application whose vertices are already in the space of the picture (pre-transformed, as Direct3D calls
         * it: a vertex position is in pixels, together with the reciprocal of its depth). No matrix moves such vertices. There is no projection to
         * read the angle of view from and no camera to measure the distance of the surfaces with, so the recipe provides both.
         *
         * A vertex is moved sideways by
         *   (width / 2) / tan(field_of_view / 2) * (eye separation / 2) * (1 / depth - 1 / convergence_distance)
         * pixels, to the right for the left eye and to the left for the right eye. This is the same as moving the camera sideways and shifting
         * the picture so that what is at the convergence distance has no parallax. A vertex at that distance is not moved. That is how an
         * interface (drawn flat at one depth) and a picture that is made from another picture are left unmoved.
         */
        struct ScreenParallax
        {
            float convergence_distance  = 0.0f;  /* in the units of the camera of the application; content at this distance has no parallax */
            float field_of_view_degrees = 90.0f; /* the angle of view across the width of the picture                                       */
            bool  is_enabled            = false; /* whether the recipe has the section                                                      */
        };

        /* The distance at which the eyes converge for a draw, identified by its signature, in the units of the camera of the application.
         * RecipeBake measures it from how deep the passes that draw it are (see DepthHistogram), so nobody has to specify it.
         */
        struct DrawConvergence
        {
            float    distance      = 0.0f;
            uint64_t signature_key = 0;
        };

        /* A rule that classifies draws by the signature a draw has when it is issued (see KeyBuilder::compute_draw_signature_key). The signature
         * is all that a converter, which plays an application while it runs, knows about a draw, because the pass of the draw is not known until
         * the pass ends. RecipeBake builds the rules from the lists of passes and draws. Rules are tried in order, and a draw that no rule applies
         * to is affected.
         */
        struct DrawRule
        {
            DrawClass                          draw_class    = DrawClass::AFFECTED;
            bool                               is_pushed     = false;       /* the draw is moved toward the viewer (Recipe::pushed) */
            uint64_t                           signature_key = 0;
            std::vector<std::array<float, 16>> view_matrix_vec; /* if not empty, the rule applies only while the view matrix is one of these */
        };

        /* A draw in one pass. A draw structure is often in several passes (a 3D scene and a pass that draws over it may share it), and the key
         * of the draw alone names it in all of them. A replay knows the passes, so it can name a draw in one pass only. A converter that plays
         * an application while it runs does not know the passes, so it cannot tell this pair from the plain draw.
         */
        struct PassDraw
        {
            uint64_t draw_key = 0; /* of a draw structure */
            uint64_t pass_key = 0; /* of a pass structure */
        };

        /* Names passes and draws by their match keys (see KeyBuilder), which are the same in every run. */
        struct DrawSelector
        {
            std::vector<uint64_t> cover_key_vec; /* keys of draws by signature and size (KeyBuilder::compute_draw_cover_key); only "pushed" has them.
                                                  * They tell apart the draws of one structure (the parts of one object, which are drawn alike).
                                                  * They are flat as well: drawn once for both eyes, like a draw that the rules leave alone                     */
            std::vector<uint64_t> draw_key_vec;  /* keys of draw structures                                                                                     */
            std::vector<uint64_t> part_cover_key_vec; /* like cover_key_vec, for the parts of an object that protrude from it; pushed by Comfort::part_parallax */
            std::vector<PassDraw> pass_draw_vec; /* a draw in one pass only: the most specific name there is                                                    */
            std::vector<uint64_t> pass_key_vec;  /* keys of pass structures                                                                                     */
        };

        /* One change to a matrix. A rule applies to a call of its function when every condition holds and the call is in a matching pass and
         * frame (no pass keys means any pass, no shape keys means any frame). Rules apply in recipe order, and each rule works on the matrix that
         * the previous rule produced.
         */
        struct Rule
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

        /* A texture of which each eye has its own copy. What one eye renders to it is not what the other eye sees, and what a frame reads from it
         * from the previous frame is what that eye rendered. It is identified by how it was made and by the number of earlier textures made the
         * same way (see ResourceIdentity), which is known when the application creates it. RecipeBake measures it from the textures that frames
         * carry over.
         */
        struct PerEyeTexture
        {
            uint64_t description_key = 0;
            uint32_t ordinal         = 0;
        };

        /* The distance at which the eyes converge in a kind of frame (a scene), identified by its shape key. RecipeBake measures it from how deep
         * the affected passes of the frames of that shape are. A converter that knows the shape of the previous frame (what a frame is made of is
         * known when it ends) uses this distance. The distance of a draw (see DrawConvergence) is the fallback for what the converter does not know.
         */
        struct SceneConvergence
        {
            float    distance  = 0.0f;
            uint64_t shape_key = 0;
        };

        /* How far apart the eyes may be in a kind of frame (a scene), identified by its shape key, in the units of the camera of the application.
         * RecipeBake measures it from how near the surfaces of the scene are, and from the Comfort of the recipe. A scene that is not listed has
         * no limit. The shape key is the same as in SceneConvergence.
         */
        struct SceneSeparation
        {
            float    max_separation = 0.0f;
            uint64_t shape_key      = 0;
        };

        /* How the eyes get parallax when the camera of a draw is built by many calls on one matrix, and not set by a call of its own (OpenGL,
         * whose model-view matrix is a stack that the program loads, multiplies, pushes and pops). The call that begins the camera
         * (DrawInstance::view_anchor_call_index: the last glLoadIdentity or glLoadMatrix of the matrix) loads the shift of the eye on top of what
         * it loads, so everything that the program does afterwards is applied on top of the shift.
         *
         * In the space of the camera, which looks down the negative z axis, the shift is x + h + (h / d) * z for the left eye, where h is half
         * the eye separation and d is the distance at which the eyes converge. For the right eye the sign of h is reversed. This moves the camera
         * sideways and shifts the picture so that what is at distance d has no parallax. Without a distance (0), only the translation is applied.
         */
        struct ViewAnchorShift
        {
            float convergence_distance        = 0.0f;  /* where the eyes converge when the scene gives no distance; in the units of the camera of the application; 0 means none */
            bool  is_enabled                  = false; /* whether the recipe has the section                                                                                    */
            /* Whether a draw that the recipe affects, but whose camera no call begins (such as an interface drawn with an identity projection), is
             * played as it is. Otherwise such a draw is refused.
             */
            bool  leaves_draws_without_anchor = false;
        };

        /* A draw is affected by the rules unless `unaffected` names it. A draw key counts for more than a pass key, so a draw can be named in
         * `affected` inside a pass that is named in `unaffected`. A key of the same kind that is named in both `affected` and `unaffected` is
         * refused. A draw named by `skipped`, or in a pass that it names, is not played at all, in either eye. The calls that set its state are
         * played.
         */
        struct Recipe
        {
            DrawSelector                  affected;
            /* Serves the single-pixel reads that an application makes in a run from one picture that is read once (see PictureReadbackCache).
             * Off unless the recipe asks, because it changes where the data that the application reads comes from.
             */
            bool                          caches_pixel_reads      = false;
            Comfort                       comfort;                         /* set by the recipe author; see Comfort      */
            std::vector<DrawConvergence>  convergence_vec;                 /* from "baked"; see DrawConvergence          */
            std::vector<DrawRule>         draw_rule_vec;                   /* from "baked"; see DrawRule                 */
            std::vector<uint64_t>         eye_space_draw_key_vec;          /* from "baked": vertices in eye space (see Comfort::uses_automatic_layers) */
            std::vector<uint64_t>         flat_layer_draw_key_vec;         /* from "baked": draws in the space of the picture, never over a 3D scene: left as made */
            bool                          is_baked                = false; /* whether the file has a list of draw rules, even an empty one */
            std::vector<uint64_t>         known_shape_key_vec;             /* if empty, no frame is unknown              */
            std::vector<uint64_t>         covering_draw_key_vec;      /* from "baked"; keys (signature and size) of draws that cover the whole view: not moved sideways */
            std::vector<uint64_t>         known_signature_key_vec;         /* from "baked"; if empty, no draw is unknown */
            uint32_t                      key_scheme_version      = 0;
            std::vector<uint64_t>         overlay_draw_key_vec;            /* from "baked": draws in the space of the picture over a 3D scene: left as made and pushed */
            std::vector<PerEyeTexture>    per_eye_texture_vec;             /* from "baked"; see PerEyeTexture             */
            DrawSelector                  pushed;                          /* draws that are pushed toward the viewer by comfort.overlay_parallax, whatever else they are */
            std::vector<Rule>             rule_vec;
            std::vector<SceneConvergence> scene_convergence_vec;
            ScreenParallax                screen_parallax;                 /* see ScreenParallax; not enabled unless the recipe has it */
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
