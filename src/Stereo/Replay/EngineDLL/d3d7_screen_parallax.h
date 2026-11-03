/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_D3D7_SCREEN_PARALLAX_H
#define STEREO_REPLAY_ENGINEDLL_D3D7_SCREEN_PARALLAX_H

#include "Analysis/RecipeLoader/types.h"
#include "Capture/APIDumpLoader/APIDumpLoader.h"
#include "overlay_chunk_source.h"
#include <functional>
#include <stddef.h>
#include <stdint.h>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Gives the vertices of a Direct3D 7 draw parallax for an eye, when the vertices are already in the space of the picture (the position of
             * a vertex is in pixels, with the reciprocal of its depth: no matrix moves it, so the matrix of the eye cannot). See
             * Analysis::RecipeLoader::ScreenParallax for what is done to a vertex, and why.
             */
            class D3D7ScreenParallax
            {
            public:
                /* What the shift of a vertex depends on. */
                struct Parameters
                {
                    float                       convergence_distance  = 1.0f; /* in the units of the camera of the application              */
                    float                       eye_separation        = 0.0f; /* the same units                                             */
                    Analysis::RecipeLoader::Eye eye                   = Analysis::RecipeLoader::Eye::LEFT;
                    float                       field_of_view_degrees = 90.0f; /* of the width of the picture                               */
                    float                       picture_width         = 0.0f;  /* in pixels: of what the vertices are positioned in         */
                };

                /* Returns false if the call is not a draw of pre-transformed vertices which this changes (a draw of another kind, vertices which a matrix
                 * moves, a vertex buffer, or data which does not fit what the call says). Otherwise the call is copied to the output with the vertices
                 * pointing to a chunk of the overlay, which holds them moved.
                 */
                static bool rewrite(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                    const IChunkSource&                          in_chunk_source,
                                    const Parameters&                            in_parameters,
                                    OverlayChunkSource*                          inout_overlay_ptr,
                                    Capture::APIDumpLoader::DumpedAPICall*       out_call_ptr);

                /* The same, with every vertex moved by the same shift, in pixels (positive to the right): for a draw which is pushed toward the viewer as
                 * a whole, whatever its depth (an interface). Returns false for a shift of 0, too.
                 */
                static bool rewrite_by(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                       const IChunkSource&                          in_chunk_source,
                                       const float&                                 in_shift,
                                       OverlayChunkSource*                          inout_overlay_ptr,
                                       Capture::APIDumpLoader::DumpedAPICall*       out_call_ptr);

                /* Moves the x of every vertex of the array, which is of pre-transformed vertices (x, y, z and the reciprocal of the depth, in floats, and
                 * then what the format has more): to the right by the shift for the left eye, to the left for the right one. The depth is not allowed
                 * to be behind the camera or beyond reason: a vertex which is has the depth at the edge of what is allowed.
                 */
                static void shift_vertices(const Parameters& in_parameters,
                                           const uint32_t&   in_vertex_n_bytes,
                                           const size_t&     in_n_vertices,
                                           uint8_t*          inout_vertex_ptr);

                /* How far a vertex of the reciprocal of the depth is moved sideways for the eye, in pixels: positive to the right. */
                static float get_shift(const Parameters& in_parameters,
                                       const float&      in_reciprocal_depth);

            private:
                /* What is done to the vertices of a draw: their size in bytes, how many of them there are, and where they are. */
                typedef std::function<void(const uint32_t& in_vertex_n_bytes,
                                           const size_t&   in_n_vertices,
                                           uint8_t*        inout_vertex_ptr)> ShiftFunc;

                /* What rewrite() and rewrite_by() are, but for how the vertices are moved. */
                static bool rewrite_vertices(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                             const IChunkSource&                          in_chunk_source,
                                             const ShiftFunc&                             in_shift_func,
                                             OverlayChunkSource*                          inout_overlay_ptr,
                                             Capture::APIDumpLoader::DumpedAPICall*       out_call_ptr);
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_D3D7_SCREEN_PARALLAX_H */
