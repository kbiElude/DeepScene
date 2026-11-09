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
        /* What the adapters of the APIs share in deciding how much larger an application renders than it asks (see ResolutionPolicy) and in saying it: the scale is the
         * largest that fits the images in the eye of the headset, within what the frame channel holds for both eyes and what a surface of the API can be.
         */
        class RenderScalePlanner
        {
        public:
            /* The scale for an application that asks for images of the size, shown in a target of the size (an eye of the headset): the least of what fits the target with the
             * proportions of the images, what the channel holds (see EyeImageLimits), and what fits in the largest extent that a surface of the API can have (0: no limit of
             * the API). Not brought inside the limits of a ResolutionPolicy, which says what it is.
             */
            static float choose_scale(const uint32_t& in_target_width,
                                      const uint32_t& in_target_height,
                                      const uint32_t& in_asked_width,
                                      const uint32_t& in_asked_height,
                                      const uint32_t& in_max_surface_extent);

            /* The line of the log that says what was made larger: "The <what> is made 2064 x 1548 instead of 640 x 480 (scale 3.225)". */
            static std::string describe_resize(const std::string& in_what,
                                               const uint32_t&    in_asked_width,
                                               const uint32_t&    in_asked_height,
                                               const uint32_t&    in_real_width,
                                               const uint32_t&    in_real_height,
                                               const float&       in_scale);

            /* Why the images of the eyes cannot be passed on at the scale, when they are larger than the channel holds. */
            static std::string describe_too_large(const uint32_t& in_real_width,
                                                  const uint32_t& in_real_height,
                                                  const float&    in_scale);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_RENDER_SCALE_PLANNER_H */
