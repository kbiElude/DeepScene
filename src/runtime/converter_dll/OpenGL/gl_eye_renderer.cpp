/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "common/argument_reader.h"
#include "common/callbacks.h"
#include "gl_eye_renderer.h"
#include "gl_driver_functions.h"
#include "analysis/api_dump_analysis/draw_measurer.h"
#include "capture/api_dump_loader/api_info.h"
#include "capture/api_dump_names/function_names.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_matrix_math.h"
#include "own_call_scope.h"
#include "stereo/core/push_shift_planner.h"
#include <algorithm>
#include <cmath>
#include <cstring>

Stereo::ConverterDLL::GLEyeRenderer::GLEyeRenderer(Stereo::Core::RecipeStereoPlanner* in_core_ptr,
                                                   const bool&                        in_is_shown_elsewhere,
                                                   const uint32_t&                    in_target_width,
                                                   const uint32_t&                    in_target_height)
    :m_bound_texture_array(),
     m_active_unit        (0),
     m_applied_height     (0),
     m_applied_width      (0),
     m_block_class        (Analysis::Recipe::DrawClass::UNAFFECTED),
     m_block_has_draws    (false),
     m_block_is_pushed    (false),
     m_block_list         (0),
     m_caches_pixel_reads (in_core_ptr->get_caches_pixel_reads() ),
     m_core_ptr           (in_core_ptr),
     m_defers_right_eye   (false),
     m_depth_generation   (0),
     m_depth_lag          (0),
     m_fast_depth_range_measurer     (true),
     m_get_chunk_func     (),
     m_get_matrices_func  (),
     m_has_failed         (false),
     m_is_in_app_list     (false),
     m_is_in_block        (false),
     m_is_recording       (false),
     m_is_run_open        (false),
     m_is_shown_elsewhere (in_is_shown_elsewhere),
     m_matrix_mode_shadow (0x1700),
     m_measures_depth     (in_core_ptr->get_measures_depth() ),
     m_n_flushes          (0),
     m_n_failed_attempts  (0),
     m_n_tracked_matrix_reads(0),
     m_n_unanalyzed_vertices(0),
     m_pass_has_affected_draws(false),
     m_records_immediate_mode_blocks(false),
     m_scaler             (in_target_width,
                           in_target_height,
                           in_core_ptr,
                           [this](const std::string& in_message)
                           {
                               m_note_vec.push_back(in_message);
                           }),
     m_trusts_tracked_matrices(true)
{
    if (m_caches_pixel_reads)
    {
        m_note_vec.push_back("The recipe has the reads of one pixel that the application makes in a run given from a picture of the left eye that is read once (cache_pixel_reads)");
    }
}

void Stereo::ConverterDLL::GLEyeRenderer::apply_eye(const Analysis::Recipe::Eye& in_eye,
                                                    const EyePlan&               in_plan,
                                                    const std::function<void()>& in_draw_func)
{
    const EyeSetup setup = make_eye_setup(in_eye,
                                          in_plan);

    if (setup.has_matrix)
    {
        GLDriverFunctions::get().load_matrixf_func_ptr(setup.matrix.data() );
    }

    select_textures(in_plan.texture_selection,
                    in_eye == Analysis::Recipe::Eye::RIGHT);

    if (setup.has_viewport)
    {
        GLDriverFunctions::get().viewport_func_ptr(setup.viewport.at(0),
                                                   setup.viewport.at(1),
                                                   setup.viewport.at(2),
                                                   setup.viewport.at(3) );
    }

    in_draw_func();
}

void Stereo::ConverterDLL::GLEyeRenderer::collect_depth_reads(const bool& in_only_older_frames,
                                                              const bool& in_only_the_oldest)
{
    Stereo::Core::PendingDepthRead read;

    bool is_popped = m_depth_ledger.pop_pending(in_only_older_frames,
                                               &read);

    while (is_popped)
    {
        /* The depth is measured in place, in the buffer of the graphics card. The pass it belongs to is identified by the z scale and shift saved when the read started. */
        const auto depth_func = [this, &read](const float* in_depth_ptr, const size_t& in_n_depths)
        {
            const auto range = measure_depths(read.z_scale,
                                              read.z_shift,
                                              in_depth_ptr,
                                              in_n_depths);

            if (range.has_depth)
            {
                m_depth_ledger.add_result(read,
                                          range);
            }
        };

        m_targets.finish_depth_read(read.slot,
                                    depth_func);

        if (in_only_the_oldest)
        {
            is_popped = false;
        }
        else
        {
            is_popped = m_depth_ledger.pop_pending(in_only_older_frames,
                                                  &read);
        }
    }
}

void Stereo::ConverterDLL::GLEyeRenderer::copy_framebuffer(const bool&                   in_is_sub,
                                                           const uint32_t&               in_target,
                                                           const std::array<int32_t, 8>& in_value_array) const
{
    if (in_is_sub)
    {
        GLDriverFunctions::get().copy_tex_sub_image_2d_func_ptr(in_target,
                                                                in_value_array.at(1),
                                                                in_value_array.at(2),
                                                                in_value_array.at(3),
                                                                in_value_array.at(4),
                                                                in_value_array.at(5),
                                                                in_value_array.at(6),
                                                                in_value_array.at(7) );
    }
    else
    {
        GLDriverFunctions::get().copy_tex_image_2d_func_ptr(in_target,
                                                            in_value_array.at(1),
                                                            in_value_array.at(2),
                                                            in_value_array.at(3),
                                                            in_value_array.at(4),
                                                            in_value_array.at(5),
                                                            in_value_array.at(6),
                                                            in_value_array.at(7) );
    }
}

bool Stereo::ConverterDLL::GLEyeRenderer::defer_block(const Analysis::Recipe::DrawClass& in_draw_class,
                                                      const bool&                        in_is_pushed)
{
    const OwnCallScope own_call_scope;
    const EyePlan      plan   = make_eye_plan(in_draw_class,
                                              in_is_pushed,
                                              true); /* in_needs_matrices */
    const bool         result = plan.has_tracked_matrices;

    if (result)
    {
        const auto&    call_vec = m_block_recorder.get_calls();
        const EyeSetup setup    = make_eye_setup(Analysis::Recipe::Eye::RIGHT,
                                                 plan);
        RunDraw        draw;

        if (m_is_run_open == false)
        {
            open_run();
        }

        draw.has_pushed_viewport     = setup.has_viewport;
        draw.mode                    = m_block_recorder.get_mode();
        draw.modelview_f32mat        = (setup.has_matrix) ? setup.matrix
                                                          : plan.tracked_modelview;
        draw.n_calls                 = static_cast<uint32_t>(call_vec.size() );
        draw.projection_f32mat       = plan.tracked_projection;
        draw.pushed_viewport_i32vec4 = setup.viewport;
        draw.saved_viewport_i32vec4  = plan.saved_viewport;
        draw.selection               = plan.texture_selection;

        m_run_recorder.add_draw(draw,
                                call_vec.data() );

        const auto left_draw_func = [this]()
        {
            m_block_recorder.play();
        };

        apply_eye(Analysis::Recipe::Eye::LEFT,
                  plan,
                  left_draw_func);

        m_statistics.n_run_draws++;
    }

    finish_eye_plan(plan);

    return result;
}

void Stereo::ConverterDLL::GLEyeRenderer::fall_back_to_list(const APIInterceptor::APIFunction& in_function)
{
    const OwnCallScope own_call_scope;

    /* The block is drawn for both eyes when it ends, so what came before it is drawn for the right eye first. */
    flush_run();

    if (m_block_list == 0)
    {
        m_block_list = GLDriverFunctions::get().gen_lists_func_ptr(1);
    }

    GLDriverFunctions::get().new_list_func_ptr(m_block_list,
                                               GL_COMPILE);
    GLDriverFunctions::get().begin_func_ptr   (m_block_recorder.get_mode() );

    m_block_recorder.play_calls();

    m_is_in_block  = true;
    m_is_recording = false;

    m_statistics.n_fallback_blocks++;

    note_once("block_fallback_" + std::to_string(static_cast<uint32_t>(in_function) ),
              std::string("A block had a call that cannot be recorded (")                       +
              Capture::APIDumpNames::get_function_name(in_function)                              +
              "): the block was compiled into a display list instead");
}

