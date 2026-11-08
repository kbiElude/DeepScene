/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_DEPTH_BUFFER_STATISTICS_H
#define STEREO_CORE_DEPTH_BUFFER_STATISTICS_H

#include <stddef.h>

namespace Stereo
{
    namespace Core
    {
        /* How deep what a frame (or a pass of it) drew is, in the units of the camera of the application, as the surfaces that are visible in its depth buffer say. */
        struct DepthRange
        {
            float far_depth    = 0.0f;
            bool  has_depth    = false; /* false if nothing in the buffer was drawn (all of it is as deep as the buffer goes) */
            float median_depth = 0.0f;
            float near_depth   = 0.0f;
        };

        /* What the depth buffer of a frame says of how deep it is. */
        class DepthBufferStatistics
        {
        public:
            /* The depths are what an OpenGL depth buffer holds (0 at the near plane and 1 at the far one) of a perspective projection whose elements are the ones that give the depth
             * its meaning (its z scale and z shift: the elements 10 and 14 of a matrix of OpenGL). A pixel that is as deep as the buffer goes was not drawn to: it is the sky, which is as far as the far plane, if there are as many of
             * them as the fraction. The nearest surfaces are the first fraction of the pixels that were drawn, and the farthest the last (0.001 is a thousandth); the
             * median is that of the pixels that were drawn. Only every step-th depth is looked at.
             */
            static DepthRange measure(const float*  in_depth_ptr,
                                      const size_t& in_n_depths,
                                      const size_t& in_step,
                                      const float&  in_projection_z_scale,
                                      const float&  in_projection_z_shift,
                                      const float&  in_fraction);

            /* The distance from the camera of a depth of the buffer, for the projection (as measure() takes it); 0 if the projection says no such distance. */
            static float get_distance(const float& in_depth,
                                      const float& in_projection_z_scale,
                                      const float& in_projection_z_shift);
        };
    }
}

#endif /* STEREO_CORE_DEPTH_BUFFER_STATISTICS_H */
