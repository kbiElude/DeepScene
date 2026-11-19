/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/recipe/recipe_applier.h"
#include <algorithm>
#include <math.h>

Analysis::Recipe::RecipeApplier::RecipeApplier(const Analysis::Recipe::Recipe& in_recipe)
    :m_recipe(in_recipe)
{
    for (const auto& rule : m_recipe.rule_vec)
    {
        m_rule_function_set.insert(rule.function);
    }

    for (const auto& convergence : m_recipe.convergence_vec)
    {
        m_convergence_distance_map[convergence.signature_key] = convergence.distance;
    }

    for (const auto& scene_convergence : m_recipe.scene_convergence_vec)
    {
        m_scene_convergence_distance_map[scene_convergence.shape_key] = scene_convergence.distance;
    }

    for (const auto& scene_separation : m_recipe.scene_separation_vec)
    {
        m_scene_max_separation_map[scene_separation.shape_key] = scene_separation.max_separation;
    }

    for (uint32_t n_rule = 0;
                  n_rule < m_recipe.draw_rule_vec.size();
                ++n_rule)
    {
        m_draw_rule_index_map[m_recipe.draw_rule_vec.at(n_rule).signature_key].push_back(n_rule);
    }

    m_affected_draw_key_set.insert  (m_recipe.affected.draw_key_vec.begin  (),
                                     m_recipe.affected.draw_key_vec.end    () );
    m_affected_pass_key_set.insert  (m_recipe.affected.pass_key_vec.begin  (),
                                     m_recipe.affected.pass_key_vec.end    () );

    for (const auto& pass_draw : m_recipe.affected.pass_draw_vec)
    {
        m_affected_pass_draw_set.insert(PassDrawKey(pass_draw.pass_key,
                                                    pass_draw.draw_key) );
    }

    for (const auto& pass_draw : m_recipe.pushed.pass_draw_vec)
    {
        m_pushed_pass_draw_set.insert(PassDrawKey(pass_draw.pass_key,
                                                  pass_draw.draw_key) );
    }

    for (const auto& pass_draw : m_recipe.skipped.pass_draw_vec)
    {
        m_skipped_pass_draw_set.insert(PassDrawKey(pass_draw.pass_key,
                                                   pass_draw.draw_key) );
    }

    for (const auto& pass_draw : m_recipe.unaffected.pass_draw_vec)
    {
        m_unaffected_pass_draw_set.insert(PassDrawKey(pass_draw.pass_key,
                                                      pass_draw.draw_key) );
    }

    m_covering_draw_key_set.insert  (m_recipe.covering_draw_key_vec.begin          (),
                                     m_recipe.covering_draw_key_vec.end            () );
    m_eye_space_cover_key_set.insert(m_recipe.comfort.eye_space_cover_key_vec.begin(),
                                     m_recipe.comfort.eye_space_cover_key_vec.end  () );

    if (m_recipe.comfort.uses_automatic_layers)
    {
        m_automatic_flat_layer_cover_key_set.insert(m_recipe.flat_layer_draw_key_vec.begin(),
                                                    m_recipe.flat_layer_draw_key_vec.end  () );
        m_automatic_overlay_cover_key_set.insert   (m_recipe.overlay_draw_key_vec.begin   (),
                                                    m_recipe.overlay_draw_key_vec.end     () );
        m_eye_space_cover_key_set.insert           (m_recipe.eye_space_draw_key_vec.begin (),
                                                    m_recipe.eye_space_draw_key_vec.end   () );
    }

    m_known_signature_key_set.insert   (m_recipe.known_signature_key_vec.begin   (),
                                        m_recipe.known_signature_key_vec.end     () );
    m_pushed_cover_key_set.insert      (m_recipe.pushed.cover_key_vec.begin      (),
                                        m_recipe.pushed.cover_key_vec.end        () );
    m_pushed_part_cover_key_set.insert (m_recipe.pushed.part_cover_key_vec.begin (),
                                        m_recipe.pushed.part_cover_key_vec.end   () );
    m_pushed_draw_key_set.insert       (m_recipe.pushed.draw_key_vec.begin       (),
                                        m_recipe.pushed.draw_key_vec.end         () );
    m_pushed_pass_key_set.insert       (m_recipe.pushed.pass_key_vec.begin       (),
                                        m_recipe.pushed.pass_key_vec.end         () );
    m_side_reaching_draw_key_set.insert(m_recipe.side_reaching_draw_key_vec.begin(),
                                        m_recipe.side_reaching_draw_key_vec.end  () );
    m_skipped_draw_key_set.insert      (m_recipe.skipped.draw_key_vec.begin      (),
                                        m_recipe.skipped.draw_key_vec.end        () );
    m_skipped_pass_key_set.insert      (m_recipe.skipped.pass_key_vec.begin      (),
                                        m_recipe.skipped.pass_key_vec.end        () );
    m_unaffected_draw_key_set.insert   (m_recipe.unaffected.draw_key_vec.begin   (),
                                        m_recipe.unaffected.draw_key_vec.end     () );
    m_unaffected_pass_key_set.insert   (m_recipe.unaffected.pass_key_vec.begin   (),
                                        m_recipe.unaffected.pass_key_vec.end     () );
}