Stereo::ConverterDLL::TextureSelection Stereo::ConverterDLL::GLEyeRenderer::find_texture_selection() const
{
    TextureSelection result;

    if (m_twin_table.is_empty() == false)
    {
        result.texture = m_bound_texture_array.at(m_active_unit);

        if (result.texture != 0)
        {
            result.has_twin = m_twin_table.find(result.texture,
                                               &result.twin);
        }

        for (uint32_t n_unit = 0;
                      n_unit < N_TEXTURE_UNITS && result.has_twin_in_another_unit == false;
                    ++n_unit)
        {
            const uint32_t texture = m_bound_texture_array.at(n_unit);
            uint32_t       twin    = 0;

            if (n_unit  != m_active_unit &&
                texture != 0)
            {
                result.has_twin_in_another_unit = m_twin_table.find(texture,
                                                                   &twin);
            }
        }
    }

    return result;
}

void Stereo::ConverterDLL::GLEyeRenderer::finish_eye_plan(const EyePlan& in_plan)
{
    constexpr GLenum MODELVIEW       = 0x1700;
    constexpr size_t VIEWPORT_HEIGHT = 3;
    constexpr size_t VIEWPORT_WIDTH  = 2;
    constexpr size_t VIEWPORT_X      = 0;
    constexpr size_t VIEWPORT_Y      = 1;

    if (in_plan.is_pushed)
    {
        GLDriverFunctions::get().viewport_func_ptr(in_plan.saved_viewport.at(VIEWPORT_X),
                                                   in_plan.saved_viewport.at(VIEWPORT_Y),
                                                   in_plan.saved_viewport.at(VIEWPORT_WIDTH),
                                                   in_plan.saved_viewport.at(VIEWPORT_HEIGHT) );
    }

    select_textures(in_plan.texture_selection,
                    false);

    if (in_plan.is_affected)
    {
        GLDriverFunctions::get().load_matrixf_func_ptr(in_plan.saved_matrix.data() );
    }

    if (in_plan.has_read_matrix                  &&
        in_plan.matrix_mode != static_cast<int32_t>(MODELVIEW) )
    {
        GLDriverFunctions::get().matrix_mode_func_ptr(in_plan.matrix_mode);
    }
}

void Stereo::ConverterDLL::GLEyeRenderer::finish_recorded_block(const bool&                        in_has_draws,
                                                                const Analysis::Recipe::DrawClass& in_draw_class,
                                                                const bool&                        in_is_pushed)
{
    m_is_recording = false;

    m_statistics.n_recorded_blocks++;

    if (in_has_draws == false ||
        in_draw_class != Analysis::Recipe::DrawClass::SKIPPED)
    {
        const auto draw_class = (in_has_draws) ? in_draw_class
                                               : Analysis::Recipe::DrawClass::UNAFFECTED;
        const bool is_pushed  = (in_has_draws &&
                                 in_is_pushed);

        /* With the right eye deferred, the left eye is drawn now and the right eye when the run is played. A block that cannot be kept is drawn for both eyes after the run is played. */
        const bool is_deferred = (m_defers_right_eye &&
                                  defer_block(draw_class,
                                              is_pushed) );

        if (is_deferred == false)
        {
            const auto replay_func = [this]()
            {
                m_block_recorder.play();
            };

            flush_run();

            play_eyes(replay_func,
                      draw_class,
                      is_pushed);
        }

        m_statistics.n_block_draws++;
    }
    else
    {
        m_statistics.n_skipped_draws++;
    }
}

void Stereo::ConverterDLL::GLEyeRenderer::flush_run()
{
    /* A few of the settings that the replay leaves the state with are compared with the ones the left eye left, every N_FLUSHES_BETWEEN_CHECKS runs. */
    constexpr uint64_t N_FLUSHES_BETWEEN_CHECKS = 32;
    constexpr GLenum   MODELVIEW                = 0x1700;
    constexpr GLenum   PROJECTION               = 0x1701;
    constexpr GLenum   PROJECTION_MATRIX        = 0x0BA7;

    constexpr std::array<GLenum, 8> CHECKED_CAPABILITIES =
    {
        0x0B44, /* GL_CULL_FACE    */
        0x0B50, /* GL_LIGHTING     */
        0x0B60, /* GL_FOG          */
        0x0B71, /* GL_DEPTH_TEST   */
        0x0B90, /* GL_STENCIL_TEST */
        0x0BC0, /* GL_ALPHA_TEST   */
        0x0BE2, /* GL_BLEND        */
        0x0DE1  /* GL_TEXTURE_2D   */
    };

    if (m_is_run_open)
    {
        const OwnCallScope    own_call_scope;
        const bool            is_checked = (m_n_flushes % N_FLUSHES_BETWEEN_CHECKS == 0);
        GLint                 left_matrix_mode = 0;
        std::array<float, 16> left_modelview;
        std::array<float, 16> left_projection;
        std::array<bool, 8>   left_capabilities  = {};

        m_n_flushes++;

        /* The state that the left eye has now is the state that the application is to find when the run has been played. */
        GLDriverFunctions::get().get_integerv_func_ptr(GL_MATRIX_MODE,
                                                       &left_matrix_mode);
        GLDriverFunctions::get().get_floatv_func_ptr  (GL_MODELVIEW_MATRIX,
                                                       left_modelview.data() );
        GLDriverFunctions::get().get_floatv_func_ptr  (PROJECTION_MATRIX,
                                                       left_projection.data() );

        if (is_checked)
        {
            for (size_t n_capability = 0;
                        n_capability < CHECKED_CAPABILITIES.size();
                      ++n_capability)
            {
                left_capabilities.at(n_capability) = (GLDriverFunctions::get().is_enabled_func_ptr(CHECKED_CAPABILITIES.at(n_capability) ) != GL_FALSE);
            }
        }

        m_targets.bind_eye(Analysis::Recipe::Eye::RIGHT);

        /* The state of the start of the run, then every call of the run as the application made it. */
        GLDriverFunctions::get().pop_attrib_func_ptr();

        m_run_recorder.play();

        GLDriverFunctions::get().matrix_mode_func_ptr(MODELVIEW);
        GLDriverFunctions::get().load_matrixf_func_ptr(left_modelview.data() );
        GLDriverFunctions::get().matrix_mode_func_ptr(PROJECTION);
        GLDriverFunctions::get().load_matrixf_func_ptr(left_projection.data() );
        GLDriverFunctions::get().matrix_mode_func_ptr(static_cast<GLenum>(left_matrix_mode) );

        if (is_checked)
        {
            for (size_t n_capability = 0;
                        n_capability < CHECKED_CAPABILITIES.size();
                      ++n_capability)
            {
                const bool is_enabled = (GLDriverFunctions::get().is_enabled_func_ptr(CHECKED_CAPABILITIES.at(n_capability) ) != GL_FALSE);

                if (is_enabled != left_capabilities.at(n_capability) )
                {
                    m_statistics.n_run_mismatches++;

                    note_once("run_mismatch",
                              "A run that was played for the right eye left a setting different from the one the left eye left (a capability that is enabled)");
                }
            }
        }

        m_targets.bind_eye(Analysis::Recipe::Eye::LEFT);

        m_statistics.n_runs++;

        m_run_recorder.clear();

        m_is_run_open = false;
    }
}

void Stereo::ConverterDLL::GLEyeRenderer::follow_run_call(const APIInterceptor::APIFunction&         in_function,
                                                          const uint32_t&                            in_n_arguments,
                                                          const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t MODELVIEW  = 0x1700;
    constexpr uint32_t PROJECTION = 0x1701;

    switch (GLRunRecorder::classify(in_function) )
    {
        case RunCallClass::FLUSH:
        {
            flush_run();

            break;
        }

        case RunCallClass::MATRIX:
        {
            /* The matrices of a draw are given with the draw. The matrices of the texture have no such thing, so a call that changes them is made after the run is played. */
            if (in_function == APIInterceptor::APIFUNCTION_GL_GLMATRIXMODE)
            {
                uint32_t mode = 0;

                if (in_n_arguments > 0                                       &&
                    APIInterceptor::read_number(in_arguments_ptr[0],
                                               &mode) )
                {
                    m_matrix_mode_shadow = mode;
                }
                else
                {
                    m_matrix_mode_shadow = 0;
                }
            }
            else
            if (m_matrix_mode_shadow != MODELVIEW  &&
                m_matrix_mode_shadow != PROJECTION)
            {
                flush_run();
            }

            break;
        }

        case RunCallClass::RECORDED:
        {
            RunStateCall call;

            if (m_is_run_open == false)
            {
                open_run();
            }

            if (GLRunRecorder::read_state_call(in_function,
                                               in_n_arguments,
                                               in_arguments_ptr,
                                              &call) )
            {
                m_run_recorder.add_state(call);

                m_statistics.n_run_states++;
            }
            else
            {
                flush_run();
            }

            break;
        }

        default:
        {
            break;
        }
    }
}

