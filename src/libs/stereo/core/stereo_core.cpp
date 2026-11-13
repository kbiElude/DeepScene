/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/push_shift_planner.h"
#include "stereo/core/stereo_core.h"
#include <stdlib.h>

Stereo::Core::StereoCore::StereoCore(const Analysis::Recipe::Recipe& in_recipe,
                                     const FailFunc&                 in_fail_func)
    :m_applier               (in_recipe),
     m_fail_func             (in_fail_func),
     m_frame_interval_seconds(ConvergenceModel::get_reference_frame_seconds() ),
     m_is_last_draw_covering (false),
     m_is_last_draw_part     (false),
     m_is_last_draw_pushed   (false),
     m_is_overlay_pushed     (false),
     m_last_draw_class       (Analysis::Recipe::DrawClass::AFFECTED),
     m_last_draw_cover_key   (0),
     m_n_unknown_draws       (0),
     m_n_unknown_frames      (0),
     m_scene_shape_key       (0),
     m_view_matrix_fp32vec16 ()
{
    m_view_matrix_fp32vec16 = {1.0f, 0.0f, 0.0f, 0.0f,
                               0.0f, 1.0f, 0.0f, 0.0f,
                               0.0f, 0.0f, 1.0f, 0.0f,
                               0.0f, 0.0f, 0.0f, 1.0f};

    require_fast_path(in_recipe.is_baked,
                      "The recipe is not baked, so nothing tells the converter what the draws of the application are");

    for (const auto& rule : in_recipe.rule_vec)
    {
        require_fast_path(rule.pass_key_vec.empty () &&
                          rule.shape_key_vec.empty(),
                          "The rule \"" + rule.name + "\" is about a pass or a frame shape, which is not known while the application "
                          "runs");
    }
}

uint32_t Stereo::Core::StereoCore::bind_matrices(const Analysis::Recipe::DrawClass& in_draw_class,
                                                 const Analysis::Recipe::Eye&       in_eye,
                                                 std::vector<MatrixChange>*         inout_change_vec_ptr,
                                                 const bool&                        in_is_final_target)
{
    return m_matrix_slots.bind(in_draw_class,
                               in_eye,
                               m_convergence_model.get_effective_convergence_distance(),
                               m_convergence_model.get_effective_eye_separation      (),
                               (in_is_final_target && m_is_last_draw_covering == false),
                               (m_is_overlay_pushed) ? m_applier.get_overlay_parallax()
                                                     : 0.0f,
                               inout_change_vec_ptr);
}

Analysis::Recipe::DrawClass Stereo::Core::StereoCore::classify_draw(const uint64_t& in_signature_key,
                                                                    const uint64_t& in_cover_key)
{
    const bool is_cover_pushed = m_applier.is_cover_pushed(in_cover_key);

    if (m_applier.is_signature_known(in_signature_key) == false)
    {
        m_n_unknown_draws++;
    }

    m_convergence_model.set_draw_convergence_distance(m_applier.get_convergence_distance(in_signature_key) );

    m_is_last_draw_covering = m_applier.is_draw_covering(in_cover_key);
    m_is_last_draw_part     = m_applier.is_part_cover   (in_cover_key);
    m_last_draw_class       = m_applier.classify_draw    (in_signature_key,
                                                          m_view_matrix_fp32vec16);
    m_last_draw_cover_key   = in_cover_key;
    m_is_last_draw_pushed   = m_applier.is_signature_pushed(in_signature_key,
                                                            m_view_matrix_fp32vec16);

    if (m_is_last_draw_pushed == false)
    {
        m_is_last_draw_pushed = is_cover_pushed;
    }

    /* A draw that is named by its size as pushed is flat: the parts of an object that are drawn alike, one of them a protruding part, have no depth of their own to show. */
    if (m_last_draw_class == Analysis::Recipe::DrawClass::AFFECTED &&
        is_cover_pushed)
    {
        m_last_draw_class = Analysis::Recipe::DrawClass::UNAFFECTED;
    }

    return m_last_draw_class;
}

void Stereo::Core::StereoCore::fail(const std::string& in_message) const
{
    if (m_fail_func)
    {
        m_fail_func(in_message);
    }
    else
    {
        ::abort();
    }
}