bool Analysis::Recipe::RecipeApplier::apply_overlay_shift(const Analysis::Recipe::MatrixRule& in_rule,
                                                          const Analysis::Recipe::Eye&        in_eye,
                                                          const float&                        in_overlay_parallax,
                                                          std::array<float, 16>*              inout_matrix_ptr)
{
    constexpr size_t SHEAR_INDEX = 8; /* row 2, column 0: what the depth adds to x */

    bool is_perspective = false;
    bool result         = false;

    if (in_rule.action_kind == Analysis::Recipe::ActionKind::EYE_CONVERGENCE)
    {
        is_perspective = is_matrix_of_kind(*inout_matrix_ptr,
                                           Analysis::Recipe::MatrixKind::PERSPECTIVE);
    }

    if (is_perspective)
    {
        /* The picture is two units wide: +p for the left picture and -p for the right puts them p of the width apart. The left picture
         * moves right, as for a near object.
         */
        inout_matrix_ptr->at(SHEAR_INDEX) += (in_eye == Analysis::Recipe::Eye::LEFT) ?  in_overlay_parallax
                                                                                     : -in_overlay_parallax;

        result = true;
    }

    return result;
}

bool Analysis::Recipe::RecipeApplier::apply_rule(const Analysis::Recipe::MatrixRule& in_rule,
                                                 const Analysis::Recipe::Eye&        in_eye,
                                                 const float&                        in_eye_separation,
                                                 std::array<float, 16>*              inout_matrix_ptr,
                                                 const float&                        in_convergence_distance)
{
    constexpr size_t SCALE_INDEX = 0;
    constexpr size_t SHEAR_INDEX = 8; /* row 2, column 0: what the depth adds to x */

    const bool                  is_offset     = (in_rule.action_kind == Analysis::Recipe::ActionKind::EYE_OFFSET);
    const std::array<float, 16> offset_matrix = make_eye_offset_matrix(in_rule.offset_axis,
                                                                       in_eye,
                                                                       in_eye_separation);
    const auto&                 recipe_matrix = (is_offset)                             ? offset_matrix
                                              : (in_eye == Analysis::Recipe::Eye::LEFT) ? in_rule.left_matrix_fp32vec16
                                                                                        : in_rule.right_matrix_fp32vec16;
    bool                        result        = false;

    if (is_matrix_of_kind(*inout_matrix_ptr,
                          in_rule.matrix_kind) )
    {
        if (in_rule.action_kind == Analysis::Recipe::ActionKind::EYE_CONVERGENCE)
        {
            /* Only a perspective projection has a depth to converge at. */
            if (is_matrix_of_kind(*inout_matrix_ptr,
                                  Analysis::Recipe::MatrixKind::PERSPECTIVE) )
            {
                /* The view moves the scene right for the left eye; the shear moves it back at the convergence distance. */
                if (in_convergence_distance > 0.0f)
                {
                    const float half_separation = ((in_eye == Analysis::Recipe::Eye::LEFT) ?  0.5f
                                                                                           : -0.5f) * in_eye_separation;

                    inout_matrix_ptr->at(SHEAR_INDEX) -= half_separation * inout_matrix_ptr->at(SCALE_INDEX) / in_convergence_distance;
                }

                result = true;
            }
        }
        else
        {
            if (in_rule.action_kind == Analysis::Recipe::ActionKind::REPLACE)
            {
                *inout_matrix_ptr = recipe_matrix;
            }
            else
            if (in_rule.multiply_order == Analysis::Recipe::MultiplyOrder::POST)
            {
                *inout_matrix_ptr = multiply(*inout_matrix_ptr,
                                             recipe_matrix);
            }
            else
            {
                *inout_matrix_ptr = multiply(recipe_matrix,
                                             *inout_matrix_ptr);
            }

            result = true;
        }
    }

    return result;
}

