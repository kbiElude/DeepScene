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
        /* How large the image of an eye can be: the two eyes are passed on side by side, in the image of the frame channel, which has a size of
         * its own. What a converter makes the surfaces as large as (see ResolutionPolicy) is within it.
         */
        class EyeImageLimits
        {
        public:
            /* The scale which makes an image of the size the application asks for as large as the frame can hold both eyes of. */
            static float get_largest_scale(const uint32_t& in_width,
                                           const uint32_t& in_height);

            static uint32_t get_max_height();
            static uint32_t get_max_width ();

            /* Whether both eyes, each of the size, are within what the frame can hold. */
            static bool fits(const uint32_t& in_width,
                             const uint32_t& in_height);

        private:
            static constexpr uint32_t N_EYES = 2;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_EYE_IMAGE_LIMITS_H */
