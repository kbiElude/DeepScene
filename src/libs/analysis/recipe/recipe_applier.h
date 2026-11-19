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
        /* Where a call is: in which frame (identified by its shape key) and, if it is in a pass, which pass (see ModelBuilder::find_pass_index). */
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

            /* Classifies a draw by its signature (see Recipe::draw_rule_vec) and the current view matrix (16 numbers, row by row). The
             * first applicable rule decides; with none the draw is affected. For live converters; a replay uses is_draw_affected() and
             * is_draw_skipped().
             */
            Analysis::Recipe::DrawClass classify_draw(const uint64_t&              in_signature_key,
                                                      const std::array<float, 16>& in_view_matrix) const;

            /* Decides what is done to a draw: affected, left alone or skipped, and whether it is pushed. Replay and live converter both
             * call it so they decide alike.
             * With a pass (DrawQuery::has_pass) the draw is named by pass and structure (see is_draw_skipped(), is_draw_affected(),
             * is_draw_pushed()); without one by signature and view matrix (see classify_draw(), is_signature_pushed()). Then the cover
             * key applies: a baked flat layer or a draw pushed by cover key is left alone, and the latter is pushed. A caller that cannot
             * leave a draw alone sets DrawQuery::is_cover_hold_waived; the waiver does not apply to a baked flat layer.
             */
            Analysis::Recipe::DrawDecision decide_draw(const Analysis::Recipe::DrawQuery& in_query) const;

            /* Returns the convergence distance for the draws with this signature (see Recipe::convergence_vec), or 0 if none. */
            float get_convergence_distance(const uint64_t& in_signature_key) const;

            /* Returns the rules about the call, in apply order; apply_rule() checks the matrix kind. Pointers live as long as the applier. */
            std::vector<const Analysis::Recipe::MatrixRule*> get_matching_rule_ptr_vec(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                       const CallPlace&                             in_place) const;

            /* Whether the recipe serves single-pixel reads from one picture read once (see Recipe::caches_pixel_reads). */
            bool get_caches_pixel_reads() const;

            /* Returns the author's comfort settings. */
            const Comfort& get_comfort() const;

            /* Computes where the eyes converge and how far apart they may be for a scene, so that no surface exceeds the Comfort limits.
             * Depths (nearest, farthest, median) are in camera units; the projection scale is the x scale (1 / tan(half the angle of view)).
             * A SceneComfort (nullptr for none) wins. With both limits and Comfort::balances_convergence:
             * 1 / d = 1 / near - (1 / near - 1 / far) * near limit / (near limit + far limit).
             */
            static ComfortLimits get_comfort_limits(const Comfort&      in_comfort,
                                                    const SceneComfort* in_scene_ptr,
                                                    const float&        in_median_depth,
                                                    const float&        in_near_depth,
                                                    const float&        in_far_depth,
                                                    const float&        in_projection_scale);

            /* Returns how far toward the viewer a draw with this cover key is pushed: the part parallax for a protruding part (see
             * DrawSelector::part_cover_key_vec) if set, else the overlay parallax.
             */
            float get_cover_push_parallax(const uint64_t& in_cover_key) const;

            /* Returns the class from the draw and pass names alone: SKIPPED (see is_draw_skipped()), AFFECTED (see is_draw_affected()),
             * else UNAFFECTED. decide_draw() adds the cover key.
             */
            Analysis::Recipe::DrawClass get_named_draw_class(const uint64_t& in_pass_key,
                                                             const uint64_t& in_draw_key) const;

            /* Returns how far toward the viewer the draws that the rules leave alone are pushed (see Comfort::overlay_parallax). 0 means no push. */
            float get_overlay_parallax() const;

            /* Whether the recipe has the blocks between glBegin and glEnd recorded and made again for each eye (see Recipe::records_immediate_mode_blocks). */
            bool get_records_immediate_mode_blocks() const;

            /* Whether the recipe has the right eye drawn once for a run of the calls, after the left eye (see Recipe::renders_left_then_right_eye). */
            bool get_renders_left_then_right_eye() const;

            /* Returns what the recipe says about the scene (see Comfort::scene_override_vec), or nullptr if it says nothing. */
            const SceneComfort* find_scene_comfort_ptr(const uint64_t& in_shape_key) const;

            /* Returns the convergence distance for the shape (see Recipe::scene_convergence_vec), or 0 if none. */
            float get_scene_convergence_distance(const uint64_t& in_shape_key) const;

            /* Returns how far apart the eyes may be in the frames of the shape (see Recipe::scene_separation_vec), or 0 if the recipe gives no limit. */
            float get_scene_max_separation(const uint64_t& in_shape_key) const;

            /* Returns how vertices that no matrix moves get parallax (see ScreenParallaxSettings); not enabled without the section. */
            const Analysis::Recipe::ScreenParallaxSettings& get_screen_parallax() const;

            Analysis::Recipe::UnknownFramePolicy get_unknown_frame_policy() const;

            /* Returns the view anchor shift of the recipe (see ViewAnchorShift). It is not enabled if the recipe does not have the section. */
            const Analysis::Recipe::ViewAnchorShift& get_view_anchor_shift() const;

            /* Returns false if no rule is about the function, whatever its arguments. It is a quick check to run before looking at a call. */
            bool has_rule_for(const APIInterceptor::APIFunction& in_function) const;

            /* Whether the recipe has a view anchor shift (see ViewAnchorShift): the camera of a draw is moved by the call that begins it. */
            bool has_view_anchor_shift() const;

            /* Whether the rules apply to a draw (see Recipe::affected). Most specific name wins: draw in pass, then draw key, then pass
             * key. A draw named by none is affected.
             */
            bool is_draw_affected(const uint64_t& in_pass_key,
                                  const uint64_t& in_draw_key) const;

            /* Whether the draw is an automatic flat layer, left as the application makes it (see Recipe::flat_layer_draw_key_vec). */
            bool is_cover_flat_layer(const uint64_t& in_cover_key) const;

            /* Whether the draw is pushed toward the viewer by its key (DrawSelector::cover_key_vec, or Recipe::overlay_draw_key_vec). */
            bool is_cover_pushed(const uint64_t& in_cover_key) const;

            /* Whether a draw of the key (see MatchKeyBuilder::compute_draw_cover_key()) covers the whole view; see Recipe::covering_draw_key_vec. */
            bool is_draw_covering(const uint64_t& in_cover_key) const;

            /* Whether the draw is pushed toward the viewer (see Recipe::pushed): the draw in its pass, its key, or the key of its pass is named. */
            bool is_draw_pushed(const uint64_t& in_pass_key,
                                const uint64_t& in_draw_key) const;

            /* Whether the draw is not to be played (see Recipe::skipped): the draw in its pass, its key, or the key of its pass is named. */
            bool is_draw_skipped(const uint64_t& in_pass_key,
                                 const uint64_t& in_draw_key) const;

            /* Whether the draw has its vertices in eye space, so it is not treated as attached to the camera. */
            bool is_eye_space_cover(const uint64_t& in_cover_key) const;

            /* True if the recipe was made for this frame: its shape is listed, or the recipe lists no shapes. */
            bool is_frame_known(const uint64_t& in_shape_key) const;

            /* Whether a draw with this key (see MatchKeyBuilder::compute_draw_cover_key()) is a protruding part (see DrawSelector::part_cover_key_vec). */
            bool is_part_cover(const uint64_t& in_cover_key) const;

            /* True if the recipe says that each eye has a copy of the texture (see PerEyeTexture). */
            bool is_per_eye_texture(const Analysis::APIDumpAnalysis::ResourceIdentity& in_identity) const;

            /* Whether the draw is a flat overlay spanning edge to edge; pushing it widens its viewport so no edge stays bare. */
            bool is_side_reaching(const uint64_t& in_cover_key) const;

            /* True if the baked analysis saw the signature, or the recipe lists none. An unknown signature is affected, but the recipe was
             * not made for it.
             */
            bool is_signature_known(const uint64_t& in_signature_key) const;

            /* Whether the draw is pushed, by signature and view matrix: the rule classify_draw() picks must have is_pushed set (see
             * DrawRule::is_pushed). For live converters.
             */
            bool is_signature_pushed(const uint64_t&              in_signature_key,
                                     const std::array<float, 16>& in_view_matrix) const;

            /* Returns the matrix a draw's camera starts with for the eye (see ViewAnchorShift), column-major as OpenGL. The separation is
             * in_eye_separation, capped by the scene (see SceneSeparation). Converges at the scene distance (see SceneConvergence), else
             * the recipe's; with neither the matrix is only the translation.
             */
            std::array<float, 16> make_view_anchor_matrix(const Analysis::Recipe::Eye& in_eye,
                                                          const float&                 in_eye_separation,
                                                          const uint64_t&              in_shape_key) const;


            /* Applies the push toward the viewer (see Comfort::overlay_parallax) for the eye. Only for a projection rule (eye convergence)
             * on a perspective matrix: the picture moves sideways, right for the left eye. Otherwise returns false, matrix unchanged.
             */
            static bool apply_overlay_shift(const Analysis::Recipe::MatrixRule& in_rule,
                                            const Analysis::Recipe::Eye&        in_eye,
                                            const float&                        in_overlay_parallax,
                                            std::array<float, 16>*              inout_matrix_ptr);

            /* Applies the rule for the eye to the matrix (16 numbers, row by row). Eye separation and convergence distance are in matrix
             * units; an eye convergence changes nothing if the distance is not above zero. Returns false, matrix unchanged, if the rule
             * is about another kind of matrix.
             */
            static bool apply_rule(const Analysis::Recipe::MatrixRule& in_rule,
                                   const Analysis::Recipe::Eye&        in_eye,
                                   const float&                        in_eye_separation,
                                   std::array<float, 16>*              inout_matrix_ptr,
                                   const float&                        in_convergence_distance = 0.0f);

        private:
            /* Maps a signature to the positions of its rules in the draw rules of the recipe. */
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