Analysis::Recipe::DrawClass Analysis::Recipe::RecipeApplier::classify_draw(const uint64_t&              in_signature_key,
                                                                           const std::array<float, 16>& in_view_matrix) const
{
    Analysis::Recipe::DrawClass result   = Analysis::Recipe::DrawClass::AFFECTED;
    const auto*                 rule_ptr = find_draw_rule_ptr(in_signature_key,
                                                              in_view_matrix);

    if (rule_ptr != nullptr)
    {
        result = rule_ptr->draw_class;
    }

    return result;
}

Analysis::Recipe::DrawDecision Analysis::Recipe::RecipeApplier::decide_draw(const Analysis::Recipe::DrawQuery& in_query) const
{
    const bool   is_cover_flat      = is_cover_flat_layer(in_query.cover_key);
    const bool   is_pushed_by_cover = is_cover_pushed(in_query.cover_key);
    DrawDecision result;

    if (in_query.has_pass)
    {
        result.draw_class = get_named_draw_class(in_query.pass_key,
                                                 in_query.draw_key);
        result.is_pushed  = is_draw_pushed      (in_query.pass_key,
                                                 in_query.draw_key);
    }
    else
    {
        result.draw_class = classify_draw      (in_query.signature_key,
                                                in_query.view_matrix);
        result.is_pushed  = is_signature_pushed(in_query.signature_key,
                                                in_query.view_matrix);
    }

    /* A baked flat layer is left alone, and so is a draw pushed by its cover key (unless the caller waives that). */
    const bool is_held = (is_cover_flat ||
                          (is_pushed_by_cover && in_query.is_cover_hold_waived == false) );

    if (result.draw_class == DrawClass::AFFECTED &&
        is_held)
    {
        result.draw_class = DrawClass::UNAFFECTED;
    }

    if (is_pushed_by_cover)
    {
        result.is_pushed = true;
    }

    /* A recipe for an application whose left-alone draws are all an overlay pushes them without listing them. */
    if (m_recipe.comfort.pushes_unaffected &&
        result.draw_class != DrawClass::AFFECTED)
    {
        result.is_pushed = true;
    }

    return result;
}

