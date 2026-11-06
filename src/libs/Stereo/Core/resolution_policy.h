/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CORE_RESOLUTION_POLICY_H
#define STEREO_CORE_RESOLUTION_POLICY_H

#include <stdint.h>

namespace Stereo
{
    namespace Core
    {
        /* How much larger than the application asks a converter renders, in each direction: the application is told nothing, and goes on
         * with the sizes it knows; the adapter of its API makes what it creates that many times larger, and makes what it states in pixels
         * (a viewport) match. This is what the sizes are worked out by, whatever the API is.
         */
        class ResolutionPolicy
        {
        public:
            /* A scale which is not a number, or less than the least, is the least; more than the most is the most. */
            explicit ResolutionPolicy(const float& in_scale);

            float get_scale() const;

            /* True if nothing is made larger. */
            bool is_identity() const;

            /* The size in pixels that an extent which the application asks for is made, to the nearest pixel. */
            uint32_t scale_extent(const uint32_t& in_extent) const;


            /* The scale that makes an image of the size the application asks for as large as it can be in a target, with its proportions: the
             * least of what the width and the height allow. 1 if the target or the image has no size. Not brought inside the limits of a policy.
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
