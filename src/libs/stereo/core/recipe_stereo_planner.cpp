/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo/core/push_shift_planner.h"
#include "stereo/core/screen_space_vertex_shifter.h"
#include "stereo/core/recipe_stereo_planner.h"
#include <stdlib.h>

Stereo::Core::RecipeStereoPlanner::RecipeStereoPlanner(const Analysis::Recipe::Recipe& in_recipe,
                                                       const FailFunc&                 in_fail_func)
    :m_applier                   (in_recipe),
     m_fail_func                 (in_fail_func),
     m_frame_interval_seconds    (ConvergenceModel::get_reference_frame_seconds() ),
     m_is_last_draw_covering     (false),
     m_is_last_draw_eye_space    (false),
     m_is_last_draw_part         (false),
     m_is_last_draw_pushed       (false),
     m_is_last_draw_side_reaching(false),
     m_is_overlay_pushed         (in_recipe.comfort.pushes_by_default),
     m_last_draw_class           (Analysis::Recipe::DrawClass::AFFECTED),
     m_last_draw_cover_key       (0),
     m_n_unknown_draws           (0),
     m_n_unknown_frames          (0),
     m_scene_shape_key           (0),
     m_view_matrix_fp32vec16     ()
{
    m_view_matrix_fp32vec16 = {1.0f, 0.0f, 0.0f, 0.0f,
                               0.0f, 1.0f, 0.0f, 0.0f,
                               0.0f, 0.0f, 1.0f, 0.0f,
                               0.0f, 0.0f, 0.0f, 1.0f};

    m_convergence_model.set_limits_follow_strength(in_recipe.comfort.limits_follow_strength);
    m_convergence_model.set_snaps_at_cuts         (in_recipe.comfort.snaps_at_cuts);

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

uint32_t Stereo::Core::RecipeStereoPlanner::bind_matrices(const Analysis::Recipe::DrawClass& in_draw_class,
                                                          const Analysis::Recipe::Eye&       in_eye,
                                                          std::vector<MatrixChange>*         inout_change_vec_ptr,
                                                          const bool&                        in_is_final_target)
{
    return m_matrix_slots.bind(in_draw_class,
                               in_eye,
                               m_convergence_model.get_effective_convergence_distance(),
                               m_convergence_model.get_effective_eye_separation      (),
                               (in_is_final_target && m_is_last_draw_covering == false),
                               (get_is_last_draw_pushed() ) ? m_applier.get_overlay_parallax()
                                                            : 0.0f,
                               inout_change_vec_ptr);
}

Analysis::Recipe::DrawClass Stereo::Core::RecipeStereoPlanner::classify_draw(const uint64_t& in_signature_key,
                                                                             const uint64_t& in_cover_key)
{
    Analysis::Recipe::DrawQuery query;

    query.cover_key     = in_cover_key;
    query.signature_key = in_signature_key;
    query.view_matrix   = m_view_matrix_fp32vec16;

    const Analysis::Recipe::DrawDecision decision = m_applier.decide_draw(query);

    if (m_applier.is_signature_known(in_signature_key) == false)
    {
        m_n_unknown_draws++;
    }

    m_convergence_model.set_draw_convergence_distance(m_applier.get_convergence_distance(in_signature_key) );

    m_is_last_draw_covering      = m_applier.is_draw_covering  (in_cover_key);
    m_is_last_draw_eye_space     = m_applier.is_eye_space_cover(in_cover_key);
    m_is_last_draw_part          = m_applier.is_part_cover     (in_cover_key);
    m_is_last_draw_side_reaching = m_applier.is_side_reaching  (in_cover_key);

    m_last_draw_class     = decision.draw_class;
    m_last_draw_cover_key = in_cover_key;
    m_is_last_draw_pushed = decision.is_pushed;

    return m_last_draw_class;
}

void Stereo::Core::RecipeStereoPlanner::fail(const std::string& in_message) const
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

void Stereo::Core::RecipeStereoPlanner::forget_matrices()
{
    m_matrix_slots.forget();
}

bool Stereo::Core::RecipeStereoPlanner::get_caches_pixel_reads() const
{
    return m_applier.get_caches_pixel_reads();
}

float Stereo::Core::RecipeStereoPlanner::get_camera_attached_depth() const
{
    return m_applier.get_comfort().camera_attached_depth;
}

float Stereo::Core::RecipeStereoPlanner::get_convergence_distance() const
{
    return m_convergence_model.get_convergence_distance();
}

float Stereo::Core::RecipeStereoPlanner::get_depth_strength() const
{
    return m_convergence_model.get_depth_strength();
}

float Stereo::Core::RecipeStereoPlanner::get_effective_convergence_distance() const
{
    return m_convergence_model.get_effective_convergence_distance();
}

float Stereo::Core::RecipeStereoPlanner::get_effective_eye_separation() const
{
    return m_convergence_model.get_effective_eye_separation();
}

float Stereo::Core::RecipeStereoPlanner::get_eye_separation() const
{
    return m_convergence_model.get_eye_separation();
}

float Stereo::Core::RecipeStereoPlanner::get_fraction_of_the_way() const
{
    return (m_applier.get_comfort().eases_by_time) ? ConvergenceModel::get_fraction_of_the_way(m_frame_interval_seconds)
                                                   : ConvergenceModel::get_fraction_per_frame();
}

float Stereo::Core::RecipeStereoPlanner::get_ipd_scale() const
{
    return m_convergence_model.get_ipd_scale();
}

bool Stereo::Core::RecipeStereoPlanner::get_is_last_draw_eye_space() const
{
    return m_is_last_draw_eye_space;
}

bool Stereo::Core::RecipeStereoPlanner::get_is_last_draw_part() const
{
    return m_is_last_draw_part;
}

bool Stereo::Core::RecipeStereoPlanner::get_is_last_draw_pushed() const
{
    return (m_is_last_draw_pushed &&
            m_is_overlay_pushed);
}

bool Stereo::Core::RecipeStereoPlanner::get_is_last_draw_side_reaching() const
{
    return m_is_last_draw_side_reaching;
}

bool Stereo::Core::RecipeStereoPlanner::get_is_overlay_pushed() const
{
    return m_is_overlay_pushed;
}

bool Stereo::Core::RecipeStereoPlanner::get_measures_depth() const
{
    return m_applier.get_comfort().measures_depth;
}

uint64_t Stereo::Core::RecipeStereoPlanner::get_n_unknown_draws() const
{
    return m_n_unknown_draws;
}

uint64_t Stereo::Core::RecipeStereoPlanner::get_n_unknown_frames() const
{
    return m_n_unknown_frames;
}

float Stereo::Core::RecipeStereoPlanner::get_near_fraction() const
{
    return m_applier.get_comfort().near_fraction;
}

const std::array<float, 16>& Stereo::Core::RecipeStereoPlanner::get_view_matrix() const
{
    return m_view_matrix_fp32vec16;
}

bool Stereo::Core::RecipeStereoPlanner::is_last_draw_attached_to_camera(const float& in_origin_depth) const
{
    const float attached_depth = get_camera_attached_depth() * get_effective_convergence_distance();

    return (attached_depth            >  0.0f           &&
            in_origin_depth           <  attached_depth &&
            m_is_last_draw_eye_space == false);
}

bool Stereo::Core::RecipeStereoPlanner::make_screen_vertices(const Analysis::Recipe::Eye& in_eye,
                                                             const bool&                  in_is_presented,
                                                             const float&                 in_picture_width,
                                                             const uint8_t*               in_vertex_ptr,
                                                             const uint32_t&              in_vertex_n_bytes,
                                                             const size_t&                in_n_vertices,
                                                             const uint16_t*              in_index_ptr,
                                                             const size_t&                in_n_indices,
                                                             const bool&                  in_must_copy,
                                                             std::vector<uint8_t>*        out_vertex_vec_ptr) const
{
    const bool reaches_both_sides = ScreenSpaceVertexShifter::reaches_both_sides(in_vertex_ptr,
                                                                                 in_vertex_n_bytes,
                                                                                 in_n_vertices,
                                                                                 in_index_ptr,
                                                                                 in_n_indices,
                                                                                 in_picture_width);
    const auto plan               = plan_screen_shift                 (in_eye,
                                                                       true, /* in_has_extent */
                                                                       in_is_presented,
                                                                       in_picture_width,
                                                                       reaches_both_sides);
    bool       result             = false;

    if (plan.kind != ScreenShiftKind::NONE ||
        in_must_copy)
    {
        out_vertex_vec_ptr->assign(in_vertex_ptr,
                                   in_vertex_ptr + in_n_vertices * in_vertex_n_bytes);

        const bool is_moved = ScreenSpaceVertexShifter::apply_plan(plan,
                                                                   in_vertex_n_bytes,
                                                                   in_n_vertices,
                                                                   out_vertex_vec_ptr->data() );

        result = (is_moved     ||
                  in_must_copy);
    }

    return result;
}

std::array<float, 16> Stereo::Core::RecipeStereoPlanner::make_eye_shift_matrix(const Analysis::Recipe::Eye& in_eye) const
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

void Stereo::Core::RecipeStereoPlanner::on_frame_completed(const uint64_t& in_shape_key)
{
    const float fraction_of_the_way = get_fraction_of_the_way();
    const float scene_distance      = m_applier.get_scene_convergence_distance (in_shape_key);

    m_scene_shape_key = in_shape_key;

    if (m_convergence_model.follow_scene_convergence_distance(scene_distance,
                                                              fraction_of_the_way) == false)
    {
        /* The scene is unknown. The limit of the previous scene stays together with its distance, because lifting it would give the eyes
         * more room than the scene on the screen may allow. The distance of a far scene would be too much for a near one, so it comes down to
         * the distance that the recipe gives for the draws that have none, and the natural separation with it. A distance below it stays.
         */
        const float anchor_distance  = m_applier.get_view_anchor_shift().convergence_distance;
        const float current_distance = m_convergence_model.get_scene_convergence_distance();
        const bool  has_anchor       = m_applier.has_view_anchor_shift();

        if (has_anchor                    &&
            anchor_distance  >  0.0f      &&
            current_distance >  anchor_distance)
        {
            m_convergence_model.follow_scene_convergence_distance(anchor_distance,
                                                                  fraction_of_the_way);
        }

        m_n_unknown_frames++;
    }
    else
    {
        const float scene_max_separation = m_applier.get_scene_max_separation(in_shape_key);

        m_convergence_model.follow_scene_max_separation(scene_max_separation,
                                                        fraction_of_the_way);
    }
}

bool Stereo::Core::RecipeStereoPlanner::on_frame_depth(const uint64_t&   in_shape_key,
                                                       const DepthRange& in_depth_range,
                                                       const float&      in_projection_scale)
{
    const bool result = in_depth_range.has_depth;

    if (result)
    {
        const auto limits = Analysis::Recipe::RecipeApplier::get_comfort_limits(m_applier.get_comfort           (),
                                                                                m_applier.find_scene_comfort_ptr(in_shape_key),
                                                                                in_depth_range.median_depth,
                                                                                in_depth_range.near_depth,
                                                                                in_depth_range.far_depth,
                                                                                in_projection_scale);

        const float fraction_of_the_way = get_fraction_of_the_way();

        m_convergence_model.follow_scene_convergence_distance(limits.distance,
                                                              fraction_of_the_way);
        m_convergence_model.follow_scene_max_separation      (limits.max_separation,
                                                              fraction_of_the_way);
    }

    return result;
}

bool Stereo::Core::RecipeStereoPlanner::on_matrix_call(const APIInterceptor::APIFunction&                      in_function,
                                                       const std::vector<APIInterceptor::APIFunctionArgument>& in_argument_vec,
                                                       const std::array<float, 16>&                            in_matrix)
{
    return m_matrix_slots.on_matrix_call(m_applier,
                                         in_function,
                                         in_argument_vec,
                                         in_matrix);
}

void Stereo::Core::RecipeStereoPlanner::on_view_matrix_set(const std::array<float, 16>& in_matrix)
{
    m_view_matrix_fp32vec16 = in_matrix;
}

float Stereo::Core::RecipeStereoPlanner::get_eye_parallax(const float& in_origin_depth,
                                                          const float& in_projection_x_scale,
                                                          const bool&  in_is_perspective) const
{
    constexpr float HALF = 0.5f;

    const float distance   = m_convergence_model.get_effective_convergence_distance();
    const float separation = m_convergence_model.get_effective_eye_separation      ();
    float       result     = 0.0f;

    if (in_origin_depth < 0.0f &&
        distance        > 0.0f)
    {
        /* The left eye's shift of a point at this depth is half the separation, reduced by what the convergence takes back. In normalized
         * device units the picture is 2 wide, so this shift equals the distance between the two eyes' images as a fraction of the picture
         * width.
         */
        const float shift_in_units = separation * HALF * (1.0f + in_origin_depth / distance);

        result = (in_is_perspective) ? shift_in_units * in_projection_x_scale / -in_origin_depth
                                     : shift_in_units * in_projection_x_scale;
    }

    return result;
}

float Stereo::Core::RecipeStereoPlanner::plan_follow_shift(const Analysis::Recipe::Eye& in_eye,
                                                           const float&                 in_picture_width_pixels,
                                                           const float&                 in_origin_depth,
                                                           const float&                 in_projection_x_scale,
                                                           const bool&                  in_is_perspective) const
{
    constexpr float HALF = 0.5f;

    float result = 0.0f;

    if (m_is_last_draw_part)
    {
        /* Each eye moves by half the parallax: the left eye to the right, the right eye to the left. */
        const float shift_in_ndc = get_eye_parallax(in_origin_depth,
                                                    in_projection_x_scale,
                                                    in_is_perspective);

        result = ((in_eye == Analysis::Recipe::Eye::LEFT) ? shift_in_ndc
                                                          : -shift_in_ndc) * in_picture_width_pixels * HALF;
    }

    return result;
}

float Stereo::Core::RecipeStereoPlanner::plan_part_shift(const Analysis::Recipe::Eye& in_eye,
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
        const auto& comfort    = m_applier.get_comfort           ();
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

float Stereo::Core::RecipeStereoPlanner::plan_push_shift(const Analysis::Recipe::Eye& in_eye,
                                                         const float&                 in_picture_width_pixels) const
{
    return plan_push_shift(in_eye,
                           in_picture_width_pixels,
                           (m_last_draw_class == Analysis::Recipe::DrawClass::AFFECTED) );
}

float Stereo::Core::RecipeStereoPlanner::plan_push_shift_with_layer(const Analysis::Recipe::Eye& in_eye,
                                                                    const float&                 in_picture_width_pixels,
                                                                    const float&                 in_layer_parallax) const
{
    return PushShiftPlanner::plan_with_layer(m_applier,
                                             in_eye,
                                             m_applier.get_cover_push_parallax(m_last_draw_cover_key),
                                             in_layer_parallax,
                                             in_picture_width_pixels);
}

float Stereo::Core::RecipeStereoPlanner::plan_push_shift(const Analysis::Recipe::Eye& in_eye,
                                                         const float&                 in_picture_width_pixels,
                                                         const bool&                  in_is_moved_by_eyes) const
{
    const float in_parallax = m_applier.get_cover_push_parallax(m_last_draw_cover_key);

    return PushShiftPlanner::plan(m_applier,
                                  in_eye,
                                  in_parallax,
                                  in_is_moved_by_eyes,
                                  in_picture_width_pixels,
                                  m_convergence_model.get_effective_eye_separation(),
                                  0.0f /* the effective separation already respects the scene limit */);
}

Stereo::Core::ScreenShiftPlan Stereo::Core::RecipeStereoPlanner::plan_screen_shift(const Analysis::Recipe::Eye& in_eye,
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
    input.is_overlay_pushed    = get_is_last_draw_pushed();
    input.is_presented         = in_is_presented;
    input.picture_width        = in_picture_width;
    input.reaches_both_sides   = in_reaches_both_sides;
    input.separation_limit     = 0.0f; /* the effective separation already respects the scene limit */

    return ScreenParallaxPlanner::plan(m_applier,
                                       input);
}

void Stereo::Core::RecipeStereoPlanner::require_fast_path(const bool&        in_is_ok,
                                                          const std::string& in_reason) const
{
    /* Limitation: an application that fails here needs the calls of a pass to be held back until the pass is known. The converter has no
     * such deferred path.
     */
    if (in_is_ok == false)
    {
        fail(in_reason                                                                                                   +
             ". This application needs the converter to hold the calls of a pass back until it knows which pass it is, "
             "which the converter cannot do");
    }
}

void Stereo::Core::RecipeStereoPlanner::require_supported(const bool&        in_is_ok,
                                                          const std::string& in_reason) const
{
    if (in_is_ok == false)
    {
        fail(in_reason);
    }
}

void Stereo::Core::RecipeStereoPlanner::set_convergence_distance(const float& in_convergence_distance)
{
    if (m_convergence_model.set_convergence_distance(in_convergence_distance) == false)
    {
        fail("The convergence distance must be zero (none) or more");
    }
}

void Stereo::Core::RecipeStereoPlanner::set_depth_strength(const float& in_depth_strength)
{
    if (m_convergence_model.set_depth_strength(in_depth_strength) == false)
    {
        fail("The strength of the 3D must be zero or more");
    }
}

void Stereo::Core::RecipeStereoPlanner::set_eye_separation(const float& in_eye_separation)
{
    if (m_convergence_model.set_eye_separation(in_eye_separation) == false)
    {
        fail("The eye separation must be zero or more");
    }
}

void Stereo::Core::RecipeStereoPlanner::set_frame_interval_seconds(const float& in_frame_interval_seconds)
{
    m_frame_interval_seconds = in_frame_interval_seconds;
}

void Stereo::Core::RecipeStereoPlanner::set_ipd_scale(const float& in_ipd_scale)
{
    if (m_convergence_model.set_ipd_scale(in_ipd_scale) == false)
    {
        fail("The scale of the distance between the eyes of the person must be more than zero");
    }
}

void Stereo::Core::RecipeStereoPlanner::set_is_overlay_pushed(const bool& in_is_pushed)
{
    m_is_overlay_pushed = in_is_pushed;
}

void Stereo::Core::RecipeStereoPlanner::set_scene_shape_key(const uint64_t& in_shape_key)
{
    m_scene_shape_key = in_shape_key;
}