const Analysis::Recipe::DrawRule* Analysis::Recipe::RecipeApplier::find_draw_rule_ptr(const uint64_t&              in_signature_key,
                                                                                      const std::array<float, 16>& in_view_matrix) const
{
    const auto                        index_iterator = m_draw_rule_index_map.find(in_signature_key);
    const Analysis::Recipe::DrawRule* result_ptr     = nullptr;

    if (index_iterator != m_draw_rule_index_map.end() )
    {
        for (const auto& n_rule : index_iterator->second)
        {
            const auto& rule       = m_recipe.draw_rule_vec.at (n_rule);
            bool        is_applied = rule.view_matrix_vec.empty();

            for (const auto& view_matrix : rule.view_matrix_vec)
            {
                is_applied = is_applied || (view_matrix == in_view_matrix);
            }

            if (is_applied)
            {
                result_ptr = &rule;

                break;
            }
        }
    }

    return result_ptr;
}

const Analysis::Recipe::SceneComfort* Analysis::Recipe::RecipeApplier::find_scene_comfort_ptr(const uint64_t& in_shape_key) const
{
    const SceneComfort* result_ptr = nullptr;

    for (const auto& scene_comfort : m_recipe.comfort.scene_override_vec)
    {
        if (scene_comfort.shape_key == in_shape_key)
        {
            result_ptr = &scene_comfort;
        }
    }

    return result_ptr;
}

bool Analysis::Recipe::RecipeApplier::get_caches_pixel_reads() const
{
    return m_recipe.caches_pixel_reads;
}

const Analysis::Recipe::Comfort& Analysis::Recipe::RecipeApplier::get_comfort() const
{
    return m_recipe.comfort;
}

Analysis::Recipe::ComfortLimits Analysis::Recipe::RecipeApplier::get_comfort_limits(const Comfort&      in_comfort,
                                                                                    const SceneComfort* in_scene_ptr,
                                                                                    const float&        in_median_depth,
                                                                                    const float&        in_near_depth,
                                                                                    const float&        in_far_depth,
                                                                                    const float&        in_projection_scale)
{
    ComfortLimits result;

    const float scale = (in_projection_scale > 0.0f) ? in_projection_scale
                                                     : 1.0f;

    result.distance = (in_scene_ptr != nullptr                   &&
                       in_scene_ptr->convergence_distance > 0.0f) ? in_scene_ptr->convergence_distance
                                                                  : in_median_depth;

    const float max_far_parallax  = (in_scene_ptr                    != nullptr &&
                                     in_scene_ptr->max_far_parallax  >  0.0f)   ? in_scene_ptr->max_far_parallax
                                                                                : in_comfort.max_far_parallax;
    const float max_near_parallax = (in_scene_ptr                    != nullptr &&
                                     in_scene_ptr->max_near_parallax >  0.0f)   ? in_scene_ptr->max_near_parallax
                                                                                : in_comfort.max_near_parallax;

    /* With both limits, d splits the total parallax s * scale * (1 / zn - 1 / zf) / 2 in the ratio of the limits, so the nearest
     * surfaces get exactly the near limit: 1 / d = 1 / zn - (1 / zn - 1 / zf) * near / (near + far).
     */
    if (in_comfort.balances_convergence                                                           &&
        (in_scene_ptr                   == nullptr || in_scene_ptr->convergence_distance <= 0.0f) &&
        max_near_parallax               >  0.0f                                                   &&
        max_far_parallax                >  0.0f                                                   &&
        in_near_depth                   >  0.0f                                                   &&
        in_far_depth                    >  in_near_depth)
    {
        const float depth_term    = 1.0f / in_near_depth - 1.0f                           / in_far_depth;
        const float inverse_depth = 1.0f / in_near_depth - depth_term * max_near_parallax / (max_near_parallax + max_far_parallax);

        result.distance = 1.0f / inverse_depth;
    }

    /* A surface at depth z is s * scale * (1 / z - 1 / d) / 2 of the picture width apart in the two pictures (zero at z = d). The limit
     * is the s that makes this max_near_parallax at the nearest z. For the farthest surface use (1 / d - 1 / z), which never exceeds 1 / d.
     */
    if (max_near_parallax > 0.0f           &&
        in_near_depth     > 0.0f           &&
        in_near_depth     < result.distance)
    {
        result.max_separation = 2.0f * max_near_parallax / (scale * (1.0f / in_near_depth - 1.0f / result.distance));
    }

    if (max_far_parallax > 0.0f &&
        result.distance  > 0.0f)
    {
        const float far_term  = (in_far_depth > result.distance) ? (1.0f / result.distance - 1.0f / in_far_depth)
                                                                 : 0.0f;
        const float far_limit = (far_term > 0.0f)                ? 2.0f * max_far_parallax / (scale * far_term)
                                                                 : 0.0f;

        if (far_limit              >  0.0f                   &&
            (result.max_separation <= 0.0f                   ||
             far_limit             <  result.max_separation) )
        {
            result.is_far_limit   = true;
            result.max_separation = far_limit;
        }
    }

    if (in_scene_ptr                 != nullptr &&
        in_scene_ptr->max_separation >  0.0f)
    {
        result.is_far_limit   = false;
        result.max_separation = in_scene_ptr->max_separation;
    }

    return result;
}

