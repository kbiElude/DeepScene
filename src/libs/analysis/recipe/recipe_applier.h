/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Decides which rules of a recipe apply to a call, and what a rule does to a matrix. It knows nothing about how calls are
 * played: the caller reads the matrix of a call, asks for the rules, and puts the result back.
 */
#ifndef RECIPE_RECIPE_APPLIER_H
#define RECIPE_RECIPE_APPLIER_H

#include "analysis/api_dump_analysis/resource_identity.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include "analysis/recipe/types.h"
#include <array>
#include <map>
#include <set>
#include <stdint.h>
#include <unordered_map>
#include <vector>

namespace Analysis
{
    namespace Recipe
    {
        /* Where a call is: in what frame, and in what pass of it, if any (see ModelBuilder::find_pass_index). */
        struct CallPlace
        {
            bool     has_pass  = false;
            uint64_t pass_key  = 0;
            uint64_t shape_key = 0;
        };

        class RecipeApplier
        {
        public:
            explicit RecipeApplier(const Analysis::Recipe::Recipe& in_recipe);

            /* What the recipe does to a draw by the signature it has when it is issued (see Recipe::draw_rule_vec), given the view matrix
             * in effect then (16 numbers, row by row): that of the first rule of the signature which applies. A draw which no rule
             * applies to is affected. For a converter which plays an application as it runs; a replay knows the passes, and asks
             * is_draw_affected() and is_draw_skipped().
             */
            Analysis::Recipe::DrawClass classify_draw(const uint64_t&              in_signature_key,
                                                      const std::array<float, 16>& in_view_matrix) const;

            /* The distance at which the eyes converge for the draws of the signature (see Recipe::convergence_vec); 0 if the recipe says
             * none.
             */
            float get_convergence_distance(const uint64_t& in_signature_key) const;

            /* The rules about the call, in the order they apply. The kind of matrix is checked later, by apply_rule(). The pointers
             * last as long as the applier.
             */
            std::vector<const Analysis::Recipe::Rule*> get_matching_rule_ptr_vec(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                 const CallPlace&                             in_place) const;

            /* Whether the recipe has the reads of one pixel that the application makes in a run given from a picture that is read once (see Recipe::caches_pixel_reads). */
            bool get_caches_pixel_reads() const;

            /* What the person who writes the recipe says about how much the eyes can be asked to do. */
            const Comfort& get_comfort() const;

            /* What the comfort of the recipe makes of a scene that is this deep (the depth of the surfaces that are nearest and farthest, and the median of all of them, in the units
             * of the camera of the application, and the x scale of its projection, which is 1 over the tangent of half the angle of view of the width of the picture): where the eyes
             * converge and how far apart they may be so that no surface is further apart in the pictures than Comfort says; what the recipe says of the scene (a SceneComfort, or
             * none) wins. With both limits and Comfort::balances_convergence the eyes converge where the whole depth fits them best: 1 / d = 1 / near - (1 / near - 1 / far) * near limit /
             * (near limit + far limit).
             */
            static ComfortLimits get_comfort_limits(const Comfort&      in_comfort,
                                                    const SceneComfort* in_scene_ptr,
                                                    const float&        in_median_depth,
                                                    const float&        in_near_depth,
                                                    const float&        in_far_depth,
                                                    const float&        in_projection_scale);

            /* How far toward the viewer a draw of the cover key is pushed, if it is pushed: the part parallax for a part of an object that protrudes (see
             * DrawSelector::part_cover_key_vec) if the recipe has one, the overlay parallax otherwise.
             */
            float get_cover_push_parallax(const uint64_t& in_cover_key) const;

            /* How far toward the viewer the draws that the rules leave alone are pushed (see Comfort::overlay_parallax); 0: not. */
            float get_overlay_parallax() const;

            /* What the recipe says of the scene (see Comfort::scene_override_vec); nullptr if it says nothing. */
            const SceneComfort* find_scene_comfort_ptr(const uint64_t& in_shape_key) const;

            /* The distance at which the eyes converge in the frames of the shape (see Recipe::scene_convergence_vec); 0 if the recipe says
             * none.
             */
            float get_scene_convergence_distance(const uint64_t& in_shape_key) const;

            /* How far apart the eyes may be in the frames of the shape (see Recipe::scene_separation_vec); 0 if the recipe says none. */
            float get_scene_max_separation(const uint64_t& in_shape_key) const;

            /* How vertices that no matrix moves are given parallax (see ScreenParallax); not enabled if the recipe does not say. */
            const Analysis::Recipe::ScreenParallax& get_screen_parallax() const;