void Stereo::ConverterDLL::GLEyeRenderer::forget_lost_depth_reads()
{
    const uint32_t generation = m_targets.get_generation();

    if (generation != m_depth_generation)
    {
        m_depth_ledger.forget_pending();

        m_depth_generation = generation;
    }
}

uint32_t Stereo::ConverterDLL::GLEyeRenderer::get_depth_lag() const
{
    return m_depth_lag;
}

const Stereo::ConverterDLL::GLEyeRenderer::Statistics& Stereo::ConverterDLL::GLEyeRenderer::get_statistics() const
{
    return m_statistics;
}

bool Stereo::ConverterDLL::GLEyeRenderer::is_ready() const
{
    return m_targets.is_ready();
}

Stereo::ConverterDLL::GLEyeRenderer::EyePlan Stereo::ConverterDLL::GLEyeRenderer::make_eye_plan(const Analysis::Recipe::DrawClass& in_draw_class,
                                                                                                const bool&                        in_is_pushed,
                                                                                                const bool&                        in_needs_matrices)
{
    /* Positions of the values this function needs within a matrix (stored column by column) and within a viewport. */
    constexpr size_t MATRIX_X_SCALE       = 0;
    constexpr size_t MATRIX_Z_TRANSLATION = 14;
    constexpr GLenum MODELVIEW            = 0x1700;
    constexpr GLenum PROJECTION_MATRIX    = 0x0BA7;
    constexpr GLenum VIEWPORT             = 0x0BA2;

    EyePlan    result;
    const bool is_last_draw_part = m_core_ptr->get_is_last_draw_part();

    result.is_affected      = (in_draw_class == Analysis::Recipe::DrawClass::AFFECTED);
    result.is_pushed        = in_is_pushed;
    result.is_side_reaching = m_core_ptr->get_is_last_draw_side_reaching();
    result.matrix_mode      = MODELVIEW;

    /* The matrices come from the analysis, which follows the calls of the application; the driver is asked only if it does not have them. */
    result.has_tracked_matrices = (in_needs_matrices                       ||
                                   result.is_affected                      ||
                                   (in_is_pushed && is_last_draw_part) ) ? read_tracked_matrices(&result.tracked_modelview,
                                                                                                 &result.tracked_projection,
                                                                                                 &result.matrix_mode)
                                                                         : false;

    const auto get_modelview_func = [&](std::array<float, 16>* out_f32mat_ptr)
    {
        if (result.has_tracked_matrices)
        {
            *out_f32mat_ptr = result.tracked_modelview;
        }
        else
        {
            GLDriverFunctions::get().get_floatv_func_ptr(GL_MODELVIEW_MATRIX,
                                                         out_f32mat_ptr->data() );
        }
    };

    const auto get_projection_func = [&](std::array<float, 16>* out_f32mat_ptr)
    {
        if (result.has_tracked_matrices)
        {
            *out_f32mat_ptr = result.tracked_projection;
        }
        else
        {
            GLDriverFunctions::get().get_floatv_func_ptr(PROJECTION_MATRIX,
                                                         out_f32mat_ptr->data() );
        }
    };

    /* The viewport is the one the draw is made with. A pushed draw is moved by shifting the origin of the viewport, which works with any projection. */
    if (in_is_pushed)
    {
        GLDriverFunctions::get().get_integerv_func_ptr(VIEWPORT,
                                                       result.saved_viewport.data() );
    }

    /* A part that protrudes from an object is drawn once for both eyes. It must appear where the eyes would have put its origin, so the camera of the draw is read here. */
    if (in_is_pushed &&
        is_last_draw_part)
    {
        get_modelview_func (&result.part_matrix);
        get_projection_func(&result.part_projection);

        result.is_part = true;
    }

    if (result.is_affected                  &&
        m_measures_depth                    &&
        m_pass_has_affected_draws == false)
    {
        get_projection_func(&m_pass_projection_f32mat);

        /* A projection that is not perspective carries no depth information. */
        m_pass_has_affected_draws = Analysis::APIDumpAnalysis::DrawMeasurer::is_perspective_projection(m_pass_projection_f32mat);
    }

    if (result.is_affected)
    {
        if (result.has_tracked_matrices == false)
        {
            GLDriverFunctions::get().get_integerv_func_ptr(GL_MATRIX_MODE,
                                                           &result.matrix_mode);
        }

        result.has_read_matrix = true;

        if (result.matrix_mode != static_cast<int32_t>(MODELVIEW) )
        {
            GLDriverFunctions::get().matrix_mode_func_ptr(MODELVIEW);
        }

        get_modelview_func(&result.saved_matrix);

        /* The core decides whether the draw is attached to the camera from the distance of its origin in front of the camera. */
        const float origin_depth = -result.saved_matrix.at(MATRIX_Z_TRANSLATION);

        if (m_core_ptr->is_last_draw_attached_to_camera(origin_depth) )
        {
            result.is_affected = false;

            m_statistics.n_camera_attached_draws++;
        }
    }

    /* Parallax the eyes already apply to the draw, which the push must leave room for; zero for an unmoved draw. */
    if (in_is_pushed &&
        result.is_affected)
    {
        std::array<float, 16> projection_f32mat;

        get_projection_func(&projection_f32mat);

        result.layer_parallax = m_core_ptr->get_eye_parallax(-result.saved_matrix.at(MATRIX_Z_TRANSLATION),
                                                             projection_f32mat.at(MATRIX_X_SCALE),
                                                             Analysis::APIDumpAnalysis::DrawMeasurer::is_perspective_projection(projection_f32mat) );
    }

    /* The textures do not change between the eyes, so what each needs is looked up once. */
    result.texture_selection = find_texture_selection();

    if (result.texture_selection.has_twin_in_another_unit)
    {
        note_once("units",
                  "A texture that has a twin is bound to a unit other than the one that is active: it is not bound to its twin for the right eye");
    }

    return result;
}

Stereo::ConverterDLL::GLEyeRenderer::EyeSetup Stereo::ConverterDLL::GLEyeRenderer::make_eye_setup(const Analysis::Recipe::Eye& in_eye,
                                                                                                  const EyePlan&               in_plan)
{
    constexpr size_t MATRIX_X_SCALE       = 0;
    constexpr size_t MATRIX_Z_TRANSLATION = 14;
    constexpr size_t VIEWPORT_HEIGHT      = 3;
    constexpr size_t VIEWPORT_WIDTH       = 2;
    constexpr size_t VIEWPORT_X           = 0;
    constexpr size_t VIEWPORT_Y           = 1;

    EyeSetup result;

    if (in_plan.is_affected)
    {
        /* The eye matrix times the draw's own, loaded at once. */
        const auto eye_matrix = m_core_ptr->make_eye_shift_matrix(in_eye);

        result.has_matrix = true;
        result.matrix     = Analysis::APIDumpAnalysis::Frontends::GLMatrixMath::multiply(eye_matrix,
                                                                                         in_plan.saved_matrix);
    }

    if (in_plan.is_pushed)
    {
        const float picture_width_pixels = static_cast<float>(in_plan.saved_viewport.at(VIEWPORT_WIDTH) );
        float       shift_pixels         = m_core_ptr->plan_push_shift_with_layer(in_eye,
                                                                                  picture_width_pixels,
                                                                                  in_plan.layer_parallax);

        if (in_plan.is_part)
        {
            /* A perspective projection has a -1 in the third column of its fourth row. */
            shift_pixels = m_core_ptr->plan_part_shift(in_eye,
                                                       picture_width_pixels,
                                                       in_plan.part_matrix.at(MATRIX_Z_TRANSLATION),
                                                       in_plan.part_projection.at(MATRIX_X_SCALE),
                                                       Analysis::APIDumpAnalysis::DrawMeasurer::is_perspective_projection(in_plan.part_projection) );
        }

        /* The shift is rounded toward zero so that it stays within the plan. An overlay that reaches the sides is widened so that no edge is left bare. */
        const auto pushed_viewport = Stereo::Core::PushShiftPlanner::make_viewport(in_plan.saved_viewport.at(VIEWPORT_X),
                                                                                   in_plan.saved_viewport.at(VIEWPORT_WIDTH),
                                                                                   shift_pixels,
                                                                                   (in_plan.is_part == false && in_plan.is_side_reaching) );

        result.has_viewport = true;
        result.viewport     = {pushed_viewport.x,
                               in_plan.saved_viewport.at(VIEWPORT_Y),
                               pushed_viewport.width,
                               in_plan.saved_viewport.at(VIEWPORT_HEIGHT)};
    }

    return result;
}

