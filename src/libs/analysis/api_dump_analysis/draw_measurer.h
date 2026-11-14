/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What a draw looks like through its camera: how deep the origin of its object is, how deep its vertices are (the surfaces), how wide the picture is for the
 * projection that it is made with, whether it reaches the edges of the view, how it blends and where its vertices are. The model builder says what the draw is
 * made with (the constants, the viewport, the resources that a copy of the color went to) and where the results go (a pass, the facts of the draw structure);
 * nothing is kept here.
 */
#ifndef APIDUMPANALYSIS_DRAW_MEASURER_H
#define APIDUMPANALYSIS_DRAW_MEASURER_H

#include "analysis/api_dump_analysis/types.h"
#include <array>
#include <set>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class DrawMeasurer
        {
        public:
            /* The matrices of the draw, if its camera has depth to converge at (a view, and a perspective projection). */
            static bool find_draw_matrices(const ConstantStateMap& in_constant_state_map,
                                           DrawMatrices*           out_matrices_ptr);

            /* Whether the projection divides by the depth: w comes from z (1 or -1 by handedness), with no w translation. Neither orthographic nor
             * identity.
             */
            static bool is_perspective_projection(const std::array<float, 16>& in_projection_f32mat);

            /* Counts the draw at the depth of the origin of its object, as the camera of the pass sees it. */
            static void record_draw_depth(const DrawDescription&  in_draw,
                                          const ConstantStateMap& in_constant_state_map,
                                          PassDepth*              inout_depth_ptr);

            /* Adds the draw to the facts of its structure (see DrawFacts): how it blends, whether it samples a copy of the picture, and where its vertices are. Without
             * history, only what does not need the vertices is counted.
             */
            static void record_draw_facts(const DrawDescription&      in_draw,
                                          const std::set<ResourceId>& in_color_copy_target_set,
                                          const bool&                 in_keeps_history,
                                          const ConstantStateMap&     in_constant_state_map,
                                          const Viewport&             in_viewport,
                                          bool*                       inout_has_seen_3d_draw_ptr,
                                          DrawFacts*                  inout_facts_ptr);

            /* Counts the draw by the depth of the vertices of its sample (see DrawDescription::geometry_ptr), those in front of the camera and in view, each for its
             * share of the vertices of the draw. Returns what the vertices, as far as they were looked at, say of the view: they reach all four edges, or they do not,
             * or there was nothing to look at.
             */
            static ViewCoverage record_draw_surface_depth(const DrawDescription&  in_draw,
                                                          const ConstantStateMap& in_constant_state_map,
                                                          PassDepth*              inout_depth_ptr);

            /* The same, for a draw whose vertices are already in the space of the picture (see GeometrySample::is_pre_transformed): the depth of a vertex is the
             * reciprocal of what it says, it is in view when it is in the viewport, and the draw reaches the edges of the view when its vertices reach those of the
             * viewport. No matrix is asked.
             */
            static ViewCoverage record_screen_depth(const DrawDescription& in_draw,
                                                    const Viewport&        in_viewport,
                                                    PassDepth*             inout_depth_ptr);

        private:
            /* The first three columns of the model matrix times the view matrix (rows of four, as the matrices are kept): what a vertex needs to be given its place in the
             * space of the camera.
             */
            static std::array<float, 12> compute_model_view(const DrawMatrices& in_matrices);
        };
    }
}

#endif /* APIDUMPANALYSIS_DRAW_MEASURER_H */