void Stereo::Core::StereoCore::forget_matrices()
{
    m_matrix_slots.forget();
}

bool Stereo::Core::StereoCore::get_caches_pixel_reads() const
{
    return m_applier.get_caches_pixel_reads();
}

float Stereo::Core::StereoCore::get_camera_attached_depth() const
{
    return m_applier.get_comfort().camera_attached_depth;
}

float Stereo::Core::StereoCore::get_convergence_distance() const
{
    return m_convergence_model.get_convergence_distance();
}

float Stereo::Core::StereoCore::get_depth_strength() const
{
    return m_convergence_model.get_depth_strength();
}

float Stereo::Core::StereoCore::get_effective_convergence_distance() const
{
    return m_convergence_model.get_effective_convergence_distance();
}

float Stereo::Core::StereoCore::get_effective_eye_separation() const
{
    return m_convergence_model.get_effective_eye_separation();
}

float Stereo::Core::StereoCore::get_eye_separation() const
{
    return m_convergence_model.get_eye_separation();
}

float Stereo::Core::StereoCore::get_ipd_scale() const
{
    return m_convergence_model.get_ipd_scale();
}

bool Stereo::Core::StereoCore::get_is_last_draw_part() const
{
    return m_is_last_draw_part;
}

bool Stereo::Core::StereoCore::get_is_last_draw_pushed() const
{
    return m_is_last_draw_pushed;
}

bool Stereo::Core::StereoCore::get_is_overlay_pushed() const
{
    return m_is_overlay_pushed;
}

bool Stereo::Core::StereoCore::get_measures_depth() const
{
    return m_applier.get_comfort().measures_depth;
}

uint64_t Stereo::Core::StereoCore::get_n_unknown_draws() const
{
    return m_n_unknown_draws;
}

uint64_t Stereo::Core::StereoCore::get_n_unknown_frames() const
{
    return m_n_unknown_frames;
}

float Stereo::Core::StereoCore::get_near_fraction() const
{
    return m_applier.get_comfort().near_fraction;
}

const std::array<float, 16>& Stereo::Core::StereoCore::get_view_matrix() const
{
    return m_view_matrix_fp32vec16;
}

std::array<float, 16> Stereo::Core::StereoCore::make_eye_shift_matrix(const Analysis::Recipe::Eye& in_eye) const
{
    constexpr float HALF = 0.5f;

    const float distance   = m_convergence_model.get_effective_convergence_distance();
    const float separation = m_convergence_model.get_effective_eye_separation      ();

    /* The scene moves toward the right for the left eye: the camera is to the left. */
    const float half_shift = (in_eye == Analysis::Recipe::Eye::LEFT) ? separation * HALF
                                                                    : -separation * HALF;

    std::array<float, 16> result =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    result.at(12) = half_shift;

    if (distance > 0.0f)
    {
        result.at(8) = half_shift / distance;
    }

    return result;
}

void Stereo::Core::StereoCore::on_frame_completed(const uint64_t& in_shape_key)
{
    const float fraction_of_the_way = ConvergenceModel::get_fraction_of_the_way(m_frame_interval_seconds);
    const float scene_distance      = m_applier.get_scene_convergence_distance(in_shape_key);

    m_scene_shape_key = in_shape_key;

    if (m_convergence_model.follow_scene_convergence_distance(scene_distance,
                                                              fraction_of_the_way) == false)
    {
        /* The scene is not known: the limit of the one before stays with its distance, for lifting it would give the eyes more room than
         * the scene that is on the screen may allow.
         */
        m_n_unknown_frames++;
    }
    else
    {
        const float scene_max_separation = m_applier.get_scene_max_separation(in_shape_key);

        m_convergence_model.follow_scene_max_separation(scene_max_separation,
                                                        fraction_of_the_way);
    }
}