bool Stereo::ConverterDLL::GLEyeRenderer::make_twin(const uint32_t& in_texture,
                                                    const bool&     in_has_level_size,
                                                    const int32_t&  in_level)
{
    constexpr GLenum TEXTURE_HEIGHT          = 0x1001;
    constexpr GLenum TEXTURE_INTERNAL_FORMAT = 0x1003;
    constexpr GLenum TEXTURE_MAG_FILTER      = 0x2800;
    constexpr GLenum TEXTURE_MIN_FILTER      = 0x2801;
    constexpr GLenum TEXTURE_WIDTH           = 0x1000;
    constexpr GLenum TEXTURE_WRAP_S          = 0x2802;
    constexpr GLenum TEXTURE_WRAP_T          = 0x2803;

    const OwnCallScope own_call_scope;
    GLuint             twin    = 0;
    bool               result  = false;
    GLint              height  = 0;
    GLint              format  = 0;
    GLint              width   = 0;
    std::array<GLint, 4> parameter_array = {};

    if (in_has_level_size)
    {
        GLDriverFunctions::get().get_tex_level_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                                    in_level,
                                                                    TEXTURE_WIDTH,
                                                                    &width);
        GLDriverFunctions::get().get_tex_level_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                                    in_level,
                                                                    TEXTURE_HEIGHT,
                                                                    &height);
        GLDriverFunctions::get().get_tex_level_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                                    in_level,
                                                                    TEXTURE_INTERNAL_FORMAT,
                                                                    &format);

        if (width  <= 0 ||
            height <= 0)
        {
            goto end;
        }
    }

    GLDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_MIN_FILTER,
                                                          &parameter_array.at(0) );
    GLDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_MAG_FILTER,
                                                          &parameter_array.at(1) );
    GLDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_WRAP_S,
                                                          &parameter_array.at(2) );
    GLDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_WRAP_T,
                                                          &parameter_array.at(3) );

    GLDriverFunctions::get().gen_textures_func_ptr(1,
                                                   &twin);
    GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                   twin);

    GLDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_MIN_FILTER,
                                                     parameter_array.at(0) );
    GLDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_MAG_FILTER,
                                                     parameter_array.at(1) );
    GLDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_WRAP_S,
                                                     parameter_array.at(2) );
    GLDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_WRAP_T,
                                                     parameter_array.at(3) );

    if (in_has_level_size)
    {
        GLDriverFunctions::get().tex_image_2d_func_ptr(GL_TEXTURE_2D,
                                                       in_level,
                                                       format,
                                                       width,
                                                       height,
                                                       0, /* border */
                                                       GL_RGBA,
                                                       GL_UNSIGNED_BYTE,
                                                       nullptr);
    }

    GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                   in_texture);

    m_twin_table.set(in_texture,
                     twin);

    m_statistics.n_twin_textures++;

    result = true;

end:
    return result;
}

Stereo::Core::DepthRange Stereo::ConverterDLL::GLEyeRenderer::measure_depths(const float&  in_z_scale,
                                                                             const float&  in_z_shift,
                                                                             const float*  in_depth_ptr,
                                                                             const size_t& in_n_depths)
{
    /* A depth array larger than N_DEPTHS_LOOKED_AT_WHOLE is a whole depth buffer (the small copy could not be made), so every fourth pixel is measured. The small copy is measured at every pixel. */
    constexpr size_t N_DEPTHS_LOOKED_AT_WHOLE = 100000;
    constexpr size_t N_PIXELS_BETWEEN_SAMPLES = 4;

    const float  near_fraction = m_core_ptr->get_near_fraction();
    const size_t step          = (in_n_depths > N_DEPTHS_LOOKED_AT_WHOLE) ? N_PIXELS_BETWEEN_SAMPLES
                                                                          : 1;

    return m_fast_depth_range_measurer.measure(in_depth_ptr,
                                               in_n_depths,
                                               step,
                                               in_z_scale,
                                               in_z_shift,
                                               near_fraction);
}

void Stereo::ConverterDLL::GLEyeRenderer::measure_pass_depth()
{
    const bool is_ready = m_targets.is_ready();

    if (m_measures_depth          &&
        m_pass_has_affected_draws &&
        is_ready)
    {
        forget_lost_depth_reads();

        /* With a buffer of the graphics card, the depth read is not waited for. Its result is taken at the end of the next frame, and it belongs to the frame the read started in. */
        if (m_targets.has_depth_read_buffers() )
        {
            start_depth_read();
        }
        else
        {
            read_depth_at_once();
        }
    }

    m_pass_has_affected_draws = false;
}

bool Stereo::ConverterDLL::GLEyeRenderer::mirror_copy(const APIInterceptor::APIFunction&         in_function,
                                                      const uint32_t&                            in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t N_COPY_IMAGE_ARGUMENTS     = 8;
    constexpr uint32_t N_COPY_SUB_IMAGE_ARGUMENTS = 8;

    const bool     is_sub  = (in_function == APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D);
    const uint32_t texture = m_bound_texture_array.at(m_active_unit);

    bool                   is_made_scaled = false;
    std::array<int32_t, 8> value_array    = {};
    uint32_t               target         = 0;

    if (texture != 0                                                              &&
        in_n_arguments == ( (is_sub) ? N_COPY_SUB_IMAGE_ARGUMENTS : N_COPY_IMAGE_ARGUMENTS) &&
        APIInterceptor::read_number(in_arguments_ptr[0],
                                   &target) )
    {
        bool is_read = true;

        for (uint32_t n_argument = 1;
                      n_argument < in_n_arguments;
                    ++n_argument)
        {
            is_read &= APIInterceptor::read_number(in_arguments_ptr[n_argument],
                                                  &value_array.at(n_argument) );
        }

        if (is_read && target == GL_TEXTURE_2D)
        {
            const OwnCallScope own_call_scope;
            uint32_t           twin          = 0;
            bool               has_twin      = m_twin_table.find(texture,
                                                                &twin);
            const bool         is_scaled     = (m_scaler.is_identity() == false);

            /* The framebuffer is larger than the application knows, so the sizes of the copy are scaled up by the same factor. */
            if (is_scaled)
            {
                m_scaler.scale_copy(is_sub,
                                    texture,
                                   &value_array);
            }

            if (has_twin == false)
            {
                has_twin = make_twin(texture,
                                     is_sub,
                                     value_array.at(1) );

                if (has_twin)
                {
                    m_twin_table.find(texture,
                                     &twin);
                }
            }

            if (has_twin)
            {
                m_targets.bind_eye(Analysis::Recipe::Eye::RIGHT);

                GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                               twin);

                copy_framebuffer(is_sub,
                                 target,
                                 value_array);

                GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                               texture);

                m_targets.bind_eye(Analysis::Recipe::Eye::LEFT);

                m_statistics.n_texture_copies++;
            }
            else
            {
                m_statistics.n_unmirrored_calls++;

                note_once("copy_no_twin",
                          "A copy of the framebuffer to a texture was not made for the right eye: the twin of the texture cannot be made");
            }

            /* The application's copy call uses the sizes of the smaller framebuffer, so it cannot be made. The left-eye copy is made with the scaled-up sizes instead. */
            if (is_scaled)
            {
                copy_framebuffer(is_sub,
                                 target,
                                 value_array);

                is_made_scaled = true;
            }
        }
    }

    return is_made_scaled;
}

