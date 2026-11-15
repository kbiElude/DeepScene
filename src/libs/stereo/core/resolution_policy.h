/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_RESOLUTION_POLICY_H
#define STEREO_CORE_RESOLUTION_POLICY_H

#include <stdint.h>
#include "stereo/core/types.h"

namespace Stereo
{
    namespace Core
    {
        /* Defines how much larger than the application asks the converter renders, in each direction. The application is not told. It keeps
         * using the sizes it knows. The adapter of its API creates its resources that many times larger and adjusts everything the
         * application states in pixels (a viewport, for example) to match. This class works out the sizes for any API.
         */
        class ResolutionPolicy
        {
        public:
            /* A scale that is not a number or is below the minimum becomes the minimum. A scale above the maximum becomes the maximum. */
            explicit ResolutionPolicy(const float& in_scale);

            float get_scale() const;

            /* True if nothing is made larger. */
            bool is_identity() const;

            /* Returns the size in pixels that an extent requested by the application becomes, rounded to the nearest pixel. */
            uint32_t scale_extent(const uint32_t& in_extent) const;

            /* Returns the pixel of the enlarged picture to read for a single pixel that the application names. It is the pixel under the center
             * of the application's pixel, not an average of the pixels that the application's pixel covers. A coordinate outside the
             * application's picture (below 0, or past its last pixel) is first moved to the nearest pixel that the application has. The result
             * is also kept within the enlarged extent, which the rounding of the center could otherwise exceed. Returns 0 if either extent is 0.
             */
            uint32_t scale_pixel_coordinate(const int64_t&  in_coordinate,
                                            const uint32_t& in_asked_extent,
                                            const uint32_t& in_real_extent) const;

            /* Returns the rectangle that a rectangle requested by the application becomes. Each of its four numbers is rounded to the nearest
             * pixel, as scale_extent() does. The overload with bounds also keeps the result inside the bounds (the size of the target the
             * rectangle is for), which the rounding of the numbers could otherwise exceed.
             */
            PixelRect scale_rect(const PixelRect& in_rect) const;
            PixelRect scale_rect(const PixelRect& in_rect,
                                 const uint32_t&  in_bounds_width,
                                 const uint32_t&  in_bounds_height) const;


            /* Returns the scale that makes an image of the requested size as large as possible inside a target while keeping its proportions.
             * It is the smaller of the scale that the width allows and the scale that the height allows. Returns 1 if the target or the image
             * has no size. The result is not limited to the minimum and maximum scale of a policy.
             */
            static float get_fit_scale(const uint32_t& in_target_width,
                                       const uint32_t& in_target_height,
                                       const uint32_t& in_width,
                                       const uint32_t& in_height);

            static float get_max_scale();
            static float get_min_scale();

        private:
            float m_scale;
        };
    }
}

#endif /* STEREO_CORE_RESOLUTION_POLICY_H */
