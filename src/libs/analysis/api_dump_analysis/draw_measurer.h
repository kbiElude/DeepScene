/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Measures a draw as seen through its camera: the depth of the object origin, the depth of the vertices (the surfaces), the width of the picture
 * for the projection, whether the draw reaches the edges of the view, how it blends and where its vertices are. The model builder passes in what
 * the draw is made with (the constants, the viewport, the resources that received a copy of the color) and where the results go (a pass, the
 * draw facts). The class keeps no state.
 */
#ifndef APIDUMPANALYSIS_DRAW_MEASURER_H
#define APIDUMPANALYSIS_DRAW_MEASURER_H

#include "analysis/api_dump_analysis/types.h"
#include <array>
#include <map>
#include <set>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class DrawMeasurer
        {
        public:
            /* Finds the matrices of the draw. Returns true if the camera has a depth to converge at, which needs a view matrix and a projection that is not orthographic. */
            static bool find_draw_matrices(const ConstantStateMap& in_constant_state_map,
                                           DrawMatrices*           out_matrices_ptr);

            /* Returns true if the projection divides by the depth: w comes from z (1 or -1, depending on handedness) and has no translation.
             * An orthographic projection and the identity do not qualify.
             */
            static bool is_perspective_projection(const std::array<float, 16>& in_projection_f32mat);

            /* Counts the draw at the depth of its object origin, as seen by the camera of the pass. */
            static void record_draw_depth(const DrawDescription&  in_draw,
                                          const ConstantStateMap& in_constant_state_map,
                                          PassDepth*              inout_depth_ptr);

            /* Whether the draw samples a resource that a copy of the picture filled (see ModelBuilder: a copy of the color that is rendered to). Such a draw reads what is rendered below it,
             * as a bloom does.
             */
            static bool is_sampling_copied_color(const DrawDescription&      in_draw,
                                                 const std::set<ResourceId>& in_color_copy_target_set);

            /* Adds the draw to the draw facts (see DrawFacts): how it blends, whether it samples a copy of the picture, and where its vertices are.
             * Without history, only the facts that do not need the vertices are counted.
             */
            static void record_draw_facts(const DrawDescription&      in_draw,
                                          const std::set<ResourceId>& in_color_copy_target_set,
                                          const bool&                 in_keeps_history,
                                          const ConstantStateMap&     in_constant_state_map,
                                          const Viewport&             in_viewport,
                                          bool*                       inout_has_seen_3d_draw_ptr,
                                          DrawFacts*                  inout_facts_ptr);

            /* Counts the draw at the depth of each vertex of its geometry sample (see DrawDescription::geometry_ptr) that is in front of the camera and in view.
             * Each vertex carries its share of the draw vertex count. Returns how the sampled vertices cover the view: COVERING if they reach all four edges,
             * PARTIAL if they do not, UNKNOWN if there was nothing to measure.
             */
            static ViewCoverage record_draw_surface_depth(const DrawDescription&  in_draw,
                                                          const ConstantStateMap& in_constant_state_map,
                                                          PassDepth*              inout_depth_ptr);

            /* The same, for a draw whose vertices are already in the space of the picture (see GeometrySample::is_pre_transformed). The depth of a vertex is
             * the reciprocal of its third coordinate, a vertex is in view when it is inside the viewport, and the draw reaches the edges of the view when its
             * vertices reach the edges of the viewport. No matrices are used.
             */
            static ViewCoverage record_screen_depth(const DrawDescription& in_draw,
                                                    const Viewport&        in_viewport,
                                                    PassDepth*             inout_depth_ptr);

        private:
            /* Returns the first three columns of the model matrix times the view matrix (stored as rows of four, like the inputs). A vertex multiplied by
             * this matrix is in the space of the camera.
             */
            static std::array<float, 12> compute_model_view(const DrawMatrices& in_matrices);
        };
    }
}

#endif /* APIDUMPANALYSIS_DRAW_MEASURER_H */
