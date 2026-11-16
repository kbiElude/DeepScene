/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_SCREEN_SPACE_VERTEX_SHIFTER_H
#define STEREO_CORE_SCREEN_SPACE_VERTEX_SHIFTER_H

#include "analysis/recipe/types.h"
#include "stereo/core/screen_parallax_planner.h"
#include <stddef.h>
#include <stdint.h>

namespace Stereo
{
    namespace Core
    {
        /* Gives the vertices of a draw parallax for one eye, when the vertices are already in the space of the picture. In that space a
         * vertex position is in pixels and comes with the reciprocal of its depth. No matrix moves such a vertex, so the matrix of the eye
         * cannot be used. See Analysis::Recipe::ScreenParallaxSettings for what is done to a vertex, and why. This class works on the bytes of the
         * vertices, whoever holds them: a dump that is replayed, or an application that runs.
         */
        class ScreenSpaceVertexShifter
        {
        public:
            /* Everything that the shift of a vertex depends on. */
            struct Parameters
            {
                float                 convergence_distance  = 1.0f;  /* in the application's camera units                                  */
                float                 eye_separation        = 0.0f;  /* in the same units                                                  */
                Analysis::Recipe::Eye eye                   = Analysis::Recipe::Eye::LEFT;
                float                 field_of_view_degrees = 90.0f; /* horizontal, across the picture width                               */
                float                 picture_width         = 0.0f;  /* in pixels: the width of the picture the vertices are positioned in */
            };

            /* The values that are the same for every vertex of a draw. They are computed once because the focal length needs a tangent. */
            struct ShiftFactors
            {
                float convergence_reciprocal = 1.0f;
                bool  is_left                = true;
                float magnitude_per_reciprocal = 0.0f; /* pixels of shift per unit of reciprocal depth above the convergence reciprocal */
                float max_reciprocal         = 1.0f;
            };

            /* Moves the vertices as in_plan says: each by its depth, or all by one shift. Returns true if the vertices were moved. */
            static bool apply_plan(const ScreenShiftPlan& in_plan,
                                   const uint32_t&        in_vertex_n_bytes,
                                   const size_t&          in_n_vertices,
                                   uint8_t*               inout_vertex_ptr);

            /* Returns how far a vertex with the given reciprocal depth is moved sideways for the eye, in pixels. Positive is to the right. */
            static float get_shift(const Parameters&   in_parameters,
                                   const float&        in_reciprocal_depth);
            static float get_shift(const ShiftFactors& in_factors,
                                   const float&        in_reciprocal_depth);

            /* Returns true if a shift can be computed from the parameters. */
            static bool is_valid(const Parameters& in_parameters);

            /* Returns the parameters of a PER_VERTEX plan. */
            static Parameters make_parameters(const ScreenShiftPlan& in_plan);

            static ShiftFactors make_shift_factors(const Parameters& in_parameters);

            /* Returns true if the vertices that a draw uses reach both sides of the picture: from its left edge to its right edge, to within a
             * small fraction of its width. The 16-bit indices select which vertices of the array are used. If in_index_ptr is null, all
             * in_n_vertices vertices are used. A draw of a part of a vertex buffer that other draws also use is judged by the vertices it
             * uses, not by the whole buffer. Returns false if no vertex is used.
             */
            static bool reaches_both_sides(const uint8_t*  in_vertex_ptr,
                                           const uint32_t& in_vertex_n_bytes,
                                           const size_t&   in_n_vertices,
                                           const uint16_t* in_index_ptr,
                                           const size_t&   in_n_indices,
                                           const float&    in_picture_width);

            /* Multiplies the position of every vertex of the array (x and y, in pixels) by in_scale. The depth and its reciprocal are not
             * changed. It is used for a draw to a surface that is rendered larger than the application asks.
             */
            static void scale_vertices(const float&    in_scale,
                                       const uint32_t& in_vertex_n_bytes,
                                       const size_t&   in_n_vertices,
                                       uint8_t*        inout_vertex_ptr);

            /* Moves the x of every vertex of the array. The vertices are pre-transformed: x, y, z and the reciprocal of the depth as floats,
             * followed by whatever else the vertex format holds. The shift is to the right for the left eye and to the left for the right eye.
             * The depth is limited to a valid range: a vertex behind the camera or at an unreasonable depth gets the depth at the edge of
             * that range. A vertex at the left edge of the picture is not moved to the right, and a vertex at the right edge is not moved to
             * the left, because that would open a gap between the picture and what it covered.
             */
            static void shift_vertices(const Parameters& in_parameters,
                                       const uint32_t&   in_vertex_n_bytes,
                                       const size_t&     in_n_vertices,
                                       uint8_t*          inout_vertex_ptr);

            /* The same, but every vertex moves by the same shift in pixels (positive to the right). It is used for a draw that is pushed
             * toward the viewer as a whole, whatever its depth (a user interface).
             */
            static void shift_vertices_by(const float&    in_shift,
                                          const uint32_t& in_vertex_n_bytes,
                                          const size_t&   in_n_vertices,
                                          uint8_t*        inout_vertex_ptr);
        };
    }
}

#endif /* STEREO_CORE_SCREEN_SPACE_VERTEX_SHIFTER_H */
