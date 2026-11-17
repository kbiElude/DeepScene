/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/convergence_model.h"
#include <algorithm>
#include <math.h>

/* The fraction of the way to its target that a value covers in one frame at 72 Hz (the headset rate the easing was tuned for). */
constexpr float FRACTION_PER_REFERENCE_FRAME = 0.05f;

Stereo::Core::ConvergenceModel::ConvergenceModel()
    :m_convergence_distance                (0.0f),
     m_depth_strength                      (0.0f),
     m_draw_convergence_distance           (0.0f),
     m_eye_separation                      (0.0f),
     m_ipd_scale                           (1.0f),
     m_limits_follow_strength              (0.0f),
     m_previous_scene_convergence_target   (0.0f),
     m_previous_scene_max_separation_target(0.0f),
     m_scene_convergence_distance          (0.0f),
     m_scene_max_separation                (0.0f),
     m_snaps_at_cuts                       (false)
{
    /* Stub */
}

bool Stereo::Core::ConvergenceModel::follow_scene_convergence_distance(const float& in_target,
                                                                       const float& in_fraction_of_the_way)
{
    const bool result = (in_target > 0.0f);

    if (result)
    {
        const bool is_target_cut = is_cut(m_previous_scene_convergence_target,
                                          in_target);
        const bool is_at_cut     = (m_snaps_at_cuts &&
                                    is_target_cut);

        m_scene_convergence_distance        = (is_at_cut) ? in_target
                                                          : get_eased_value(m_scene_convergence_distance,
                                                                            in_target,
                                                                            in_fraction_of_the_way);
        m_previous_scene_convergence_target = in_target;
    }

    return result;
}

void Stereo::Core::ConvergenceModel::follow_scene_max_separation(const float& in_target,
                                                            const float& in_fraction_of_the_way)
{
    const bool is_target_cut = is_cut(m_previous_scene_max_separation_target,
                                      in_target);
    const bool is_at_cut     = (m_snaps_at_cuts &&
                                is_target_cut);

    m_scene_max_separation                 = (in_target <= 0.0f) ? 0.0f
                                           : (is_at_cut)         ? in_target
                                                                 : get_eased_value(m_scene_max_separation,
                                                                                   in_target,
                                                                                   in_fraction_of_the_way);
    m_previous_scene_max_separation_target = in_target;
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
    /* Within this fraction of the target, the value snaps to the target. */
    constexpr float SNAP_FRACTION = 0.001f;

    float result = in_target;

    if (in_current > 0.0f)
    {
        /* Eases on the logarithm because a depth is perceived in proportion to its size. */
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
    /* One thirtieth of the convergence distance: a rule of thumb for the camera distance of a stereo shot. */
    constexpr float SEPARATION_PER_CONVERGENCE_DISTANCE = 1.0f / 30.0f;

    const float convergence_distance = get_effective_convergence_distance();

    /* The nearest surface of the scene may allow less separation than the natural one. The scene limit caps the separation. A depth strength
     * below 1 asks for less separation, so the cap is lowered by the same factor. A strength above 1 raises the cap by the part of the excess that
     * the recipe says (none by default: no strength raises it).
     */
    const float natural_separation = m_depth_strength * m_ipd_scale * SEPARATION_PER_CONVERGENCE_DISTANCE * convergence_distance;
    const float wished_for_part    = (m_depth_strength <= 1.0f) ? m_depth_strength
                                                                : 1.0f + (m_depth_strength - 1.0f) * m_limits_follow_strength;
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
    float result = 0.0f;

    if (in_elapsed_seconds > 0.0f)
    {
        const float n_reference_frames = in_elapsed_seconds / get_reference_frame_seconds();

        result = 1.0f - ::powf(1.0f - FRACTION_PER_REFERENCE_FRAME,
                               n_reference_frames);
    }

    return result;
}

float Stereo::Core::ConvergenceModel::get_fraction_per_frame()
{
    return FRACTION_PER_REFERENCE_FRAME;
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

float Stereo::Core::ConvergenceModel::get_scene_convergence_distance() const
{
    return m_scene_convergence_distance;
}

bool Stereo::Core::ConvergenceModel::is_cut(const float& in_previous_target,
                                            const float& in_target)
{
    /* Targets of frames in one scene are close together. A cut makes them jump. */
    constexpr float CUT_RATIO = 2.0f;

    bool result = false;

    if (in_previous_target > 0.0f &&
        in_target          > 0.0f)
    {
        const float ratio = (in_target > in_previous_target) ? in_target          / in_previous_target
                                                             : in_previous_target / in_target;

        result = (ratio > CUT_RATIO);
    }

    return result;
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

bool Stereo::Core::ConvergenceModel::set_limits_follow_strength(const float& in_limits_follow_strength)
{
    const bool is_finite = ::isfinite(in_limits_follow_strength);
    const bool result    = (is_finite                       &&
                            in_limits_follow_strength >= 0.0f &&
                            in_limits_follow_strength <= 1.0f);

    if (result)
    {
        m_limits_follow_strength = in_limits_follow_strength;
    }

    return result;
}

void Stereo::Core::ConvergenceModel::set_snaps_at_cuts(const bool& in_snaps_at_cuts)
{
    m_snaps_at_cuts = in_snaps_at_cuts;
}