void Stereo::ConverterDLL::GLEyeRenderer::mirror_texture_parameter(const APIInterceptor::APIFunction&         in_function,
                                                                   const uint32_t&                            in_n_arguments,
                                                                   const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t N_ARGUMENTS = 3;

    const uint32_t texture = m_bound_texture_array.at(m_active_unit);

    uint32_t target = 0;
    uint32_t twin   = 0;
    uint32_t name   = 0;
    float    value  = 0.0f;

    const bool has_twin = m_twin_table.find(texture,
                                           &twin);

    if (has_twin                   &&
        in_n_arguments == N_ARGUMENTS   &&
        APIInterceptor::read_number(in_arguments_ptr[0],
                                   &target)                       &&
        APIInterceptor::read_number(in_arguments_ptr[1],
                                   &name)                         &&
        APIInterceptor::read_number(in_arguments_ptr[2],
                                   &value)                        &&
        target == GL_TEXTURE_2D)
    {
        const OwnCallScope own_call_scope;

        GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                       twin);

        if (in_function == APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI)
        {
            GLDriverFunctions::get().tex_parameteri_func_ptr(target,
                                                             name,
                                                             static_cast<GLint>(value) );
        }
        else
        {
            GLDriverFunctions::get().tex_parameterf_func_ptr(target,
                                                             name,
                                                             value);
        }

        GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                       texture);
    }
}

void Stereo::ConverterDLL::GLEyeRenderer::note_once(const std::string& in_key,
                                                    const std::string& in_note)
{
    if (std::find(m_noted_key_vec.begin(),
                  m_noted_key_vec.end  (),
                  in_key) == m_noted_key_vec.end() )
    {
        m_noted_key_vec.push_back(in_key);
        m_note_vec.push_back     (in_note);
    }
}

bool Stereo::ConverterDLL::GLEyeRenderer::on_attribute(const APIInterceptor::APIFunction&         in_function,
                                                       const uint32_t&                            in_n_arguments,
                                                       const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    RecordedCallLayout layout;
    bool               result    = true;
    const bool         is_vertex = (GLBlockRecorder::find_layout(in_function,
                                                                &layout)                   &&
                                    GLBlockRecorder::is_vertex(layout.kind) );

    /* The analysis is not given the vertex calls that are recorded here, nor the ones that follow a fall back to a display list: it needs how many there were when the
     * block ends (see take_unanalyzed_vertex_count()).
     */
    if (m_is_recording)
    {
        if (record_call(in_function,
                        in_n_arguments,
                        in_arguments_ptr) )
        {
            result = false;
        }
        else
        {
            fall_back_to_list(in_function);
        }

        if (is_vertex)
        {
            m_n_unanalyzed_vertices++;
        }
    }
    else
    if (is_vertex &&
        m_is_in_block)
    {
        m_n_unanalyzed_vertices++;
    }
    else
    if (m_defers_right_eye          &&
        m_is_in_app_list == false   &&
        is_vertex        == false   &&
        m_targets.is_ready() )
    {
        /* A color, a normal or a texture coordinate that is set between blocks is what the next block starts with, for both eyes. */
        RecordedCall       call;
        RecordedCallLayout run_layout;

        if (read_recorded_call(in_function,
                               in_n_arguments,
                               in_arguments_ptr,
                              &run_layout,
                              &call) &&
            run_layout.is_vector == false)
        {
            if (m_is_run_open == false)
            {
                open_run();
            }

            m_run_recorder.add_attribute(call);

            m_statistics.n_run_states++;
        }
        else
        {
            flush_run();
        }
    }

    return result;
}

bool Stereo::ConverterDLL::GLEyeRenderer::on_frame_ending(GLFrameImages* out_images_ptr)
{
    bool result = false;

    /* The right eye is complete before the eyes are read. */
    flush_run();

    /* A frame ends here. The picture that was read is out of date in the next frame, whatever calls come in between. */
    m_readback_cache.invalidate();

    if (m_targets.is_ready() == false)
    {
        goto end;
    }

    /* Take the results of the depth reads started in earlier frames. finish_depth_read() waits for any read that is still running. Each result goes to the frame it belongs to. */
    forget_lost_depth_reads();

    collect_depth_reads(true,   /* in_only_older_frames */
                        false); /* in_only_the_oldest */

    measure_pass_depth();

    /* The depth of the previous frame is complete, because all of its reads were taken above. With buffers of the graphics card, the depth of this frame arrives one frame later (lag 1). Without them it was read at once (lag 0). */
    m_depth_lag = (m_targets.has_depth_read_buffers() ) ? 1
                                                        : 0;

    m_depth_ledger.end_frame(m_depth_lag);

    {
        /* The images returned belong to the previous frame. They were read while this frame was being made. */
        result = m_frame_reader.read(&m_targets,
                                     out_images_ptr);

        out_images_ptr->picture = m_targets.get_picture();

        /* A program that shows the images elsewhere does not need the window. Otherwise the window shows the left eye. */
        if (m_is_shown_elsewhere == false)
        {
            m_targets.blit_left_to_window();
        }

        m_invisible_window_manager.keep_invisible();
    }

end:
    return result;
}