            Analysis::Recipe::UnknownFramePolicy get_unknown_frame_policy() const;

            /* The view anchor shift of the recipe (see ViewAnchorShift); not enabled if the recipe does not say. */
            const Analysis::Recipe::ViewAnchorShift& get_view_anchor_shift() const;

            /* False if no rule is about the function, whatever the arguments: a quick check before the call is looked at. */
            bool has_rule_for(const APIInterceptor::APIFunction& in_function) const;

            /* Whether the recipe has a view anchor shift (see ViewAnchorShift): the camera of a draw is moved by the call that begins it. */
            bool has_view_anchor_shift() const;

            /* Whether the rules apply to a draw (see Recipe::affected): the draw in its pass counts for more than the key of the draw, which counts for more
             * than the key of its pass, and a draw which none of them names is affected.
             */
            bool is_draw_affected(const uint64_t& in_pass_key,
                                  const uint64_t& in_draw_key) const;

            /* Whether a draw of the key (its signature and size; see KeyBuilder::compute_draw_cover_key()) is made in the space of the picture and not over a 3D scene, which the
             * baker found (see Recipe::flat_layer_draw_key_vec): it is left as the application makes it. False unless Comfort::uses_automatic_layers.
             */
            bool is_cover_flat_layer(const uint64_t& in_cover_key) const;

            /* Whether a draw of the key (its signature and size; see KeyBuilder::compute_draw_cover_key()) is pushed toward the viewer (see DrawSelector::cover_key_vec, and
             * Recipe::overlay_draw_key_vec with Comfort::uses_automatic_layers); a
             * draw can be pushed by that and by the other names too.
             */
            bool is_cover_pushed(const uint64_t& in_cover_key) const;

            /* Whether a draw of the key (its signature and size; see KeyBuilder::compute_draw_cover_key()) covers the whole of the view, as a
             * flash does; see Recipe::covering_draw_key_vec.
             */
            bool is_draw_covering(const uint64_t& in_cover_key) const;

            /* Whether the draw is pushed toward the viewer (see Recipe::pushed): the draw in its pass, its key, or the key of its pass is named. */
            bool is_draw_pushed(const uint64_t& in_pass_key,
                                const uint64_t& in_draw_key) const;

            /* Whether the draw is not to be played (see Recipe::skipped): the draw in its pass, its key, or the key of its pass is named. */
            bool is_draw_skipped(const uint64_t& in_pass_key,
                                 const uint64_t& in_draw_key) const;

            /* Whether a draw of the key (see KeyBuilder::compute_draw_cover_key()) has its vertices in the space of the eye, so that it is not taken for attached to the camera
             * (see Comfort::eye_space_cover_key_vec).
             */
            bool is_eye_space_cover(const uint64_t& in_cover_key) const;

            /* True if the frame is one the recipe was made for: its shape is listed, or no shape is. */
            bool is_frame_known(const uint64_t& in_shape_key) const;

            /* Whether a draw of the key (see KeyBuilder::compute_draw_cover_key()) is a part of an object that protrudes from it (see DrawSelector::part_cover_key_vec). */
            bool is_part_cover(const uint64_t& in_cover_key) const;

            /* True if the recipe says that each eye has a copy of the texture (see PerEyeTexture). */
            bool is_per_eye_texture(const Analysis::APIDumpAnalysis::ResourceIdentity& in_identity) const;

            /* Whether a draw of the key (see KeyBuilder::compute_draw_cover_key()) is a flat overlay that reaches from the left edge of the picture to the right one (see
             * Recipe::side_reaching_draw_key_vec): moved sideways, it is made with the picture wider by the shift, so that no edge is bare.
             */
            bool is_side_reaching(const uint64_t& in_cover_key) const;

            /* True if the recipe knows the signature of a draw: the analysis it was baked from saw it, or the recipe lists none. A draw of
             * an unknown signature is affected, like any draw which no rule applies to, but the recipe was not made for it.
             */
            bool is_signature_known(const uint64_t& in_signature_key) const;

            /* Whether the draw is pushed toward the viewer, by its signature and the view matrix (the rule that classify_draw() takes says so; see DrawRule::is_pushed). For a converter
             * which plays an application as it runs.
             */
            bool is_signature_pushed(const uint64_t&              in_signature_key,
                                     const std::array<float, 16>& in_view_matrix) const;

            /* The matrix that the camera of a draw begins with for the eye (see ViewAnchorShift), as OpenGL has matrices: the columns one after the other. The eyes are
             * apart by the separation, but no more than the scene of the shape allows (see SceneSeparation); they converge at the distance of the scene (see
             * SceneConvergence), or else at the one of the recipe, and with neither the matrix is the translation alone.
             */
            std::array<float, 16> make_view_anchor_matrix(const Analysis::Recipe::Eye& in_eye,
                                                          const float&                 in_eye_separation,
                                                          const uint64_t&              in_shape_key) const;


