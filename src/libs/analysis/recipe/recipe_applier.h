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
#include "Capture/APIDumpLoader/APIDumpLoader.h"
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

            /* How far toward the viewer the draws that the rules leave alone are pushed (see Comfort::overlay_parallax); 0: not. */
            float get_overlay_parallax() const;

            /* The distance at which the eyes converge in the frames of the shape (see Recipe::scene_convergence_vec); 0 if the recipe says
             * none.
             */
            float get_scene_convergence_distance(const uint64_t& in_shape_key) const;

            /* How far apart the eyes may be in the frames of the shape (see Recipe::scene_separation_vec); 0 if the recipe says none. */
            float get_scene_max_separation(const uint64_t& in_shape_key) const;

            /* How vertices that no matrix moves are given parallax (see ScreenParallax); not enabled if the recipe does not say. */
            const Analysis::Recipe::ScreenParallax& get_screen_parallax() const;

            Analysis::Recipe::UnknownFramePolicy get_unknown_frame_policy() const;

            /* False if no rule is about the function, whatever the arguments: a quick check before the call is looked at. */
            bool has_rule_for(const APIInterceptor::APIFunction& in_function) const;

            /* Whether the rules apply to a draw (see Recipe::affected): the draw in its pass counts for more than the key of the draw, which counts for more
             * than the key of its pass, and a draw which none of them names is affected.
             */
            bool is_draw_affected(const uint64_t& in_pass_key,
                                  const uint64_t& in_draw_key) const;

            /* Whether the draw is not to be played (see Recipe::skipped): the draw in its pass, its key, or the key of its pass is named. */
            bool is_draw_skipped(const uint64_t& in_pass_key,
                                 const uint64_t& in_draw_key) const;

            /* True if the frame is one the recipe was made for: its shape is listed, or no shape is. */
            bool is_frame_known(const uint64_t& in_shape_key) const;

            /* True if the recipe says that each eye has a copy of the texture (see PerEyeTexture). */
            bool is_per_eye_texture(const Analysis::APIDumpAnalysis::ResourceIdentity& in_identity) const;

            /* True if the recipe knows the signature of a draw: the analysis it was baked from saw it, or the recipe lists none. A draw of
             * an unknown signature is affected, like any draw which no rule applies to, but the recipe was not made for it.
             */
            /* Whether a draw of the key (its signature and size; see KeyBuilder::compute_draw_cover_key()) covers the whole of the view, as a
             * flash does; see Recipe::covering_draw_key_vec.
             */
            bool is_draw_covering(const uint64_t& in_cover_key) const;

            bool is_signature_known(const uint64_t& in_signature_key) const;


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
            std::map<uint64_t, float>             m_convergence_distance_map;
            DrawRuleIndexMap                      m_draw_rule_index_map;
            std::set<uint64_t>                    m_covering_draw_key_set;
            std::set<uint64_t>                    m_known_signature_key_set;
            Analysis::Recipe::Recipe        m_recipe;
            std::set<APIInterceptor::APIFunction> m_rule_function_set;
            std::map<uint64_t, float>             m_scene_convergence_distance_map;
            std::map<uint64_t, float>             m_scene_max_separation_map;
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
