/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_DEPTH_BUFFER_STATISTICS_H
#define STEREO_CORE_DEPTH_BUFFER_STATISTICS_H

#include "stereo/core/types.h"
#include <stddef.h>

namespace Stereo
{
    namespace Core
    {
        /* How deep the scene drawn by a frame (or by one pass of it) is. The values are distances from the application's camera, in its units,
         * taken from the visible surfaces in the depth buffer.
         */
        struct DepthRange
        {
            float far_depth    = 0.0f;
            bool  has_depth    = false; /* false if too little was drawn, or far_depth is not above near_depth */
            float median_depth = 0.0f;
            float near_depth   = 0.0f;
        };

        /* Measures how deep a scene is from the depth buffer of a frame. */
        class DepthBufferStatistics
        {
        public:
            /* The distance from the camera of a depth buffer value, for the given projection (the same two values that measure() takes).
             * Returns 0 if the projection gives no valid distance.
             */
            static float get_distance(const float& in_depth,
                                      const float& in_projection_z_scale,
                                      const float& in_projection_z_shift);

            /* Decides what measure() looks for among the pixels that were drawn to. in_n_looked_at pixels were examined, in_n_drawn of them
             * were drawn to and in_n_sky were not. The nearest and the farthest in_fraction of the drawn pixels are the edges of the scene.
             * The result has is_enough set to false if too little was drawn to measure anything.
             */
            static DepthSelection get_selection(const size_t& in_n_drawn,
                                                const size_t& in_n_looked_at,
                                                const size_t& in_n_sky,
                                                const float&  in_projection_z_scale,
                                                const float&  in_projection_z_shift,
                                                const float&  in_fraction);

            /* The depths are OpenGL depth buffer values (0 at the near plane, 1 at the far plane) of a perspective projection. The projection
             * enters through its z scale and z shift, which are elements 10 and 14 of an OpenGL projection matrix. A pixel at the far end of the
             * buffer was not drawn to. It is sky, and it counts as being at the far plane if the sky covers at least in_fraction of the examined
             * pixels. The nearest surfaces are the first in_fraction of the drawn pixels, and the farthest surfaces are the last in_fraction
             * (0.001 means a thousandth). The median is the median of the drawn pixels. Only every in_step-th depth is examined.
             */
            static DepthRange measure(const float*  in_depth_ptr,
                                      const size_t& in_n_depths,
                                      const size_t& in_step,
                                      const float&  in_projection_z_scale,
                                      const float&  in_projection_z_shift,
                                      const float&  in_fraction);

            /* A depth at or above this value was not drawn to (the buffer is cleared to 1). */
            static const float NOT_DRAWN_DEPTH;
        };
    }
}

#endif /* STEREO_CORE_DEPTH_BUFFER_STATISTICS_H */