void Stereo::ConverterDLL::GLEyeRenderer::on_post(const APIInterceptor::APIFunction&         in_function,
                                                  const uint32_t&                            in_n_arguments_out,
                                                  const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr)
{
    (void) in_n_arguments_out;
    (void) in_arguments_out_ptr;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS:
        {
            /* The window may have a new size, so the framebuffers are made again for the frame that begins. */
            try_to_prepare();

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLEND:
        {
            if (m_is_in_block)
            {
                const OwnCallScope own_call_scope;

                GLDriverFunctions::get().end_list_func_ptr();

                m_is_in_block = false;

                if (m_block_has_draws == false ||
                    m_block_class     != Analysis::Recipe::DrawClass::SKIPPED)
                {
                    const GLuint list = m_block_list;

                    const auto call_list_func = [list]()
                    {
                        GLDriverFunctions::get().call_list_func_ptr(list);
                    };

                    play_eyes(call_list_func,
                              (m_block_has_draws) ? m_block_class
                                                  : Analysis::Recipe::DrawClass::UNAFFECTED,
                              (m_block_has_draws &&
                               m_block_is_pushed) );

                    m_statistics.n_block_draws++;
                }
                else
                {
                    m_statistics.n_skipped_draws++;
                }

                m_block_has_draws = false;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_WGL_WGLMAKECURRENT:
        {
            try_to_prepare();

            break;
        }

        default:
        {
            break;
        }
    }
}

bool Stereo::ConverterDLL::GLEyeRenderer::on_pre(const APIInterceptor::APIFunction&         in_function,
                                                 const uint32_t&                            in_n_arguments,
                                                 const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                 const bool&                                in_has_draws,
                                                 const Analysis::Recipe::DrawClass&         in_draw_class,
                                                 const bool&                                in_is_pushed)
{
    constexpr GLenum COMPILE_AND_EXECUTE = 0x1301;
    constexpr GLenum TEXTURE0            = 0x84C0;

    bool result            = true;
    bool is_made_scaled    = false;
    bool is_read_served    = false;

    /* A call that the right eye cannot be given later makes the run play first, before the scaler or anything else changes the state. */
    if (m_defers_right_eye       &&
        m_is_in_app_list == false &&
        m_is_recording   == false &&
        m_targets.is_ready() )
    {
        follow_run_call(in_function,
                        in_n_arguments,
                        in_arguments_ptr);
    }

    /* Any call except a pixel read may change the picture, so the picture that was read is discarded. This applies only if the recipe serves pixel reads from the picture. */
    if (m_caches_pixel_reads)
    {
        if (in_function != APIInterceptor::APIFUNCTION_GL_GLREADPIXELS)
        {
            m_readback_cache.invalidate();
        }
        else
        if (m_targets.is_ready() )
        {
            is_read_served = serve_pixel_read(in_n_arguments,
                                              in_arguments_ptr);
        }
    }

    /* State that the application gives in pixels is adapted to the size of the framebuffers. This applies inside and outside a display list that the application compiles; inside one, the call made here is compiled into the list. */
    if (m_targets.is_ready()            &&
        m_scaler.is_identity() == false &&
        is_read_served         == false)
    {
        is_made_scaled = m_scaler.on_pre(in_function,
                                         in_n_arguments,
                                         in_arguments_ptr,
                                         m_bound_texture_array.at(m_active_unit) );
    }

    /* A display list that the application compiles executes nothing. Its contents run when the list is called. */
    if (m_is_in_app_list)
    {
        if (in_function == APIInterceptor::APIFUNCTION_GL_GLENDLIST)
        {
            m_is_in_app_list = false;
        }

        goto end;
    }

    /* A block that is being recorded keeps the calls that it can, and does not make them. Its glEnd makes it for both eyes. Any other call hands the
     * block to a display list (the call is then made, into the list).
     */
    if (m_is_recording                                                                 &&
        Capture::APIDumpLoader::get_api_group(in_function) == Capture::APIDumpLoader::APIGroup::GL)
    {
        if (in_function == APIInterceptor::APIFUNCTION_GL_GLEND)
        {
            finish_recorded_block(in_has_draws,
                                  in_draw_class,
                                  in_is_pushed);

            result = false;

            goto end;
        }

        if (record_call(in_function,
                        in_n_arguments,
                        in_arguments_ptr) )
        {
            result = false;

            goto end;
        }

        fall_back_to_list(in_function);
    }

    if (m_targets.is_ready() == false)
    {
        if (in_function == APIInterceptor::APIFUNCTION_GL_GLBEGIN ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLCLEAR)
        {
            try_to_prepare();
        }
    }

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLACCUM:
        case APIInterceptor::APIFUNCTION_GL_GLBITMAP:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWPIXELS:
        {
            if (m_targets.is_ready() )
            {
                m_statistics.n_unmirrored_calls++;

                note_once("pixels",
                          "A call that draws pixels (glAccum, glBitmap, glCopyPixels, glDrawPixels) was made for the left eye only");
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE:
        {
            uint32_t texture = 0;

            if (in_n_arguments > 0 &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &texture) )
            {
                m_active_unit = std::min<uint32_t>(texture - TEXTURE0,
                                                   static_cast<uint32_t>(N_TEXTURE_UNITS - 1) );
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLBEGIN:
        {
            /* The block is not executed. It is executed once for each eye when the block ends, when the class that the recipe gives it is known: from the calls
             * that were recorded, or from a display list that it was compiled into.
             */
            if (m_targets.is_ready() )
            {
                uint32_t mode = 0;

                if (m_records_immediate_mode_blocks   &&
                    in_n_arguments > 0                &&
                    APIInterceptor::read_number(in_arguments_ptr[0],
                                               &mode) )
                {
                    m_block_recorder.begin(mode);

                    m_is_recording          = true;
                    m_n_unanalyzed_vertices = 0;

                    result = false;
                }
                else
                {
                    const OwnCallScope own_call_scope;

                    if (m_block_list == 0)
                    {
                        m_block_list = GLDriverFunctions::get().gen_lists_func_ptr(1);
                    }

                    GLDriverFunctions::get().new_list_func_ptr(m_block_list,
                                                               GL_COMPILE);

                    m_is_in_block           = true;
                    m_n_unanalyzed_vertices = 0;
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
        {
            uint32_t name   = 0;
            uint32_t target = 0;

            if (in_n_arguments > 1                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &target)                      &&
                APIInterceptor::read_number(in_arguments_ptr[1],
                                           &name)                        &&
                target == GL_TEXTURE_2D)
            {
                m_bound_texture_array.at(m_active_unit) = name;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS:
        {
            if (in_has_draws          &&
                m_targets.is_ready() )
            {
                result = on_pre_draw(in_function,
                                     in_n_arguments,
                                     in_arguments_ptr,
                                     in_draw_class,
                                     in_is_pushed);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCLEAR:
        {
            uint32_t mask = 0;

            if (m_targets.is_ready()                                     &&
                in_n_arguments > 0                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &mask) )
            {
                constexpr uint32_t DEPTH_BUFFER_BIT = 0x00000100;

                /* What the pass drew stays in the depth buffer until the buffer is cleared. */
                if (mask & DEPTH_BUFFER_BIT)
                {
                    measure_pass_depth();
                }

                const OwnCallScope own_call_scope;

                m_targets.bind_eye(Analysis::Recipe::Eye::RIGHT);

                GLDriverFunctions::get().clear_func_ptr(mask);

                m_targets.bind_eye(Analysis::Recipe::Eye::LEFT);

                m_statistics.n_clears++;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D:
        {
            if (m_targets.is_ready() )
            {
                if (mirror_copy(in_function,
                                in_n_arguments,
                                in_arguments_ptr) )
                {
                    is_made_scaled = true;
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES:
        {
            const void* names_ptr = nullptr;
            int32_t     count     = 0;

            if (in_n_arguments > 1                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &count)                       &&
                count > 0)
            {
                names_ptr = resolve_pointer(in_arguments_ptr[1]);
            }

            if (names_ptr != nullptr)
            {
                const OwnCallScope own_call_scope;
                const uint32_t*    name_ptr = static_cast<const uint32_t*>(names_ptr);

                for (int32_t n_name = 0;
                             n_name < count;
                           ++n_name)
                {
                    uint32_t   twin_name = 0;
                    const bool has_twin  = m_twin_table.find(name_ptr[n_name],
                                                            &twin_name);

                    m_scaler.forget_texture(name_ptr[n_name]);

                    if (has_twin)
                    {
                        const GLuint twin = twin_name;

                        GLDriverFunctions::get().delete_textures_func_ptr(1,
                                                                          &twin);

                        m_twin_table.erase(name_ptr[n_name]);
                    }

                    for (auto& bound_texture : m_bound_texture_array)
                    {
                        if (bound_texture == name_ptr[n_name])
                        {
                            bound_texture = 0;
                        }
                    }
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLEND:
        {
            m_block_class     = in_draw_class;
            m_block_has_draws = in_has_draws;
            m_block_is_pushed = in_is_pushed;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
        {
            uint32_t mode = 0;

            m_is_in_app_list = true;

            if (in_n_arguments > 1                                       &&
                APIInterceptor::read_number(in_arguments_ptr[1],
                                           &mode)                        &&
                mode == COMPILE_AND_EXECUTE)
            {
                note_once("compile_and_execute",
                          "A display list that is compiled and made at once is made for the left eye only");
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE2D:
        {
            uint32_t twin = 0;

            if (m_twin_table.find(m_bound_texture_array.at(m_active_unit),
                                 &twin) )
            {
                m_statistics.n_unmirrored_calls++;

                note_once("texture_upload",
                          "An upload of pixels to a texture that has a twin for the right eye was made to the texture only");
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERF:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI:
        {
            if (m_targets.is_ready() )
            {
                mirror_texture_parameter(in_function,
                                         in_n_arguments,
                                         in_arguments_ptr);
            }

            break;
        }

        default:
        {
            break;
        }
    }

end:
    return (result                      &&
            is_made_scaled == false     &&
            is_read_served == false);
}

bool Stereo::ConverterDLL::GLEyeRenderer::on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                                                      const uint32_t&                            in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                      const Analysis::Recipe::DrawClass&         in_draw_class,
                                                      const bool&                                in_is_pushed)
{
    std::function<void()> draw_func;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
        {
            uint32_t list = 0;

            if (in_n_arguments > 0                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &list) )
            {
                draw_func = [list]()
                {
                    GLDriverFunctions::get().call_list_func_ptr(list);
                };

                m_statistics.n_list_draws++;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS:
        {
            int32_t  count = 0;
            int32_t  first = 0;
            uint32_t mode  = 0;

            if (in_n_arguments > 2                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &mode)                        &&
                APIInterceptor::read_number(in_arguments_ptr[1],
                                           &first)                       &&
                APIInterceptor::read_number(in_arguments_ptr[2],
                                           &count) )
            {
                draw_func = [count, first, mode]()
                {
                    GLDriverFunctions::get().draw_arrays_func_ptr(mode,
                                                                  first,
                                                                  count);
                };

                m_statistics.n_array_draws++;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS:
        {
            int32_t  count = 0;
            uint32_t mode  = 0;
            uint32_t type  = 0;

            if (in_n_arguments > 3                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &mode)                        &&
                APIInterceptor::read_number(in_arguments_ptr[1],
                                           &count)                       &&
                APIInterceptor::read_number(in_arguments_ptr[2],
                                           &type) )
            {
                /* The indices are either in the memory of the application (a chunk) or an offset into a bound buffer. In the second case the pointer value is the offset itself. */
                const void* const indices_ptr = (in_arguments_ptr[3].type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID) ? resolve_pointer(in_arguments_ptr[3])
                                                                                                                    : in_arguments_ptr[3].get_ptr();

                draw_func = [count, indices_ptr, mode, type]()
                {
                    GLDriverFunctions::get().draw_elements_func_ptr(mode,
                                                                    count,
                                                                    type,
                                                                    indices_ptr);
                };

                m_statistics.n_array_draws++;
            }

            break;
        }

        default:
        {
            break;
        }
    }

    if (draw_func)
    {
        if (in_draw_class != Analysis::Recipe::DrawClass::SKIPPED)
        {
            play_eyes(draw_func,
                      in_draw_class,
                      in_is_pushed);
        }
        else
        {
            m_statistics.n_skipped_draws++;
        }
    }

    /* The converter makes this call for both eyes. A call whose arguments cannot be read is left to the application. */
    return (draw_func == nullptr);
}

void Stereo::ConverterDLL::GLEyeRenderer::open_run()
{
    constexpr GLbitfield ALL_ATTRIBUTE_BITS = 0x000FFFFF;

    const OwnCallScope own_call_scope;

    /* The state at the start of the run is kept by the driver. The right eye starts from it when the run is played. */
    GLDriverFunctions::get().push_attrib_func_ptr(ALL_ATTRIBUTE_BITS);

    m_is_run_open = true;
}

void Stereo::ConverterDLL::GLEyeRenderer::play_eyes(const std::function<void()>&       in_draw_func,
                                                    const Analysis::Recipe::DrawClass& in_draw_class,
                                                    const bool&                        in_is_pushed)
{
    const OwnCallScope own_call_scope;
    const EyePlan      plan = make_eye_plan(in_draw_class,
                                            in_is_pushed,
                                            false); /* in_needs_matrices */

    for (const auto eye : {Analysis::Recipe::Eye::RIGHT, Analysis::Recipe::Eye::LEFT})
    {
        m_targets.bind_eye(eye);

        apply_eye(eye,
                  plan,
                  in_draw_func);
    }

    finish_eye_plan(plan);
}

void Stereo::ConverterDLL::GLEyeRenderer::read_depth_at_once()
{
    uint32_t depth_height = 0;
    uint32_t depth_width  = 0;

    const bool is_read = m_targets.read_depth(&m_depth_f32_vec,
                                              &depth_width,
                                              &depth_height);

    if (is_read)
    {
        const auto range = measure_depths(m_pass_projection_f32mat.at(10),
                                          m_pass_projection_f32mat.at(14),
                                          m_depth_f32_vec.data(),
                                          m_depth_f32_vec.size() );

        if (range.has_depth)
        {
            m_depth_ledger.add_result_now(range,
                                          m_pass_projection_f32mat.at(0) );
        }
    }
}

bool Stereo::ConverterDLL::GLEyeRenderer::read_recorded_call(const APIInterceptor::APIFunction&         in_function,
                                                              const uint32_t&                            in_n_arguments,
                                                              const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                              RecordedCallLayout*                        out_layout_ptr,
                                                              RecordedCall*                              out_call_ptr)
{
    RecordedCallLayout   layout;
    bool                 result            = GLBlockRecorder::find_layout(in_function,
                                                                          &layout);
    std::array<float, 4> value_fp32vec4    = {};

    if (result)
    {
        if (layout.is_vector)
        {
            const void* data_ptr = nullptr;
            uint32_t    n_bytes  = 0;
            bool        is_taken = false;

            result = (in_n_arguments > 0                                                &&
                      in_arguments_ptr[0].type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID);

            if (result)
            {
                const uint32_t chunk_id = in_arguments_ptr[0].get_data_chunk_id();

                /* A vertex call is analysed before it is made: the analysis holds its chunk. The calls that set an attribute are not analysed, so their chunk is in the store, and is
                 * released when its numbers are read.
                 */
                result = (m_get_chunk_func != nullptr                         &&
                          m_get_chunk_func(chunk_id,
                                          &data_ptr,
                                          &n_bytes) );

                if (result == false)
                {
                    result = APIInterceptor::get_data_chunk(chunk_id,
                                                           &data_ptr,
                                                           &n_bytes);

                    is_taken = result;
                }

                if (result                                                                   &&
                    n_bytes >= layout.n_values * static_cast<uint32_t>(sizeof(float) ) )
                {
                    ::memcpy(value_fp32vec4.data(),
                             data_ptr,
                             layout.n_values * sizeof(float) );
                }
                else
                {
                    result = false;
                }

                if (is_taken)
                {
                    APIInterceptor::release_data_chunk(chunk_id);
                }
            }
        }
        else
        {
            result = (in_n_arguments >= layout.n_values);

            for (uint32_t n_value = 0;
                          n_value < layout.n_values && result;
                        ++n_value)
            {
                result = APIInterceptor::read_number(in_arguments_ptr[n_value],
                                                    &value_fp32vec4.at(n_value) );
            }
        }
    }

    if (result)
    {
        out_call_ptr->kind           = layout.kind;
        out_call_ptr->value_fp32vec4 = value_fp32vec4;
        *out_layout_ptr              = layout;
    }

    return result;
}


bool Stereo::ConverterDLL::GLEyeRenderer::read_tracked_matrices(std::array<float, 16>* out_modelview_f32mat_ptr,
                                                                std::array<float, 16>* out_projection_f32mat_ptr,
                                                                int32_t*               out_matrix_mode_ptr)
{
    /* The first reads are all checked, then one in N_READS_BETWEEN_CHECKS. Matrices are equal if they differ by less than MATRIX_TOLERANCE of their scale. */
    constexpr uint64_t N_READS_BETWEEN_CHECKS = 256;
    constexpr uint64_t N_READS_CHECKED_AT_ONCE = 64;
    constexpr float    MATRIX_TOLERANCE        = 1e-3f;
    constexpr GLenum   PROJECTION_MATRIX       = 0x0BA7;

    uint32_t tracked_mode = 0;
    bool     result       = false;

    if (m_trusts_tracked_matrices &&
        m_get_matrices_func       != nullptr)
    {
        result = m_get_matrices_func(out_modelview_f32mat_ptr,
                                     out_projection_f32mat_ptr,
                                     &tracked_mode);
    }

    if (result)
    {
        *out_matrix_mode_ptr = static_cast<int32_t>(tracked_mode);

        if (m_n_tracked_matrix_reads <  N_READS_CHECKED_AT_ONCE ||
            m_n_tracked_matrix_reads % N_READS_BETWEEN_CHECKS == 0)
        {
            std::array<float, 16> driver_modelview_f32mat;
            std::array<float, 16> driver_projection_f32mat;
            GLint                 driver_mode = 0;
            bool                  is_equal    = true;

            GLDriverFunctions::get().get_integerv_func_ptr(GL_MATRIX_MODE,
                                                           &driver_mode);
            GLDriverFunctions::get().get_floatv_func_ptr  (GL_MODELVIEW_MATRIX,
                                                           driver_modelview_f32mat.data() );
            GLDriverFunctions::get().get_floatv_func_ptr  (PROJECTION_MATRIX,
                                                           driver_projection_f32mat.data() );

            is_equal = (driver_mode == static_cast<GLint>(tracked_mode) );

            for (size_t n_element = 0;
                        n_element < driver_modelview_f32mat.size();
                      ++n_element)
            {
                const float modelview_scale  = (std::max)(1.0f,
                                                          std::fabs(driver_modelview_f32mat.at(n_element) ) );
                const float projection_scale = (std::max)(1.0f,
                                                          std::fabs(driver_projection_f32mat.at(n_element) ) );

                if (std::fabs(driver_modelview_f32mat.at (n_element) - out_modelview_f32mat_ptr->at (n_element) ) > MATRIX_TOLERANCE * modelview_scale ||
                    std::fabs(driver_projection_f32mat.at(n_element) - out_projection_f32mat_ptr->at(n_element) ) > MATRIX_TOLERANCE * projection_scale)
                {
                    is_equal = false;
                }
            }

            if (is_equal == false)
            {
                m_trusts_tracked_matrices = false;

                note_once("tracked_matrices",
                          "The matrices that the analysis tracked differ from the ones of the driver: the driver is asked for them from now on");

                result = false;
            }
        }

        m_n_tracked_matrix_reads++;
    }

    return result;
}

bool Stereo::ConverterDLL::GLEyeRenderer::record_call(const APIInterceptor::APIFunction&         in_function,
                                                      const uint32_t&                            in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    RecordedCall       call;
    RecordedCallLayout layout;
    const bool         result = read_recorded_call(in_function,
                                                   in_n_arguments,
                                                   in_arguments_ptr,
                                                  &layout,
                                                  &call);

    if (result)
    {
        m_block_recorder.add(call.kind,
                             call.value_fp32vec4);
    }

    return result;
}

const void* Stereo::ConverterDLL::GLEyeRenderer::resolve_pointer(const APIInterceptor::APIFunctionArgument& in_argument) const
{
    const void* result = nullptr;

    if (in_argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        uint32_t n_bytes = 0;

        /* The chunk belongs to the converter now: the call analyzer took it out of the interceptor when it analysed the call. */
        if (m_get_chunk_func                                          == nullptr ||
            m_get_chunk_func(in_argument.get_data_chunk_id(),
                            &result,
                            &n_bytes) == false)
        {
            result = nullptr;
        }
    }
    else
    if (in_argument.type == APIInterceptor::ARGTYPE_VOID_PTR)
    {
        result = in_argument.get_ptr();
    }

    return result;
}

void Stereo::ConverterDLL::GLEyeRenderer::select_textures(const TextureSelection& in_selection,
                                                          const bool&             in_are_twins)
{
    /* Only the active unit can be bound without calling the function that changes the active unit. The caller holds an OwnCallScope. */
    if (in_selection.has_twin)
    {
        GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                       (in_are_twins) ? in_selection.twin
                                                                      : in_selection.texture);
    }
}

bool Stereo::ConverterDLL::GLEyeRenderer::serve_pixel_read(const uint32_t&                            in_n_arguments,
                                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t N_PIXEL_BYTES = 3; /* R, G and B of a pixel that the application reads as GL_RGB */

    SinglePixelRead read;
    const bool      is_read        = GLResolutionScaler::parse_single_pixel_read(in_n_arguments,
                                                                                 in_arguments_ptr,
                                                                                &read);
    const uint32_t  picture_height = m_scaler.get_asked_height();
    const uint32_t  picture_width  = m_scaler.get_asked_width ();
    bool            result         = false;

    if (is_read                 &&
        picture_height != 0     &&
        picture_width  != 0)
    {
        std::array<uint8_t, 4> rgba = {};

        /* This is the picture the application sees: the graphics card scales it down to the size of the application's window. */
        const auto fill_func = [this, picture_height, picture_width](std::vector<uint8_t>* out_rgba_u8_vec_ptr)
        {
            return m_targets.read_picture(picture_width,
                                          picture_height,
                                          out_rgba_u8_vec_ptr);
        };

        const bool is_served = m_readback_cache.read_pixel(fill_func,
                                                           picture_width,
                                                           picture_height,
                                                           read.x,
                                                           read.y,
                                                          &rgba);

        if (is_served)
        {
            ::memcpy(read.pixels_ptr,
                     rgba.data(),
                     N_PIXEL_BYTES);

            result = true;
        }
    }

    return result;
}

void Stereo::ConverterDLL::GLEyeRenderer::set_defers_right_eye(const bool& in_defers_right_eye)
{
    m_defers_right_eye = in_defers_right_eye;
}

void Stereo::ConverterDLL::GLEyeRenderer::set_get_chunk_func(const GetChunkFunc& in_get_chunk_func)
{
    m_get_chunk_func = in_get_chunk_func;
}

void Stereo::ConverterDLL::GLEyeRenderer::set_get_matrices_func(const GetMatricesFunc& in_get_matrices_func)
{
    m_get_matrices_func = in_get_matrices_func;
}

void Stereo::ConverterDLL::GLEyeRenderer::set_records_immediate_mode_blocks(const bool& in_records_immediate_mode_blocks)
{
    m_records_immediate_mode_blocks = in_records_immediate_mode_blocks;
}

void Stereo::ConverterDLL::GLEyeRenderer::start_depth_read()
{
    uint32_t slot = 0;

    bool is_started = m_targets.begin_depth_read(&slot);

    /* All buffers are in use. Wait for the oldest read so that one buffer is free. */
    if (is_started == false)
    {
        collect_depth_reads(false, /* in_only_older_frames */
                            true); /* in_only_the_oldest */

        is_started = m_targets.begin_depth_read(&slot);
    }

    if (is_started)
    {
        m_depth_ledger.add_pending(slot,
                                   m_pass_projection_f32mat.at(0),
                                   m_pass_projection_f32mat.at(10),
                                   m_pass_projection_f32mat.at(14) );
    }
}

bool Stereo::ConverterDLL::GLEyeRenderer::take_frame_depth(Stereo::Core::DepthRange* out_depth_range_ptr,
                                                           float*                    out_projection_scale_ptr)
{
    return m_depth_ledger.take_ready(out_depth_range_ptr,
                                     out_projection_scale_ptr);
}

std::vector<std::string> Stereo::ConverterDLL::GLEyeRenderer::take_notes()
{
    std::vector<std::string> result;

    result.swap(m_note_vec);

    return result;
}

uint32_t Stereo::ConverterDLL::GLEyeRenderer::take_unanalyzed_vertex_count()
{
    const uint32_t result = m_n_unanalyzed_vertices;

    m_n_unanalyzed_vertices = 0;

    return result;
}

void Stereo::ConverterDLL::GLEyeRenderer::try_to_prepare()
{
    const OwnCallScope own_call_scope;
    constexpr uint32_t MAX_FAILED_ATTEMPTS = 20;

    const HGLRC        context = ::wglGetCurrentContext();
    const HDC          device  = ::wglGetCurrentDC();

    if (m_has_failed == false &&
        context      != nullptr &&
        device       != nullptr)
    {
        const HWND window = ::WindowFromDC(device);
        RECT       client = {};

        if (window != nullptr)
        {
            ::GetClientRect(window,
                           &client);
        }

        const bool     was_ready     = m_targets.is_ready();
        const uint32_t asked_width   = static_cast<uint32_t>(client.right  - client.left);
        const uint32_t asked_height  = static_cast<uint32_t>(client.bottom - client.top);
        uint32_t       real_height   = asked_height;
        uint32_t       real_width    = asked_width;
        Stereo::Core::PixelRect picture;

        picture.height = asked_height;
        picture.width  = asked_width;

        /* The framebuffers are as large as the eye of the headset allows, with the proportions of the window. */
        if (asked_width  > 0 &&
            asked_height > 0)
        {
            constexpr GLenum MAX_RENDERBUFFER_SIZE = 0x84E8;

            GLint max_extent = 0;

            GLDriverFunctions::get().get_integerv_func_ptr(MAX_RENDERBUFFER_SIZE,
                                                           &max_extent);

            m_scaler.plan(asked_width,
                          asked_height,
                          static_cast<uint32_t>(max_extent),
                         &real_width,
                         &real_height,
                         &picture);
        }

        const bool is_ready = m_targets.ensure(context,
                                               real_width,
                                               real_height,
                                               picture);

        /* Until the application sets its own, the viewport and scissor box are the picture. */
        if (is_ready                           &&
            m_scaler.is_identity() == false    &&
            (real_width  != m_applied_width    ||
             real_height != m_applied_height) )
        {
            GLDriverFunctions::get().viewport_func_ptr(static_cast<GLint>(picture.x),
                                                       static_cast<GLint>(picture.y),
                                                       static_cast<GLsizei>(picture.width),
                                                       static_cast<GLsizei>(picture.height) );
            GLDriverFunctions::get().scissor_func_ptr (static_cast<GLint>(picture.x),
                                                       static_cast<GLint>(picture.y),
                                                       static_cast<GLsizei>(picture.width),
                                                       static_cast<GLsizei>(picture.height) );

            m_applied_height = real_height;
            m_applied_width  = real_width;
        }

        if (is_ready && was_ready == false)
        {
            note_once("ready",
                      "The framebuffers of the eyes are made, " + std::to_string(m_targets.get_width() ) + " x " + std::to_string(m_targets.get_height() ) );

            if (m_is_shown_elsewhere)
            {
                m_invisible_window_manager.add(window);
            }
        }
        else
        if (is_ready == false && was_ready == false)
        {
            m_n_failed_attempts++;
            m_has_failed = (m_n_failed_attempts >= MAX_FAILED_ATTEMPTS);

            note_once("failed",
                      "The framebuffers of the eyes cannot be made (a context of OpenGL without them, or a window with no size): the application is not converted");
        }
    }
}
