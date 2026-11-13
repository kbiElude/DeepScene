/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/convergence_model.h"
#include <algorithm>
#include <math.h>

Stereo::Core::ConvergenceModel::ConvergenceModel()
    :m_convergence_distance      (0.0f),
     m_depth_strength            (0.0f),
     m_draw_convergence_distance (0.0f),
     m_eye_separation            (0.0f),
     m_ipd_scale                 (1.0f),
     m_scene_convergence_distance(0.0f),
     m_scene_max_separation      (0.0f)
{
    /* Stub */
}

bool Stereo::Core::ConvergenceModel::follow_scene_convergence_distance(const float& in_target,
                                                                  const float& in_fraction_of_the_way)
{
    const bool result = (in_target > 0.0f);

    if (result)
    {
        m_scene_convergence_distance = get_eased_value(m_scene_convergence_distance,
                                                       in_target,
                                                       in_fraction_of_the_way);
    }

    return result;
}

void Stereo::Core::ConvergenceModel::follow_scene_max_separation(const float& in_target,
                                                            const float& in_fraction_of_the_way)
{
    m_scene_max_separation = (in_target > 0.0f) ? get_eased_value(m_scene_max_separation,
                                                                   in_target,
                                                                   in_fraction_of_the_way)
                                                : 0.0f;
}

float Stereo::Core::ConvergenceModel::get_convergence_distance() const
{
    return m_convergence_distance;
}

float Stereo::Core::ConvergenceModel::get_depth_strength() const
{
    return m_depth_strength;
}

float Stereo::Core::ConvergenceModel::get_eased_value(const float& in_current,
                                                      const float& in_target,
                                                      const float& in_fraction_of_the_way)
{
    /* How close to the target is the same as being there. */
    constexpr float SNAP_FRACTION = 0.001f;

    float result = in_target;

    if (in_current > 0.0f)
    {
        /* Along the logarithm, as a depth is felt in proportion to itself. */
        const float eased = ::expf(::logf(in_current) +
                                   (::logf(in_target) - ::logf(in_current)) * in_fraction_of_the_way);

        result = (::fabsf(eased - in_target) < in_target * SNAP_FRACTION) ? in_target
                                                                          : eased;
    }

    return result;
}

float Stereo::Core::ConvergenceModel::get_effective_convergence_distance() const
{
    return (m_convergence_distance       > 0.0f) ? m_convergence_distance
         : (m_scene_convergence_distance > 0.0f) ? m_scene_convergence_distance
                                                 : m_draw_convergence_distance;
}

float Stereo::Core::ConvergenceModel::get_effective_eye_separation() const
{
    /* A thirtieth of the distance at which the eyes converge: the rule of thumb for the distance between the cameras of a stereo shot. */
    constexpr float SEPARATION_PER_CONVERGENCE_DISTANCE = 1.0f / 30.0f;

    const float convergence_distance = get_effective_convergence_distance();

    /* What the eyes can bear at the nearest surface of the scene may be less than that: the limit of the scene says, and no strength is
     * allowed to take it further. A strength below 1 is a wish for less, which the limit follows.
     */
    const float natural_separation = m_depth_strength * m_ipd_scale * SEPARATION_PER_CONVERGENCE_DISTANCE * convergence_distance;
    const float wished_for_part    = std::min(m_depth_strength,
                                              1.0f);
    const float separation         = (m_scene_max_separation > 0.0f) ? std::min(natural_separation,
                                                                                 m_scene_max_separation * wished_for_part)
                                                                     : natural_separation;

    return (m_depth_strength     >  0.0f  &&
            convergence_distance >  0.0f) ? separation
                                          : m_eye_separation;
}

float Stereo::Core::ConvergenceModel::get_eye_separation() const
{
    return m_eye_separation;
}

float Stereo::Core::ConvergenceModel::get_fraction_of_the_way(const float& in_elapsed_seconds)
{
    /* What a frame of the reference time goes. */
    constexpr float FRACTION_PER_REFERENCE_FRAME = 0.05f;

    float result = 0.0f;

    if (in_elapsed_seconds > 0.0f)
    {
        const float n_reference_frames = in_elapsed_seconds / get_reference_frame_seconds();

        result = 1.0f - ::powf(1.0f - FRACTION_PER_REFERENCE_FRAME,
                               n_reference_frames);
    }

    return result;
}

float Stereo::Core::ConvergenceModel::get_ipd_scale() const
{
    return m_ipd_scale;
}

float Stereo::Core::ConvergenceModel::get_reference_frame_seconds()
{
    constexpr float REFERENCE_FRAMES_PER_SECOND = 72.0f; /* the rate of the headset */

    return 1.0f / REFERENCE_FRAMES_PER_SECOND;
}

bool Stereo::Core::ConvergenceModel::set_convergence_distance(const float& in_convergence_distance)
{
    const bool is_finite = ::isfinite(in_convergence_distance);
    const bool result    = (is_finite                         &&
                            in_convergence_distance >= 0.0f);

    if (result)
    {
        m_convergence_distance = in_convergence_distance;
    }

    return result;
}

bool Stereo::Core::ConvergenceModel::set_depth_strength(const float& in_depth_strength)
{
    const bool is_finite = ::isfinite(in_depth_strength);
    const bool result    = (is_finite                  &&
                            in_depth_strength >= 0.0f);

    if (result)
    {
        m_depth_strength = in_depth_strength;
    }

    return result;
}

void Stereo::Core::ConvergenceModel::set_draw_convergence_distance(const float& in_draw_convergence_distance)
{
    m_draw_convergence_distance = in_draw_convergence_distance;
}

bool Stereo::Core::ConvergenceModel::set_eye_separation(const float& in_eye_separation)
{
    const bool is_finite = ::isfinite(in_eye_separation);
    const bool result    = (is_finite                  &&
                            in_eye_separation >= 0.0f);

    if (result)
    {
        m_eye_separation = in_eye_separation;
    }

    return result;
}

bool Stereo::Core::ConvergenceModel::set_ipd_scale(const float& in_ipd_scale)
{
    const bool is_finite = ::isfinite(in_ipd_scale);
    const bool result    = (is_finite          &&
                            in_ipd_scale > 0.0f);

    if (result)
    {
        m_ipd_scale = in_ipd_scale;
    }

    return result;
}