float Analysis::Recipe::RecipeApplier::get_convergence_distance(const uint64_t& in_signature_key) const
{
    const auto distance_iterator = m_convergence_distance_map.find(in_signature_key);

    return (distance_iterator != m_convergence_distance_map.end() ) ? distance_iterator->second
                                                                    : 0.0f;
}

float Analysis::Recipe::RecipeApplier::get_cover_push_parallax(const uint64_t& in_cover_key) const
{
    const bool is_part = (m_pushed_part_cover_key_set.count(in_cover_key) != 0     &&
                          m_recipe.comfort.part_parallax                  >  0.0f);

    return (is_part) ? m_recipe.comfort.part_parallax
                     : m_recipe.comfort.overlay_parallax;
}

std::vector<const Analysis::Recipe::MatrixRule*> Analysis::Recipe::RecipeApplier::get_matching_rule_ptr_vec(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                                                            const CallPlace&                             in_place) const
{
    std::vector<const Analysis::Recipe::MatrixRule*> result_vec;

    for (const auto& rule : m_recipe.rule_vec)
    {
        bool is_match = (rule.function == in_call.func);

        for (const auto& condition : rule.argument_condition_vec)
        {
            const bool has_argument      = (condition.argument_index < in_call.arg_in_vec.size() );
            bool       is_argument_match = false;

            if (has_argument)
            {
                const auto& argument = in_call.arg_in_vec.at(condition.argument_index);

                if (argument.type == APIInterceptor::ARGTYPE_U32)
                {
                    const uint32_t argument_value = argument.get_u32();

                    is_argument_match = (argument_value == condition.value);
                }
            }

            is_match = (is_match &&
                        is_argument_match);
        }

        if (is_match                          &&
            rule.pass_key_vec.empty() == false)
        {
            is_match = false;

            if (in_place.has_pass)
            {
                const auto pass_iterator = std::find(rule.pass_key_vec.begin(),
                                                     rule.pass_key_vec.end  (),
                                                     in_place.pass_key);

                is_match = (pass_iterator != rule.pass_key_vec.end() );
            }
        }

        if (is_match                            &&
            rule.shape_key_vec.empty() == false)
        {
            is_match = (std::find(rule.shape_key_vec.begin(),
                                  rule.shape_key_vec.end  (),
                                  in_place.shape_key)        != rule.shape_key_vec.end() );
        }

        if (is_match)
        {
            result_vec.push_back(&rule);
        }
    }

    return result_vec;
}

Analysis::Recipe::DrawClass Analysis::Recipe::RecipeApplier::get_named_draw_class(const uint64_t& in_pass_key,
                                                                                  const uint64_t& in_draw_key) const
{
    DrawClass result = DrawClass::UNAFFECTED;

    if (is_draw_skipped(in_pass_key,
                        in_draw_key) )
    {
        result = DrawClass::SKIPPED;
    }
    else
    if (is_draw_affected(in_pass_key,
                         in_draw_key) )
    {
        result = DrawClass::AFFECTED;
    }

    return result;
}