bool Stereo::Core::StereoCore::on_frame_depth(const uint64_t&   in_shape_key,
                                              const DepthRange& in_depth_range,
                                              const float&      in_projection_scale)
{
    const bool result = in_depth_range.has_depth;

    if (result)
    {
        const auto limits = Analysis::Recipe::RecipeApplier::get_comfort_limits(m_applier.get_comfort(),
                                                                                m_applier.find_scene_comfort_ptr(in_shape_key),
                                                                                in_depth_range.median_depth,
                                                                                in_depth_range.near_depth,
                                                                                in_depth_range.far_depth,
                                                                                in_projection_scale);

        const float fraction_of_the_way = ConvergenceModel::get_fraction_of_the_way(m_frame_interval_seconds);

        m_convergence_model.follow_scene_convergence_distance(limits.distance,
                                                              fraction_of_the_way);
        m_convergence_model.follow_scene_max_separation      (limits.max_separation,
                                                              fraction_of_the_way);
    }

    return result;
}

bool Stereo::Core::StereoCore::on_matrix_call(const APIInterceptor::APIFunction&                      in_function,
                                              const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                              const std::array<float, 16>&                            in_matrix)
{
    return m_matrix_slots.on_matrix_call(m_applier,
                                         in_function,
                                         in_argument_vec,
                                         in_matrix);
}

void Stereo::Core::StereoCore::on_view_matrix_set(const std::array<float, 16>& in_matrix)
{
    m_view_matrix_fp32vec16 = in_matrix;
}

float Stereo::Core::StereoCore::plan_follow_shift(const Analysis::Recipe::Eye& in_eye,
                                                  const float&                 in_picture_width_pixels,
                                                  const float&                 in_origin_depth,
                                                  const float&                 in_projection_x_scale,
                                                  const bool&                  in_is_perspective) const
{
    constexpr float HALF = 0.5f;

    const float distance   = m_convergence_model.get_effective_convergence_distance();
    const float separation = m_convergence_model.get_effective_eye_separation      ();
    float       result     = 0.0f;

    if (m_is_last_draw_part == false ||
        in_origin_depth     >= 0.0f  ||
        distance            <= 0.0f)
    {
        goto end;
    }

    {
        /* What make_eye_shift_matrix() does to a point at the depth: the half of the separation, less what the convergence takes back (in front of the camera, the depth is below 0). */
        const float half_shift     = (in_eye == Analysis::Recipe::Eye::LEFT) ? separation * HALF
                                                                             : -separation * HALF;
        const float shift_in_units = half_shift * (1.0f + in_origin_depth / distance);
        const float shift_in_ndc   = (in_is_perspective) ? shift_in_units * in_projection_x_scale / -in_origin_depth
                                                         : shift_in_units * in_projection_x_scale;

        result = shift_in_ndc * in_picture_width_pixels * HALF;
    }

end:
    return result;
}

float Stereo::Core::StereoCore::plan_part_shift(const Analysis::Recipe::Eye& in_eye,
                                                const float&                 in_picture_width_pixels,
                                                const float&                 in_origin_depth,
                                                const float&                 in_projection_x_scale,
                                                const bool&                  in_is_perspective) const
{
    constexpr float HALF = 0.5f;

    const float follow_shift = plan_follow_shift(in_eye,
                                                 in_picture_width_pixels,
                                                 in_origin_depth,
                                                 in_projection_x_scale,
                                                 in_is_perspective);
    const float push_shift   = plan_push_shift  (in_eye,
                                                 in_picture_width_pixels);
    float       result       = push_shift + follow_shift;

    if (m_is_last_draw_part)
    {
        const auto& comfort    = m_applier.get_comfort();
        const auto* scene_ptr  = m_applier.find_scene_comfort_ptr(m_scene_shape_key);
        const float near_limit = (scene_ptr != nullptr && scene_ptr->max_near_parallax > 0.0f) ? scene_ptr->max_near_parallax
                                                                                               : comfort.max_near_parallax;
        const float far_limit  = (scene_ptr != nullptr && scene_ptr->max_far_parallax  > 0.0f) ? scene_ptr->max_far_parallax
                                                                                               : comfort.max_far_parallax;

        /* The two pictures of the part are apart by twice the shift; the left one goes to the right for a part that is nearer than the screen. */
        const float sign      = (in_eye == Analysis::Recipe::Eye::LEFT) ? 1.0f
                                                                        : -1.0f;
        float       parallax  = sign * result / (HALF * in_picture_width_pixels);

        if (near_limit > 0.0f &&
            parallax   > near_limit)
        {
            parallax = near_limit;
        }

        if (far_limit > 0.0f &&
            parallax  < -far_limit)
        {
            parallax = -far_limit;
        }

        result = sign * parallax * HALF * in_picture_width_pixels;
    }

    return result;
}

