/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/resolution_policy.h"
#include "eye_image_limits.h"
#include "render_scale_planner.h"

float Stereo::ConverterDLL::RenderScalePlanner::choose_scale(const uint32_t& in_target_width,
                                                             const uint32_t& in_target_height,
                                                             const uint32_t& in_asked_width,
                                                             const uint32_t& in_asked_height,
                                                             const uint32_t& in_max_surface_extent)
{
    const float channel_scale = EyeImageLimits::get_largest_scale            (in_asked_width,
                                                                              in_asked_height);
    const float fit_scale     = Stereo::Core::ResolutionPolicy::get_fit_scale(in_target_width,
                                                                              in_target_height,
                                                                              in_asked_width,
                                                                              in_asked_height);
    const float surface_scale = (in_max_surface_extent != 0) ? Stereo::Core::ResolutionPolicy::get_fit_scale(in_max_surface_extent,
                                                                                                             in_max_surface_extent,
                                                                                                             in_asked_width,
                                                                                                             in_asked_height)
                                                             : fit_scale;
    const float smaller_scale = (fit_scale < channel_scale) ? fit_scale
                                                            : channel_scale;

    return (surface_scale < smaller_scale) ? surface_scale
                                           : smaller_scale;
}

Stereo::ConverterDLL::EyeLayout Stereo::ConverterDLL::RenderScalePlanner::choose_layout(const uint32_t& in_target_width,
                                                                                       const uint32_t& in_target_height,
                                                                                       const uint32_t& in_picture_width,
                                                                                       const uint32_t& in_picture_height,
                                                                                       const uint32_t& in_max_surface_extent)
{
    const bool is_target_given = (in_target_width  > 0 &&
                                  in_target_height > 0);
    const bool is_target_fit   = (is_target_given                   &&
                                  in_picture_width  <= in_target_width  &&
                                  in_picture_height <= in_target_height &&
                                  (in_max_surface_extent == 0 ||
                                   (in_target_width  <= in_max_surface_extent &&
                                    in_target_height <= in_max_surface_extent) ) );
    EyeLayout  result;

    result.frame_height   = (is_target_fit) ? in_target_height
                                            : in_picture_height;
    result.frame_width    = (is_target_fit) ? in_target_width
                                            : in_picture_width;
    result.picture.height = in_picture_height;
    result.picture.width  = in_picture_width;
    result.picture.x      = (result.frame_width  - in_picture_width)  / 2;
    result.picture.y      = (result.frame_height - in_picture_height) / 2;

    return result;
}

std::string Stereo::ConverterDLL::RenderScalePlanner::describe_resize(const std::string& in_what,
                                                                      const uint32_t&    in_asked_width,
                                                                      const uint32_t&    in_asked_height,
                                                                      const uint32_t&    in_real_width,
                                                                      const uint32_t&    in_real_height,
                                                                      const float&       in_scale)
{
    return "The "                              +
           in_what                             +
           " is made "                         +
           std::to_string(in_real_width)       +
           " x "                               +
           std::to_string(in_real_height)      +
           " instead of "                      +
           std::to_string(in_asked_width)      +
           " x "                               +
           std::to_string(in_asked_height)     +
           " (scale "                          +
           std::to_string(in_scale)            +
           ")";
}

std::string Stereo::ConverterDLL::RenderScalePlanner::describe_too_large(const uint32_t& in_real_width,
                                                                         const uint32_t& in_real_height,
                                                                         const float&    in_scale)
{
    return "At the render scale "                                +
           std::to_string(in_scale)                              +
           " the images of the eyes are "                        +
           std::to_string(in_real_width)                         +
           " x "                                                 +
           std::to_string(in_real_height)                        +
           " each, which is larger than what can be passed on (" +
           std::to_string(EyeImageLimits::get_max_width() )      +
           " x "                                                 +
           std::to_string(EyeImageLimits::get_max_height() )     +
           " each); lower the render scale";
}
