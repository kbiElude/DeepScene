/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Stereo/Core/stereo_core.h"
#include <stdlib.h>

Stereo::Core::StereoCore::StereoCore(const Analysis::RecipeLoader::Recipe& in_recipe,
                                     const FailFunc&                       in_fail_func)
    :m_applier               (in_recipe),
     m_fail_func             (in_fail_func),
     m_is_last_draw_covering (false),
     m_is_overlay_pushed     (false),
     m_last_draw_class       (Analysis::RecipeLoader::DrawClass::AFFECTED),
     m_last_draw_cover_key   (0),
     m_n_unknown_draws       (0),
     m_n_unknown_frames      (0),
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

uint32_t Stereo::Core::StereoCore::bind_matrices(const Analysis::RecipeLoader::DrawClass& in_draw_class,
                                                 const Analysis::RecipeLoader::Eye&       in_eye,
                                                 std::vector<MatrixChange>*               inout_change_vec_ptr,
                                                 const bool&                              in_is_final_target)
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

Analysis::RecipeLoader::DrawClass Stereo::Core::StereoCore::classify_draw(const uint64_t& in_signature_key,
                                                                          const uint64_t& in_cover_key)
{
    if (m_applier.is_signature_known(in_signature_key) == false)
    {
        m_n_unknown_draws++;
    }

    m_convergence_model.set_draw_convergence_distance(m_applier.get_convergence_distance(in_signature_key) );

    m_is_last_draw_covering = m_applier.is_draw_covering(in_cover_key);
    m_last_draw_class       = m_applier.classify_draw(in_signature_key,
                                                      m_view_matrix_fp32vec16);
    m_last_draw_cover_key   = in_cover_key;

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

float Stereo::Core::StereoCore::get_convergence_distance() const
{
    return m_convergence_model.get_convergence_distance();
}

float Stereo::Core::StereoCore::get_depth_strength() const
{
    return m_convergence_model.get_depth_strength();
}

float Stereo::Core::StereoCore::get_eye_separation() const
{
    return m_convergence_model.get_eye_separation();
}

float Stereo::Core::StereoCore::get_ipd_scale() const
{
    return m_convergence_model.get_ipd_scale();
}

bool Stereo::Core::StereoCore::get_is_overlay_pushed() const
{
    return m_is_overlay_pushed;
}

uint64_t Stereo::Core::StereoCore::get_n_unknown_draws() const
{
    return m_n_unknown_draws;
}

uint64_t Stereo::Core::StereoCore::get_n_unknown_frames() const
{
    return m_n_unknown_frames;
}

const std::array<float, 16>& Stereo::Core::StereoCore::get_view_matrix() const
{
    return m_view_matrix_fp32vec16;
}

void Stereo::Core::StereoCore::on_frame_completed(const uint64_t& in_shape_key)
{
    if (m_convergence_model.follow_scene_convergence_distance(m_applier.get_scene_convergence_distance(in_shape_key) ) == false)
    {
        /* The scene is not known: the limit of the one before stays with its distance, for lifting it would give the eyes more room than
         * the scene that is on the screen may allow.
         */
        m_n_unknown_frames++;
    }
    else
    {
        m_convergence_model.follow_scene_max_separation(m_applier.get_scene_max_separation(in_shape_key) );
    }
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

Stereo::Core::ScreenShiftPlan Stereo::Core::StereoCore::plan_screen_shift(const Analysis::RecipeLoader::Eye& in_eye,
                                                                          const bool&                        in_has_extent,
                                                                          const bool&                        in_is_presented,
                                                                          const float&                       in_picture_width,
                                                                          const bool&                        in_reaches_both_sides) const
{
    ScreenParallaxPlanner::Input input;

    input.convergence_distance = m_convergence_model.get_effective_convergence_distance();
    input.cover_key            = m_last_draw_cover_key;
    input.eye                  = in_eye;
    input.eye_separation       = m_convergence_model.get_effective_eye_separation();
    input.has_extent           = in_has_extent;
    input.is_affected          = (m_last_draw_class == Analysis::RecipeLoader::DrawClass::AFFECTED);
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
    /* TODO: the deferred path - the calls of a pass held back until the pass is known - is what an application that fails here
     * needs; nothing of it exists yet.
     */
    if (in_is_ok == false)
    {
        fail(in_reason                                                                                                   +
             ". This application needs the converter to hold the calls of a pass back until it knows which pass it is, "
             "which the converter cannot do yet");
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