float Analysis::Recipe::RecipeApplier::get_overlay_parallax() const
{
    return m_recipe.comfort.overlay_parallax;
}

bool Analysis::Recipe::RecipeApplier::get_records_immediate_mode_blocks() const
{
    return m_recipe.records_immediate_mode_blocks;
}

bool Analysis::Recipe::RecipeApplier::get_renders_left_then_right_eye() const
{
    return m_recipe.renders_left_then_right_eye;
}

float Analysis::Recipe::RecipeApplier::get_scene_convergence_distance(const uint64_t& in_shape_key) const
{
    const auto distance_iterator = m_scene_convergence_distance_map.find(in_shape_key);

    return (distance_iterator != m_scene_convergence_distance_map.end() ) ? distance_iterator->second
                                                                          : 0.0f;
}

float Analysis::Recipe::RecipeApplier::get_scene_max_separation(const uint64_t& in_shape_key) const
{
    const auto separation_iterator = m_scene_max_separation_map.find(in_shape_key);

    return (separation_iterator != m_scene_max_separation_map.end() ) ? separation_iterator->second
                                                                      : 0.0f;
}

const Analysis::Recipe::ScreenParallaxSettings& Analysis::Recipe::RecipeApplier::get_screen_parallax() const
{
    return m_recipe.screen_parallax;
}

Analysis::Recipe::UnknownFramePolicy Analysis::Recipe::RecipeApplier::get_unknown_frame_policy() const
{
    return m_recipe.unknown_frame_policy;
}

const Analysis::Recipe::ViewAnchorShift& Analysis::Recipe::RecipeApplier::get_view_anchor_shift() const
{
    return m_recipe.view_anchor_shift;
}

bool Analysis::Recipe::RecipeApplier::has_rule_for(const APIInterceptor::APIFunction& in_function) const
{
    return (m_rule_function_set.find(in_function) != m_rule_function_set.end() );
}

bool Analysis::Recipe::RecipeApplier::has_view_anchor_shift() const
{
    return m_recipe.view_anchor_shift.is_enabled;
}

bool Analysis::Recipe::RecipeApplier::is_cover_flat_layer(const uint64_t& in_cover_key) const
{
    return (m_automatic_flat_layer_cover_key_set.count(in_cover_key) != 0);
}

bool Analysis::Recipe::RecipeApplier::is_cover_pushed(const uint64_t& in_cover_key) const
{
    return (m_pushed_cover_key_set.count           (in_cover_key) != 0 ||
            m_pushed_part_cover_key_set.count      (in_cover_key) != 0 ||
            m_automatic_overlay_cover_key_set.count(in_cover_key) != 0);
}

bool Analysis::Recipe::RecipeApplier::is_draw_affected(const uint64_t& in_pass_key,
                                                       const uint64_t& in_draw_key) const
{
    bool result = true;

    /* The most specific name wins, in this order: a draw in its pass, a draw, a pass. */
    if (m_affected_pass_draw_set.count(PassDrawKey(in_pass_key,
                                                   in_draw_key) ) != 0)
    {
        result = true;
    }
    else
    if (m_unaffected_pass_draw_set.count(PassDrawKey(in_pass_key,
                                                     in_draw_key) ) != 0)
    {
        result = false;
    }
    else
    if (m_affected_draw_key_set.count(in_draw_key) != 0)
    {
        result = true;
    }
    else
    if (m_unaffected_draw_key_set.count(in_draw_key) != 0)
    {
        result = false;
    }
    else
    if (m_affected_pass_key_set.count(in_pass_key) != 0)
    {
        result = true;
    }
    else
    if (m_unaffected_pass_key_set.count(in_pass_key) != 0)
    {
        result = false;
    }

    return result;
}

