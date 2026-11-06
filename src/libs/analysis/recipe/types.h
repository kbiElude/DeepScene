/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A recipe: which calls to change, and how, to render each eye. It is a JSON file (see recipe_loader.h); this is what it loads
 * into. Frames and passes are referred to by match keys (see KeyBuilder), which are the same in every run, never by frame
 * indices or analysis IDs, which are not.
 */
#ifndef RECIPE_TYPES_H
#define RECIPE_TYPES_H

#include "Common/types.h"
#include <array>
#include <stdint.h>
#include <string>
#include <vector>

namespace Analysis
{
    namespace Recipe
    {
        /* The axes of a matrix: the first three components of its last row move along them. */
        enum class Axis
        {
            X,
            Y,
            Z
        };

        /* What a rule does to the matrix of a call. */
        enum class ActionKind
        {
            EYE_CONVERGENCE, /* the projection is sheared by half the eye separation, for the eyes to converge at a distance: that of the
                              * draw (baked), or one set for the replay; with neither, nothing is changed. For a perspective
                              * projection (Direct3D, row vectors): P[2][0] moves by -/+ (separation / 2) * P[0][0] / distance, for the
                              * left / right eye                                                                                            */
            EYE_OFFSET,      /* a translation along an axis, by half the eye separation: toward the left for the left eye, toward the right
                              * for the right one; the separation is a setting of the replay, not of the recipe, and the translation is
                              * multiplied with the call's matrix in the order the rule says                                                */
            MULTIPLY,        /* the recipe's matrix is multiplied with the call's                                                           */
            REPLACE          /* the recipe's matrix is used instead                                                                         */
        };

        /* What the recipe does to a draw. */
        enum class DrawClass
        {
            AFFECTED,  /* the rules apply to it                                                      */
            SKIPPED,   /* it is not played at all, in either eye (the calls which set its state are) */
            UNAFFECTED /* the rules leave it alone                                                   */
        };

        /* The eye an application is rendered for. */
        enum class Eye
        {
            LEFT,
            RIGHT
        };

        /* Restricts a rule to a kind of matrix. Direct3D matrices are row-major and multiply row vectors: a perspective projection has
         * the last column (0, 0, 1, 0), an orthographic one (0, 0, 0, 1).
         */
        enum class MatrixKind
        {
            ANY,
            ORTHOGRAPHIC,
            PERSPECTIVE
        };

        /* The order of a product, with M the call's matrix and X the recipe's. */
        enum class MultiplyOrder
        {
            POST, /* M * X */
            PRE   /* X * M */
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

        /* What the person who writes a recipe says about how much the eyes can be asked to do. The nearest surfaces of a scene (see
         * SceneSeparation) are the ones that need them to turn most, and what the eyes can bear of that does not depend on the scene.
         */
        struct Comfort
        {
            /* How far apart the pictures of the eyes may be at the nearest surface of a scene, as a part of the width of the picture (0.03 is
             * three in a hundred): the eyes are kept from being further apart than that allows. 0: no limit.
             */
            float max_near_parallax = 0.0f;

            /* Which part of the surfaces of a scene (by vertices) counts as its nearest: 0.001 is the nearest thousandth. A scene is rarely near
             * everywhere, and what is very near is often a few large polygons, so this is small.
             */
            float near_fraction = 0.001f;

            /* How far toward the viewer the draws that the rules leave alone are pushed, as a part of the width of the picture (0.04 is four in
             * a hundred): the two pictures of them are that far apart, so that they stand in front of the screen, and in front of the surfaces of
             * a scene when the limit above is kept. Only for the draws that the application makes to what it presents (an interface, text); the
             * ones that are made to a texture are left as they are, for what they make is used elsewhere. 0: not pushed.
             */
            float overlay_parallax = 0.0f;
        };

        /* How the eyes are given parallax in an application whose vertices are already in the space of the picture (pre-transformed, as Direct3D
         * says: the position of a vertex is in pixels, with the reciprocal of its depth), which no matrix moves. There is no projection to read the
         * angle of view from, nor a camera to measure the distance of the surfaces by: the recipe says them.
         *
         * A vertex is moved sideways, for the left eye to the right and for the right eye to the left, by
         *   (width / 2) / tan(field_of_view / 2) * (eye separation / 2) * (1 / depth - 1 / convergence_distance)
         * pixels: a camera moved to the side, with the picture shifted so that what is at the convergence distance is not apart. A vertex at that
         * distance is not moved, which is where an interface (drawn flat at one depth) and a picture which is made of another one are left.
         */
        struct ScreenParallax
        {
            float convergence_distance  = 0.0f;  /* in the units of the camera of the application; what is there has no parallax           */
            float field_of_view_degrees = 90.0f; /* of the width of the picture                                                            */
            bool  is_enabled            = false; /* whether the recipe has the section                                                      */
        };

        /* The distance at which the eyes converge for a draw, by its signature, in the units of the camera of the application: measured
         * by RecipeBake from how deep the passes that draw it are (see DepthHistogram), so that nobody has to say it.
         */
        struct DrawConvergence
        {
            float    distance      = 0.0f;
            uint64_t signature_key = 0;
        };

