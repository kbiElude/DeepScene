/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_EYE_IMAGE_LIMITS_H
#define STEREO_CONVERTERDLL_EYE_IMAGE_LIMITS_H

#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The largest image that one eye can have. The frame channel passes both eyes side by side in one image of a fixed maximum size, so each eye
         * gets at most half of the width. The surfaces that the converter creates (see ResolutionPolicy) must stay within this size.
         */
        class EyeImageLimits
        {
        public:
            /* Returns the scale that enlarges an image of the given size (what the application asks for) as far as both eyes still fit in the frame. */
            static float get_largest_scale(const uint32_t& in_width,
                                           const uint32_t& in_height);

            static uint32_t get_max_height();
            static uint32_t get_max_width ();

            /* Returns true if two images of the given size, one per eye, fit in the frame. */
            static bool fits(const uint32_t& in_width,
                             const uint32_t& in_height);

        private:
            static constexpr uint32_t N_EYES = 2;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_EYE_IMAGE_LIMITS_H */