bool Analysis::Recipe::RecipeApplier::is_draw_covering(const uint64_t& in_cover_key) const
{
    return (m_covering_draw_key_set.count(in_cover_key) != 0);
}

bool Analysis::Recipe::RecipeApplier::is_draw_pushed(const uint64_t& in_pass_key,
                                                     const uint64_t& in_draw_key) const
{
    const PassDrawKey pass_draw_key(in_pass_key,
                                    in_draw_key);

    return (m_pushed_pass_draw_set.count(pass_draw_key) != 0 ||
            m_pushed_draw_key_set.count (in_draw_key)   != 0 ||
            m_pushed_pass_key_set.count (in_pass_key)   != 0);
}

bool Analysis::Recipe::RecipeApplier::is_draw_skipped(const uint64_t& in_pass_key,
                                                      const uint64_t& in_draw_key) const
{
    const PassDrawKey pass_draw_key(in_pass_key,
                                    in_draw_key);

    const size_t n_draw_skips      = m_skipped_draw_key_set.count(in_draw_key);
    const size_t n_pass_draw_skips = m_skipped_pass_draw_set.count(pass_draw_key);
    const size_t n_pass_skips      = m_skipped_pass_key_set.count(in_pass_key);

    return (n_pass_draw_skips != 0 ||
            n_draw_skips      != 0 ||
            n_pass_skips      != 0);
}

bool Analysis::Recipe::RecipeApplier::is_eye_space_cover(const uint64_t& in_cover_key) const
{
    return (m_eye_space_cover_key_set.count(in_cover_key) != 0);
}

bool Analysis::Recipe::RecipeApplier::is_frame_known(const uint64_t& in_shape_key) const
{
    const auto known_iterator = std::find(m_recipe.known_shape_key_vec.begin(),
                                          m_recipe.known_shape_key_vec.end  (),
                                          in_shape_key);

    return (m_recipe.known_shape_key_vec.empty() ||
            known_iterator != m_recipe.known_shape_key_vec.end() );
}

bool Analysis::Recipe::RecipeApplier::is_matrix_of_kind(const std::array<float, 16>&        in_matrix,
                                                        const Analysis::Recipe::MatrixKind& in_kind)
{
    constexpr float TOLERANCE = 1e-6f;

    /* The last column of a perspective projection is (0, 0, 1, 0), and that of an orthographic one (0, 0, 0, 1). */
    const auto is_column_func = [&](const float& in_z,
                                    const float& in_w)
    {
        const float x_residual = ::fabsf(in_matrix.at(3) );
        const float y_residual = ::fabsf(in_matrix.at(7) );
        const float z_residual = ::fabsf(in_matrix.at(11) - in_z);
        const float w_residual = ::fabsf(in_matrix.at(15) - in_w);

        return (x_residual <= TOLERANCE &&
                y_residual <= TOLERANCE &&
                z_residual <= TOLERANCE &&
                w_residual <= TOLERANCE);
    };

    bool result = true;

    if (in_kind == Analysis::Recipe::MatrixKind::ORTHOGRAPHIC)
    {
        result = is_column_func(0.0f,
                                1.0f);
    }
    else
    if (in_kind == Analysis::Recipe::MatrixKind::PERSPECTIVE)
    {
        result = is_column_func(1.0f,
                                0.0f);
    }

    return result;
}

bool Analysis::Recipe::RecipeApplier::is_part_cover(const uint64_t& in_cover_key) const
{
    return (m_pushed_part_cover_key_set.count(in_cover_key) != 0);
}

bool Analysis::Recipe::RecipeApplier::is_per_eye_texture(const Analysis::APIDumpAnalysis::ResourceIdentity& in_identity) const
{
    bool result = false;

    for (const auto& texture : m_recipe.per_eye_texture_vec)
    {
        if (texture.description_key == in_identity.description_key &&
            texture.ordinal         == in_identity.ordinal)
        {
            result = true;

            break;
        }
    }

    return result;
}

