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

            /* Classifies a draw by the signature it has when it is issued (see Recipe::draw_rule_vec) and the view matrix in effect at that time
             * (16 numbers, row by row). The first rule for the signature that applies decides. A draw that no rule applies to is affected. This
             * is for a converter that plays an application while it runs. A replay knows the passes, so it calls is_draw_affected() and
             * is_draw_skipped() instead.
             */
            Analysis::Recipe::DrawClass classify_draw(const uint64_t&              in_signature_key,
                                                      const std::array<float, 16>& in_view_matrix) const;

            /* Decides what is done to a draw: whether the rules apply to it, it is left alone, or it is not played, and whether it is pushed toward
             * the viewer. A replay and a converter that plays an application while it runs both call this, so that they decide alike.
             *
             * With a pass (DrawQuery::has_pass) the draw is named by its pass and its structure (see is_draw_skipped(), is_draw_affected() and
             * is_draw_pushed()). Without one it is classified by its signature and the view matrix (see classify_draw() and
             * is_signature_pushed()). In both cases the cover key of the draw then applies: a baked flat layer, and a draw that the recipe pushes
             * by its cover key, are left alone instead of affected, and a draw pushed by its cover key is pushed. A caller whose way of playing
             * cannot leave a draw alone (a replay applies the shift of an eye where a camera begins, so a draw that shares its camera with an
             * affected draw has to be affected) sets DrawQuery::is_cover_hold_waived for it. The waiver does not apply to a baked flat layer.
             */
            Analysis::Recipe::DrawDecision decide_draw(const Analysis::Recipe::DrawQuery& in_query) const;

            /* Returns the distance at which the eyes converge for the draws with this signature (see Recipe::convergence_vec), or 0 if the recipe
             * gives none.
             */
            float get_convergence_distance(const uint64_t& in_signature_key) const;

            /* Returns the rules about the call, in the order they apply. The kind of matrix is checked later, by apply_rule(). The pointers stay
             * valid as long as the applier exists.
             */
            std::vector<const Analysis::Recipe::MatrixRule*> get_matching_rule_ptr_vec(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                       const CallPlace&                             in_place) const;

            /* Whether the recipe serves the single-pixel reads that the application makes in a run from one picture that is read once (see
             * Recipe::caches_pixel_reads).
             */
            bool get_caches_pixel_reads() const;

            /* Returns the comfort settings: what the recipe author says about how much the eyes can be asked to do. */
            const Comfort& get_comfort() const;

            /* Computes where the eyes converge and how far apart they may be for a scene of the given depth, so that no surface is further apart in
             * the two pictures than Comfort allows. The depths are those of the nearest and the farthest surface and the median of all surfaces, in
             * the units of the camera of the application. The projection scale is the x scale of the projection: 1 over the tangent of half the
             * angle of view across the picture width. What the recipe says about the scene (a SceneComfort, or nullptr for none) wins. With both
             * limits and Comfort::balances_convergence, the eyes converge where the whole depth fits them best:
             * 1 / d = 1 / near - (1 / near - 1 / far) * near limit / (near limit + far limit).
             */
            static ComfortLimits get_comfort_limits(const Comfort&      in_comfort,
                                                    const SceneComfort* in_scene_ptr,
                                                    const float&        in_median_depth,
                                                    const float&        in_near_depth,
                                                    const float&        in_far_depth,
                                                    const float&        in_projection_scale);

            /* Returns how far toward the viewer a draw with this cover key is pushed, if it is pushed. This is the part parallax for a part of an
             * object that protrudes (see DrawSelector::part_cover_key_vec) if the recipe sets one, and the overlay parallax otherwise.
             */
            float get_cover_push_parallax(const uint64_t& in_cover_key) const;

            /* Returns the class that the names of a draw and of its pass give it, and nothing else: SKIPPED if the recipe skips it (see
             * is_draw_skipped()), AFFECTED if the recipe says that the rules apply to it (see is_draw_affected()), UNAFFECTED otherwise.
             * decide_draw() adds the cover key of the draw to this.
             */
            Analysis::Recipe::DrawClass get_named_draw_class(const uint64_t& in_pass_key,
                                                             const uint64_t& in_draw_key) const;

            /* Returns how far toward the viewer the draws that the rules leave alone are pushed (see Comfort::overlay_parallax). 0 means no push. */
            float get_overlay_parallax() const;

            /* Returns what the recipe says about the scene (see Comfort::scene_override_vec), or nullptr if it says nothing. */
            const SceneComfort* find_scene_comfort_ptr(const uint64_t& in_shape_key) const;

            /* Returns the distance at which the eyes converge in the frames of the shape (see Recipe::scene_convergence_vec), or 0 if the recipe
             * gives none.
             */
            float get_scene_convergence_distance(const uint64_t& in_shape_key) const;

            /* Returns how far apart the eyes may be in the frames of the shape (see Recipe::scene_separation_vec), or 0 if the recipe gives no limit. */
            float get_scene_max_separation(const uint64_t& in_shape_key) const;

            /* Returns how vertices that no matrix moves get their parallax (see ScreenParallaxSettings). It is not enabled if the recipe does not have the
             * section.
             */
            const Analysis::Recipe::ScreenParallaxSettings& get_screen_parallax() const;

            Analysis::Recipe::UnknownFramePolicy get_unknown_frame_policy() const;

            /* Returns the view anchor shift of the recipe (see ViewAnchorShift). It is not enabled if the recipe does not have the section. */
            const Analysis::Recipe::ViewAnchorShift& get_view_anchor_shift() const;

            /* Returns false if no rule is about the function, whatever its arguments. It is a quick check to run before looking at a call. */
            bool has_rule_for(const APIInterceptor::APIFunction& in_function) const;

            /* Whether the recipe has a view anchor shift (see ViewAnchorShift): the camera of a draw is moved by the call that begins it. */
            bool has_view_anchor_shift() const;

            /* Whether the rules apply to a draw (see Recipe::affected). The most specific name wins: the draw in its pass counts for more than the key
             * of the draw, which counts for more than the key of its pass. A draw that none of them names is affected.
             */
            bool is_draw_affected(const uint64_t& in_pass_key,
                                  const uint64_t& in_draw_key) const;

            /* Whether the draw is an automatic flat layer, left as the application makes it (see Recipe::flat_layer_draw_key_vec). */
            bool is_cover_flat_layer(const uint64_t& in_cover_key) const;

            /* Whether the draw is pushed toward the viewer by its key (DrawSelector::cover_key_vec, or Recipe::overlay_draw_key_vec). */
            bool is_cover_pushed(const uint64_t& in_cover_key) const;

            /* Whether a draw of the key (its signature and size; see MatchKeyBuilder::compute_draw_cover_key()) covers the whole of the view, as a
             * flash does; see Recipe::covering_draw_key_vec.
             */
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

            /* Whether a draw with this key (see MatchKeyBuilder::compute_draw_cover_key()) is a part of an object that protrudes from it (see
             * DrawSelector::part_cover_key_vec).
             */
            bool is_part_cover(const uint64_t& in_cover_key) const;

            /* True if the recipe says that each eye has a copy of the texture (see PerEyeTexture). */
            bool is_per_eye_texture(const Analysis::APIDumpAnalysis::ResourceIdentity& in_identity) const;

            /* Whether the draw is a flat overlay that spans from the left edge to the right edge. Pushing it widens its viewport so that no edge
             * stays bare.
             */
            bool is_side_reaching(const uint64_t& in_cover_key) const;

            /* True if the recipe knows the signature of a draw: the analysis that the recipe was baked from saw it, or the recipe lists no
             * signatures. A draw with an unknown signature is affected, like any draw that no rule applies to, but the recipe was not made for it.
             */
            bool is_signature_known(const uint64_t& in_signature_key) const;

            /* Whether the draw is pushed toward the viewer, decided by its signature and the view matrix. The rule that classify_draw() picks must
             * have is_pushed set (see DrawRule::is_pushed). This is for a converter that plays an application while it runs.
             */
            bool is_signature_pushed(const uint64_t&              in_signature_key,
                                     const std::array<float, 16>& in_view_matrix) const;

            /* Returns the matrix that the camera of a draw starts with for the eye (see ViewAnchorShift). It is stored the way OpenGL stores
             * matrices, column after column. The eyes are in_eye_separation apart, but no more than the scene of the shape allows (see
             * SceneSeparation). They converge at the distance of the scene (see SceneConvergence) or, if there is none, at the distance of the
             * recipe. With neither distance, the matrix is only the translation.
             */
            std::array<float, 16> make_view_anchor_matrix(const Analysis::Recipe::Eye& in_eye,
                                                          const float&                 in_eye_separation,
                                                          const uint64_t&              in_shape_key) const;


            /* Applies the push of a draw toward the viewer (see Comfort::overlay_parallax) for the eye. If the rule is a projection rule (an
             * eye convergence) and the matrix is a perspective matrix, the picture of the eye is moved sideways by the parallax, to the right for
             * the left eye. Returns false, and leaves the matrix unchanged, for any other rule or matrix.
             */
            static bool apply_overlay_shift(const Analysis::Recipe::MatrixRule& in_rule,
                                            const Analysis::Recipe::Eye&        in_eye,
                                            const float&                        in_overlay_parallax,
                                            std::array<float, 16>*              inout_matrix_ptr);

            /* Applies the rule for the eye to the matrix (16 numbers, row by row). The eye separation is the distance between the eyes, in the
             * units of the matrix. An eye offset and an eye convergence use it. The convergence distance, in the same units, is where the eyes
             * converge. An eye convergence uses it and changes nothing if it is not greater than zero. Returns false, and leaves the matrix
             * unchanged, if the rule is about another kind of matrix.
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