            /* The rule for the eye, for a draw that is pushed toward the viewer (see Comfort::overlay_parallax): if the rule is the one
             * of a projection (an eye convergence) and the matrix is a perspective one, the picture of the eye is moved sideways by the
             * parallax, to the right for the left eye. Returns false, and leaves the matrix as it is, for any other rule or matrix.
             */
            static bool apply_overlay_shift(const Analysis::Recipe::Rule& in_rule,
                                            const Analysis::Recipe::Eye&  in_eye,
                                            const float&                  in_overlay_parallax,
                                            std::array<float, 16>*        inout_matrix_ptr);

            /* Applies the rule for the eye to the matrix (16 numbers, row by row). The eye separation is the distance between the
             * eyes, in the units of the matrix; an eye offset and an eye convergence use it. The convergence distance, in the same units,
             * is where the eyes converge; an eye convergence uses it, and does nothing if it is not more than zero. Returns false, and
             * leaves the matrix as it is, if the rule is about another kind of matrix.
             */
            static bool apply_rule(const Analysis::Recipe::Rule& in_rule,
                                   const Analysis::Recipe::Eye&  in_eye,
                                   const float&                  in_eye_separation,
                                   std::array<float, 16>*        inout_matrix_ptr,
                                   const float&                  in_convergence_distance = 0.0f);

        private:
            /* Signature -> positions in the recipe's draw rules. */
            typedef std::unordered_map<uint64_t, std::vector<uint32_t> > DrawRuleIndexMap;

            /* A draw in one pass: the key of the pass and the key of the draw. */
            typedef std::pair<uint64_t, uint64_t> PassDrawKey;


            /* The first rule of the signature that applies given the view matrix; nullptr if none does. */
            const Analysis::Recipe::DrawRule* find_draw_rule_ptr(const uint64_t&              in_signature_key,
                                                                 const std::array<float, 16>& in_view_matrix) const;

            static bool is_matrix_of_kind(const std::array<float, 16>&        in_matrix,
                                          const Analysis::Recipe::MatrixKind& in_kind);

            /* The translation of an eye along an axis, as a row-major matrix of row vectors. */
            static std::array<float, 16> make_eye_offset_matrix(const Analysis::Recipe::Axis& in_axis,
                                                                const Analysis::Recipe::Eye&  in_eye,
                                                                const float&                  in_eye_separation);

            static std::array<float, 16> multiply(const std::array<float, 16>& in_a,
                                                  const std::array<float, 16>& in_b);


            std::set<uint64_t>                    m_affected_draw_key_set;
            std::set<PassDrawKey>                 m_affected_pass_draw_set;
            std::set<uint64_t>                    m_affected_pass_key_set;
            std::set<uint64_t>                    m_automatic_flat_layer_cover_key_set;
            std::set<uint64_t>                    m_automatic_overlay_cover_key_set;
            std::map<uint64_t, float>             m_convergence_distance_map;
            DrawRuleIndexMap                      m_draw_rule_index_map;
            std::set<uint64_t>                    m_covering_draw_key_set;
            std::set<uint64_t>                    m_eye_space_cover_key_set;
            std::set<uint64_t>                    m_known_signature_key_set;
            Analysis::Recipe::Recipe              m_recipe;
            std::set<APIInterceptor::APIFunction> m_rule_function_set;
            std::set<uint64_t>                    m_pushed_cover_key_set;
            std::set<uint64_t>                    m_pushed_draw_key_set;
            std::set<uint64_t>                    m_pushed_part_cover_key_set;
            std::set<PassDrawKey>                 m_pushed_pass_draw_set;
            std::set<uint64_t>                    m_pushed_pass_key_set;
            std::map<uint64_t, float>             m_scene_convergence_distance_map;
            std::map<uint64_t, float>             m_scene_max_separation_map;
            std::set<uint64_t>                    m_side_reaching_draw_key_set;
            std::set<uint64_t>                    m_skipped_draw_key_set;
            std::set<PassDrawKey>                 m_skipped_pass_draw_set;
            std::set<uint64_t>                    m_skipped_pass_key_set;
            std::set<uint64_t>                    m_unaffected_draw_key_set;
            std::set<PassDrawKey>                 m_unaffected_pass_draw_set;
            std::set<uint64_t>                    m_unaffected_pass_key_set;
        };
    }
}

#endif /* RECIPE_RECIPE_APPLIER_H */
