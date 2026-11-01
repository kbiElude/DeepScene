/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/StereoCore/convergence_model.h"
#include <algorithm>
#include <math.h>

Stereo::StereoCore::ConvergenceModel::ConvergenceModel()
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

bool Stereo::StereoCore::ConvergenceModel::follow_scene_convergence_distance(const float& in_target)
{
    /* How much of the way to the distance of the scene the distance goes in a frame, and how close is the same. */
    constexpr float SMOOTHING_PER_FRAME = 0.05f;
    constexpr float SNAP_FRACTION       = 0.001f;

    const bool result = (in_target > 0.0f);

    if (result)
    {
        if (m_scene_convergence_distance > 0.0f)
        {
            /* Along the logarithm, as a depth is felt in proportion to itself. */
            const float eased = ::expf(::logf(m_scene_convergence_distance) +
                                       (::logf(in_target) - ::logf(m_scene_convergence_distance)) * SMOOTHING_PER_FRAME);

            m_scene_convergence_distance = (::fabsf(eased - in_target) < in_target * SNAP_FRACTION) ? in_target
                                                                                                    : eased;
        }
        else
        {
            m_scene_convergence_distance = in_target;
        }
    }

    return result;
}

void Stereo::StereoCore::ConvergenceModel::follow_scene_max_separation(const float& in_target)
{
    /* The same way to the target as the distance has. */
    constexpr float SMOOTHING_PER_FRAME = 0.05f;
    constexpr float SNAP_FRACTION       = 0.001f;

    if (in_target <= 0.0f)
    {
        m_scene_max_separation = 0.0f;
    }
    else
    if (m_scene_max_separation > 0.0f)
    {
        const float eased = ::expf(::logf(m_scene_max_separation) +
                                   (::logf(in_target) - ::logf(m_scene_max_separation)) * SMOOTHING_PER_FRAME);

        m_scene_max_separation = (::fabsf(eased - in_target) < in_target * SNAP_FRACTION) ? in_target
                                                                                          : eased;
    }
    else
    {
        m_scene_max_separation = in_target;
    }
}

float Stereo::StereoCore::ConvergenceModel::get_convergence_distance() const
{
    return m_convergence_distance;
}

float Stereo::StereoCore::ConvergenceModel::get_depth_strength() const
{
    return m_depth_strength;
}

float Stereo::StereoCore::ConvergenceModel::get_effective_convergence_distance() const
{
    return (m_convergence_distance       > 0.0f) ? m_convergence_distance
         : (m_scene_convergence_distance > 0.0f) ? m_scene_convergence_distance
                                                 : m_draw_convergence_distance;
}

float Stereo::StereoCore::ConvergenceModel::get_effective_eye_separation() const
{
    /* A thirtieth of the distance at which the eyes converge: the rule of thumb for the distance between the cameras of a stereo shot. */
    constexpr float SEPARATION_PER_CONVERGENCE_DISTANCE = 1.0f / 30.0f;

    const float convergence_distance = get_effective_convergence_distance();

    /* What the eyes can bear at the nearest surface of the scene may be less than that: the limit of the scene says, and no strength is
     * allowed to take it further. A strength below 1 is a wish for less, which the limit follows.
     */
    const float natural_separation = m_depth_strength * m_ipd_scale * SEPARATION_PER_CONVERGENCE_DISTANCE * convergence_distance;
    const float separation         = (m_scene_max_separation > 0.0f) ? std::min(natural_separation,
                                                                                 m_scene_max_separation * std::min(m_depth_strength,
                                                                                                                   1.0f) )
                                                                     : natural_separation;

    return (m_depth_strength     >  0.0f  &&
            convergence_distance >  0.0f) ? separation
                                          : m_eye_separation;
}

float Stereo::StereoCore::ConvergenceModel::get_eye_separation() const
{
    return m_eye_separation;
}

float Stereo::StereoCore::ConvergenceModel::get_ipd_scale() const
{
    return m_ipd_scale;
}

bool Stereo::StereoCore::ConvergenceModel::set_convergence_distance(const float& in_convergence_distance)
{
    const bool result = (::isfinite(in_convergence_distance)         &&
                         in_convergence_distance             >= 0.0f);

    if (result)
    {
        m_convergence_distance = in_convergence_distance;
    }

    return result;
}

bool Stereo::StereoCore::ConvergenceModel::set_depth_strength(const float& in_depth_strength)
{
    const bool result = (::isfinite(in_depth_strength)          &&
                         in_depth_strength             >= 0.0f);

    if (result)
    {
        m_depth_strength = in_depth_strength;
    }

    return result;
}

void Stereo::StereoCore::ConvergenceModel::set_draw_convergence_distance(const float& in_draw_convergence_distance)
{
    m_draw_convergence_distance = in_draw_convergence_distance;
}

bool Stereo::StereoCore::ConvergenceModel::set_eye_separation(const float& in_eye_separation)
{
    const bool result = (::isfinite(in_eye_separation)          &&
                         in_eye_separation             >= 0.0f);

    if (result)
    {
        m_eye_separation = in_eye_separation;
    }

    return result;
}

bool Stereo::StereoCore::ConvergenceModel::set_ipd_scale(const float& in_ipd_scale)
{
    const bool result = (::isfinite(in_ipd_scale) &&
                         in_ipd_scale             >  0.0f);

    if (result)
    {
        m_ipd_scale = in_ipd_scale;
    }

    return result;
}