bool Analysis::Recipe::RecipeApplier::is_side_reaching(const uint64_t& in_cover_key) const
{
    return (m_side_reaching_draw_key_set.count(in_cover_key) != 0);
}

bool Analysis::Recipe::RecipeApplier::is_signature_known(const uint64_t& in_signature_key) const
{
    const size_t n_matching_keys = m_known_signature_key_set.count(in_signature_key);

    return (m_known_signature_key_set.empty()       ||
            n_matching_keys                   != 0);
}

bool Analysis::Recipe::RecipeApplier::is_signature_pushed(const uint64_t&              in_signature_key,
                                                          const std::array<float, 16>& in_view_matrix) const
{
    const auto* rule_ptr = find_draw_rule_ptr(in_signature_key,
                                              in_view_matrix);

    return (rule_ptr != nullptr &&
            rule_ptr->is_pushed);
}

std::array<float, 16> Analysis::Recipe::RecipeApplier::make_eye_offset_matrix(const Analysis::Recipe::Axis& in_axis,
                                                                              const Analysis::Recipe::Eye&  in_eye,
                                                                              const float&                  in_eye_separation)
{
    constexpr size_t N_COLUMNS      = 4;
    constexpr size_t TRANSLATION_ROW = 3;

    /* A view matrix moves the scene, not the camera. For the left eye, which looks from further to the left, the scene moves toward the right. */
    const float           half_separation = ((in_eye == Analysis::Recipe::Eye::LEFT) ?  0.5f
                                                                                     : -0.5f) * in_eye_separation;
    std::array<float, 16> result          = {1, 0, 0, 0,
                                             0, 1, 0, 0,
                                             0, 0, 1, 0,
                                             0, 0, 0, 1};

    result.at(TRANSLATION_ROW * N_COLUMNS + static_cast<size_t>(in_axis) ) = half_separation;

    return result;
}

std::array<float, 16> Analysis::Recipe::RecipeApplier::make_view_anchor_matrix(const Analysis::Recipe::Eye& in_eye,
                                                                               const float&                 in_eye_separation,
                                                                               const uint64_t&              in_shape_key) const
{
    constexpr float HALF = 0.5f;

    const float scene_limit    = get_scene_max_separation(in_shape_key);
    const float scene_distance = get_scene_convergence_distance(in_shape_key);

    const float separation = (scene_limit > 0.0f                &&
                              scene_limit < in_eye_separation) ? scene_limit
                                                               : in_eye_separation;
    const float distance   = (scene_distance > 0.0f)            ? scene_distance
                                                               : m_recipe.view_anchor_shift.convergence_distance;

    /* For the left eye the scene moves toward the right, because the camera is to the left. */
    const float half_shift = (in_eye == Analysis::Recipe::Eye::LEFT) ? separation * HALF
                                                                    : -separation * HALF;

    std::array<float, 16> result =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    /* The shift is x + h + (h / d) * z. The translation h is the fourth column, and the shear h / d is what the depth z adds to x. */
    result.at(12) = half_shift;

    if (distance > 0.0f)
    {
        result.at(8) = half_shift / distance;
    }

    return result;
}

std::array<float, 16> Analysis::Recipe::RecipeApplier::multiply(const std::array<float, 16>& in_a,
                                                                const std::array<float, 16>& in_b)
{
    constexpr size_t N_COLUMNS = 4;

    std::array<float, 16> result = {};

    for (size_t n_row = 0;
                n_row < N_COLUMNS;
              ++n_row)
    {
        for (size_t n_column = 0;
                    n_column < N_COLUMNS;
                  ++n_column)
        {
            for (size_t n_term = 0;
                        n_term < N_COLUMNS;
                      ++n_term)
            {
                result.at(n_row * N_COLUMNS + n_column) += in_a.at(n_row * N_COLUMNS + n_term) * in_b.at(n_term * N_COLUMNS + n_column);
            }
        }
    }

    return result;
}