        /* Says what a draw is by the signature it has when it is issued (see KeyBuilder::compute_draw_signature_key), which is all that
         * a converter which plays an application as it runs knows of it: the pass it belongs to is not known until the pass ends. Rules
         * are made from the lists of passes and draws by RecipeBake, and tried in order; a draw which none applies to is
         * affected.
         */
        struct DrawRule
        {
            DrawClass                          draw_class    = DrawClass::AFFECTED;
            uint64_t                           signature_key = 0;
            std::vector<std::array<float, 16>> view_matrix_vec; /* if not empty, the rule applies only while the view matrix is one of these */
        };

        /* A draw in one pass: a draw structure is often in several passes (a 3D scene and a pass that draws over it may share it), and the key of the draw alone
         * names it in all of them. A replay knows the passes; a converter which plays an application as it runs does not, and cannot tell this from the draw.
         */
        struct PassDraw
        {
            uint64_t draw_key = 0; /* of a draw structure */
            uint64_t pass_key = 0; /* of a pass structure */
        };

        /* Names passes and draws by their match keys (see KeyBuilder), which are the same in every run. */
        struct DrawSelector
        {
            std::vector<uint64_t> draw_key_vec;  /* keys of draw structures                                   */
            std::vector<PassDraw> pass_draw_vec; /* a draw in one pass only: the most specific name there is  */
            std::vector<uint64_t> pass_key_vec;  /* keys of pass structures                                   */
        };

        /* One change. It applies to a call of its function when every condition holds and the call is in a matching pass and frame
         * (no pass keys: any pass; no shape keys: any frame). Rules apply in recipe order, each to the matrix the previous one left.
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

        /* A texture which each eye has a copy of: what is rendered to it by one eye is not what the other eye sees, and what a frame reads of it
         * from the frame before is what that eye rendered. Told by how it was made and by the number of the textures made like it before it (see
         * ResourceIdentity), which is known when the application makes it; measured by RecipeBake from the textures that frames carry over.
         */
        struct PerEyeTexture
        {
            uint64_t description_key = 0;
            uint32_t ordinal         = 0;
        };

        /* The distance at which the eyes converge in a kind of frame (a scene), by its shape key: measured by RecipeBake from how
         * deep the affected passes of the frames of that shape are. A converter that knows the shape of the frame before (what it
         * is made of is known when it ends) follows it, and the distance of a draw (see DrawConvergence) is for what it does not know.
         */
        struct SceneConvergence
        {
            float    distance  = 0.0f;
            uint64_t shape_key = 0;
        };

        /* How far apart the eyes may be in a kind of frame (a scene), by its shape key, in the units of the camera of the application:
         * measured by RecipeBake from how near the surfaces of the scene are, and from the Comfort of the recipe. A scene which is not
         * listed has no limit. The same shape key as in SceneConvergence.
         */
        struct SceneSeparation
        {
            float    max_separation = 0.0f;
            uint64_t shape_key      = 0;
        };

        /* A draw is affected by the rules unless it is named by `unaffected`. Of the lists, a draw key counts for more than a pass key
         * (so a draw can be named `affected` inside an `unaffected` pass), and a key in a list of the same kind in both is refused.
         * A draw named by `skipped`, or in a pass it names, is not played at all, in either eye; the calls which set its state are.
         */
        struct Recipe
        {
            DrawSelector                  affected;
            Comfort                       comfort;                         /* what the person says; see Comfort           */
            std::vector<DrawConvergence>  convergence_vec;                 /* from "baked"; see DrawConvergence          */
            std::vector<DrawRule>         draw_rule_vec;                   /* from "baked"; see DrawRule                 */
            bool                          is_baked                = false; /* whether the file has draw rules, even none */
            std::vector<uint64_t>         known_shape_key_vec;             /* if empty, no frame is unknown              */
            std::vector<uint64_t>         covering_draw_key_vec;      /* from "baked"; keys (signature and size) of draws that cover the whole of the view: not to be moved sideways */
            std::vector<uint64_t>         known_signature_key_vec;         /* from "baked"; if empty, no draw is unknown */
            uint32_t                      key_scheme_version      = 0;
            std::vector<PerEyeTexture>    per_eye_texture_vec;             /* from "baked"; see PerEyeTexture             */
            std::vector<Rule>             rule_vec;
            std::vector<SceneConvergence> scene_convergence_vec;
            ScreenParallax                screen_parallax;                 /* see ScreenParallax; not enabled unless the recipe has it */
            std::vector<SceneSeparation>  scene_separation_vec;            /* from "baked"; see SceneSeparation           */
            DrawSelector                  skipped;                         /* draws which are not played                 */
            std::string                   target_description;              /* of the application; for people             */
            std::string                   target_name;
            DrawSelector                  unaffected;
            UnknownFramePolicy            unknown_frame_policy    = UnknownFramePolicy::MONO;
            uint32_t                      version                 = 0;
        };
    }
}

#endif /* RECIPE_TYPES_H */