float Stereo::Core::StereoCore::plan_push_shift(const Analysis::Recipe::Eye& in_eye,
                                                const float&                 in_picture_width_pixels) const
{
    const float in_parallax = m_applier.get_cover_push_parallax(m_last_draw_cover_key);

    return PushShiftPlanner::plan(m_applier,
                                  in_eye,
                                  in_parallax,
                                  (m_last_draw_class == Analysis::Recipe::DrawClass::AFFECTED),
                                  in_picture_width_pixels,
                                  m_convergence_model.get_effective_eye_separation(),
                                  0.0f /* the effective separation is within the limit of the scene */);
}

Stereo::Core::ScreenShiftPlan Stereo::Core::StereoCore::plan_screen_shift(const Analysis::Recipe::Eye& in_eye,
                                                                          const bool&                  in_has_extent,
                                                                          const bool&                  in_is_presented,
                                                                          const float&                 in_picture_width,
                                                                          const bool&                  in_reaches_both_sides) const
{
    ScreenParallaxPlanner::Input input;

    input.convergence_distance = m_convergence_model.get_effective_convergence_distance();
    input.cover_key            = m_last_draw_cover_key;
    input.eye                  = in_eye;
    input.eye_separation       = m_convergence_model.get_effective_eye_separation();
    input.has_extent           = in_has_extent;
    input.is_affected          = (m_last_draw_class == Analysis::Recipe::DrawClass::AFFECTED);
    input.is_overlay_pushed    = m_is_overlay_pushed;
    input.is_presented         = in_is_presented;
    input.picture_width        = in_picture_width;
    input.reaches_both_sides   = in_reaches_both_sides;
    input.separation_limit     = 0.0f; /* the effective separation is within it */

    return ScreenParallaxPlanner::plan(m_applier,
                                       input);
}

void Stereo::Core::StereoCore::require_fast_path(const bool&        in_is_ok,
                                                 const std::string& in_reason) const
{
    /* Limitation: an application that fails here would need the calls of a pass held back until the pass is known; the converter has no
     * such deferred path.
     */
    if (in_is_ok == false)
    {
        fail(in_reason                                                                                                   +
             ". This application needs the converter to hold the calls of a pass back until it knows which pass it is, "
             "which the converter cannot do");
    }
}

void Stereo::Core::StereoCore::require_supported(const bool&        in_is_ok,
                                                 const std::string& in_reason) const
{
    if (in_is_ok == false)
    {
        fail(in_reason);
    }
}

void Stereo::Core::StereoCore::set_convergence_distance(const float& in_convergence_distance)
{
    if (m_convergence_model.set_convergence_distance(in_convergence_distance) == false)
    {
        fail("The convergence distance must be zero (none) or more");
    }
}

void Stereo::Core::StereoCore::set_depth_strength(const float& in_depth_strength)
{
    if (m_convergence_model.set_depth_strength(in_depth_strength) == false)
    {
        fail("The strength of the 3D must be zero or more");
    }
}

void Stereo::Core::StereoCore::set_eye_separation(const float& in_eye_separation)
{
    if (m_convergence_model.set_eye_separation(in_eye_separation) == false)
    {
        fail("The eye separation must be zero or more");
    }
}

void Stereo::Core::StereoCore::set_frame_interval_seconds(const float& in_frame_interval_seconds)
{
    m_frame_interval_seconds = in_frame_interval_seconds;
}

void Stereo::Core::StereoCore::set_ipd_scale(const float& in_ipd_scale)
{
    if (m_convergence_model.set_ipd_scale(in_ipd_scale) == false)
    {
        fail("The scale of the distance between the eyes of the person must be more than zero");
    }
}

void Stereo::Core::StereoCore::set_is_overlay_pushed(const bool& in_is_pushed)
{
    m_is_overlay_pushed = in_is_pushed;
}

void Stereo::Core::StereoCore::set_scene_shape_key(const uint64_t& in_shape_key)
{
    m_scene_shape_key = in_shape_key;
}
