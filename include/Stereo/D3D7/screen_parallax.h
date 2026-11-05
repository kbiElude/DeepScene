/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_D3D7_SCREEN_PARALLAX_H
#define STEREO_D3D7_SCREEN_PARALLAX_H

#include "Analysis/RecipeLoader/types.h"
#include <stddef.h>
#include <stdint.h>

namespace Stereo
{
    namespace D3D7
    {
        /* Gives the vertices of a Direct3D 7 draw parallax for an eye, when the vertices are already in the space of the picture (the
         * position of a vertex is in pixels, with the reciprocal of its depth: no matrix moves it, so the matrix of the eye cannot). See
         * Analysis::RecipeLoader::ScreenParallax for what is done to a vertex, and why. This is what is done to the bytes of the vertices,
         * whoever holds them: a dump that is replayed, or an application that runs.
         */
        class ScreenParallax
        {
        public:
            /* What the shift of a vertex depends on. */
            struct Parameters
            {
                float                       convergence_distance  = 1.0f;  /* in the units of the camera of the application     */
                float                       eye_separation        = 0.0f;  /* the same units                                    */
                Analysis::RecipeLoader::Eye eye                   = Analysis::RecipeLoader::Eye::LEFT;
                float                       field_of_view_degrees = 90.0f; /* of the width of the picture                       */
                float                       picture_width         = 0.0f;  /* in pixels: of what the vertices are positioned in */
            };

            /* What is the same for every vertex of a draw, worked out once (the focal length takes a tangent). */
            struct ShiftFactors
            {
                float convergence_reciprocal = 1.0f;
                bool  is_left                = true;
                float magnitude_per_reciprocal = 0.0f; /* pixels for each unit of the reciprocal of the depth that is more than the convergence */
                float max_reciprocal         = 1.0f;
            };

            /* How far a vertex of the reciprocal of the depth is moved sideways for the eye, in pixels: positive to the right. */
            static float get_shift(const Parameters& in_parameters,
                                   const float&      in_reciprocal_depth);
            static float get_shift(const ShiftFactors& in_factors,
                                   const float&        in_reciprocal_depth);

            /* Whether the parameters are ones a shift can be made with. */
            static bool is_valid(const Parameters& in_parameters);

            static ShiftFactors make_shift_factors(const Parameters& in_parameters);

            /* Scales the position of every vertex of the array, in pixels, by the scale: the x and the y, and not the depth or the reciprocal of it. For
             * a draw to a surface that is rendered larger than the application asks.
             */
            static void scale_vertices(const float&    in_scale,
                                       const uint32_t& in_vertex_n_bytes,
                                       const size_t&   in_n_vertices,
                                       uint8_t*        inout_vertex_ptr);

            /* Moves the x of every vertex of the array, which is of pre-transformed vertices (x, y, z and the reciprocal of the depth, in
             * floats, and then what the format has more): to the right by the shift for the left eye, to the left for the right one. The
             * depth is not allowed to be behind the camera or beyond reason: a vertex which is has the depth at the edge of what is allowed.
             */
            static void shift_vertices(const Parameters& in_parameters,
                                       const uint32_t&   in_vertex_n_bytes,
                                       const size_t&     in_n_vertices,
                                       uint8_t*          inout_vertex_ptr);

            /* The same, with every vertex moved by the same shift, in pixels (positive to the right): for a draw which is pushed toward the
             * viewer as a whole, whatever its depth (an interface).
             */
            static void shift_vertices_by(const float&    in_shift,
                                          const uint32_t& in_vertex_n_bytes,
                                          const size_t&   in_n_vertices,
                                          uint8_t*        inout_vertex_ptr);
        };
    }
}

#endif /* STEREO_D3D7_SCREEN_PARALLAX_H */
