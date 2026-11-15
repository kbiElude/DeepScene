/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_RENDER_SCALE_PLANNER_H
#define STEREO_CONVERTERDLL_RENDER_SCALE_PLANNER_H

#include <stdint.h>
#include <string>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The code that the API adapters share to decide how much larger than requested an application renders (see ResolutionPolicy) and to report it.
         * The scale is the largest one that fits the images in an eye of the headset, while staying within what the frame channel holds for both eyes
         * and the largest surface that the API can create.
         */
        class RenderScalePlanner
        {
        public:
            /* Returns the scale for an application that asks for images of the asked size, shown in a target of the target size (an eye of the headset).
             * It is the smallest of three scales: the one that fits the images in the target with their proportions, the one that fits the channel
             * (see EyeImageLimits), and the one that fits the largest extent that a surface of the API can have (in_max_surface_extent, 0: no limit).
             * It is not clamped to the limits of a ResolutionPolicy. The policy does that.
             */
            static float choose_scale(const uint32_t& in_target_width,
                                      const uint32_t& in_target_height,
                                      const uint32_t& in_asked_width,
                                      const uint32_t& in_asked_height,
                                      const uint32_t& in_max_surface_extent);

            /* Returns the log line that reports a resize: "The <what> is made 2064 x 1548 instead of 640 x 480 (scale 3.225)". */
            static std::string describe_resize(const std::string& in_what,
                                               const uint32_t&    in_asked_width,
                                               const uint32_t&    in_asked_height,
                                               const uint32_t&    in_real_width,
                                               const uint32_t&    in_real_height,
                                               const float&       in_scale);

            /* Returns the explanation for the case where the eye images at this scale are larger than the channel can pass on. */
            static std::string describe_too_large(const uint32_t& in_real_width,
                                                  const uint32_t& in_real_height,
                                                  const float&    in_scale);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_RENDER_SCALE_PLANNER_H */
