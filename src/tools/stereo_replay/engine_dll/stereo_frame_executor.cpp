/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_matrix_math.h"
#include "capture/api_dump_loader/api_info.h"
#include "capture/api_dump_names/function_names.h"
#include "common/macros.h"
#include "stereo/core/push_shift_planner.h"
#include "stereo/core/screen_parallax_planner.h"
#include "D3D7/d3d7_draw_rewriter.h"
#include "frame_runner.h"
#include "i_eye_executor.h"
#include "stereo_frame_executor.h"
#include <algorithm>

Stereo::Replay::EngineDLL::StereoFrameExecutor::StereoFrameExecutor()
    :m_eye_separation      (0.0f),
     m_frame_eye_separation(0.0f),
     m_frame_mode          (FrameExecution::DIRECT),
     m_host_ptr            (nullptr),
     m_hurry_ptr           (nullptr),
     m_image_sink_ptr      (nullptr),
     m_slice_skipper_ptr   (nullptr),
     m_truncate_after_call (UINT64_MAX),
     m_variant_ptr         (nullptr)
{
    /* Stub */
}

Stereo::Replay::EngineDLL::StereoFrameExecutor::~StereoFrameExecutor()
{
    /* Stub */
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::begin_frame_execution(const bool&     in_uses_history,
                                                                           const uint32_t& in_n_execution)
{
    constexpr uint32_t N_LEFT_EXECUTION  = 0;
    constexpr uint32_t N_RIGHT_EXECUTION = 1;

    bool result = true;

    /* The eye of the execution, for the objects which each eye has a copy of. */
    {
        const Analysis::Recipe::Eye eye = (m_frame_mode == FrameExecution::TWICE) ? ((in_n_execution == N_LEFT_EXECUTION) ? Analysis::Recipe::Eye::LEFT
                                                                                                                         : Analysis::Recipe::Eye::RIGHT)
                                                                                  : m_frame_execution.eye;

        set_executors_eye(eye);
    }

    /* Each execution of the last frame in the mode VARIANTS leaves out what its variant says. */
    if (m_frame_mode == FrameExecution::VARIANTS &&
        m_session_ptr != nullptr                 &&
        in_n_execution < m_frame_execution.variant_vec.size() )
    {
        std::string error;

        if (prepare_variant(m_session_ptr->get_completed_frame(),
                           &m_frame_execution.variant_vec.at(in_n_execution),
                           &error) == false)
        {
            m_host_ptr->fail(error);

            result = false;
        }
    }

    if (in_uses_history == false)
    {
        /* No carry-over: the history is stale. */
        if (in_n_execution == N_LEFT_EXECUTION)
        {
            for (auto& snapshot : m_eye_history.snapshot_array)
            {
                snapshot.has_pixels = false;
            }
        }
    }
    else
    if (in_n_execution == N_LEFT_EXECUTION)
    {
        const auto& left_snapshot  = m_eye_history.snapshot_array.at(N_LEFT_EXECUTION);
        const auto& right_snapshot = m_eye_history.snapshot_array.at(N_RIGHT_EXECUTION);

        /* The right eye starts from this until it has a history. */
        if (left_snapshot.has_pixels  == false ||
            right_snapshot.has_pixels == false)
        {
            result = read_back_buffer(&m_eye_history.base_u8_vec,
                                      nullptr);
        }

        if (result                  &&
            left_snapshot.has_pixels)
        {
            result = write_back_buffer(left_snapshot.pixel_u8_vec);
        }
    }
    else
    {
        const auto& right_snapshot = m_eye_history.snapshot_array.at(N_RIGHT_EXECUTION);

        result = write_back_buffer((right_snapshot.has_pixels) ? right_snapshot.pixel_u8_vec
                                                               : m_eye_history.base_u8_vec);
    }

    if (result == false)
    {
        m_host_ptr->fail("The back buffer cannot be kept apart for each eye.");
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::capture_execution(const uint32_t& in_n_frame,
                                                                       const uint32_t& in_n_execution,
                                                                       const bool&     in_uses_history)
{
    FrameImage            image;
    bool                  is_hurried  = false;
    bool                  is_ok       = true;
    std::vector<uint8_t>* pixel_ptr   = (in_uses_history) ? &m_eye_history.snapshot_array.at(in_n_execution).pixel_u8_vec
                                                          : nullptr;
    bool                  wants_image = false;

    if (m_hurry_ptr != nullptr)
    {
        is_hurried = m_hurry_ptr->is_hurrying(in_n_frame);
    }

    if (m_image_sink_ptr != nullptr                       &&
        is_hurried       == false                         &&
        in_n_frame       >= m_frame_execution.first_frame &&
        in_n_frame       <= m_frame_execution.last_frame)
    {
        wants_image = m_image_sink_ptr->wants_frame_image(in_n_frame,
                                                          in_n_execution);
    }

    /* A read waits for the device: it is done once for the image and the history together, and only if one is needed. */
    if (wants_image     ||
        in_uses_history)
    {
        const bool is_read = read_back_buffer(pixel_ptr,
                                              (wants_image) ? &image
                                                            : nullptr);

        if (in_uses_history)
        {
            m_eye_history.snapshot_array.at(in_n_execution).has_pixels = is_read;

            if (is_read == false)
            {
                m_host_ptr->fail("The back buffer cannot be kept apart for each eye.");

                is_ok = false;
            }
        }

        if (is_read     &&
            wants_image)
        {
            m_image_sink_ptr->on_frame_image(in_n_frame,
                                             in_n_execution,
                                             image);
        }
    }

    return is_ok;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::execute_buffered_frame(const uint32_t& in_n_frame,
                                                                            bool*           out_has_workload_ended_ptr)
{
    FrameBuffer& buffer       = m_frame_buffer;
    bool         has_boundary = false;
    bool         is_rewritten = false;
    bool         result       = true;
    bool         skips        = false;
    bool         uses_history = false;

    /* The buffer of the frame before keeps what it allocated. */
    buffer.clear();

    /* In the mode VARIANTS the frames before the last are played for the eyes, as TWICE says, so that what the eyes carry over is as it is. */
    const bool is_variants_frame = (m_frame_execution.mode == FrameExecution::VARIANTS &&
                                    in_n_frame             == m_frame_execution.last_frame);

    m_frame_mode = (is_variants_frame) ? FrameExecution::VARIANTS
                                       : ( (m_frame_execution.mode == FrameExecution::VARIANTS) ? FrameExecution::TWICE
                                                                                                : m_frame_execution.mode);
    m_variant_ptr = nullptr;

    /* A change of the separation is seen from the next frame, so that the two eyes of a frame are apart by the same distance. */
    m_frame_eye_separation = m_eye_separation;

    *out_has_workload_ended_ptr = false;

    while (has_boundary == false)
    {
        const Capture::APIDumpLoader::DumpedAPICall* call_ptr = nullptr;

        if (read_next_call(&call_ptr) == false)
        {
            *out_has_workload_ended_ptr = true;

            break;
        }

        buffer.add_call(*call_ptr,
                        m_host_ptr->get_chunk_source() );

        has_boundary = Capture::APIDumpLoader::is_frame_boundary(*call_ptr);
    }

    /* Classify the frame by the shape the analysis found. */
    if (has_boundary                     &&
        m_recipe_applier_ptr != nullptr)
    {
        const auto& frame = m_session_ptr->get_completed_frame();

        if (m_recipe_applier_ptr->is_frame_known(frame.shape_key) )
        {
            is_rewritten = true;

            if (m_frame_mode == FrameExecution::TWICE ||
                m_frame_mode == FrameExecution::VARIANTS)
            {
                std::string error;

                result = get_eye_history_need(frame,
                                             &uses_history,
                                             &error);

                if (result == false)
                {
                    m_host_ptr->fail(error);
                }
                else
                if (uses_history                           &&
                    m_frame_mode == FrameExecution::VARIANTS)
                {
                    /* Each variant would have to begin with the back buffer that the frame before left, which is not kept for more than the two eyes. */
                    m_host_ptr->fail("Frame " + std::to_string(in_n_frame) + " carries the back buffer over from the frame before, so that it cannot be played once for each of several variants.");

                    result = false;
                }
            }
        }
        else
        {
            switch (m_recipe_applier_ptr->get_unknown_frame_policy() )
            {
                case Analysis::Recipe::UnknownFramePolicy::FAIL:
                {
                    char message_i8vec128[128];

                    ::snprintf(message_i8vec128,
                               sizeof(message_i8vec128),
                               "Frame %u is of a shape that the recipe does not know (0x%016llx).",
                               in_n_frame,
                               static_cast<unsigned long long>(frame.shape_key) );

                    m_host_ptr->fail(message_i8vec128);

                    result = false;

                    break;
                }

                case Analysis::Recipe::UnknownFramePolicy::SKIP:
                {
                    skips = true;

                    break;
                }

                case Analysis::Recipe::UnknownFramePolicy::MONO:
                {
                    break;
                }

                default:
                {
                    AI_FAIL_UNSUPPORTED(m_recipe_applier_ptr->get_unknown_frame_policy() );

                    break;
                }
            }
        }
    }

    m_skipped_call_range_vec.clear();

    m_truncate_after_call = UINT64_MAX;

    m_pushed_draw_by_first_call_map.clear();

    m_view_anchor_call_index_set.clear();

    if (is_rewritten)
    {
        find_skipped_call_ranges(m_session_ptr->get_completed_frame() );
        find_pushed_draw_calls(m_session_ptr->get_completed_frame() );

        if (m_frame_execution.truncates_after_draw &&
            in_n_frame == m_frame_execution.last_frame)
        {
            find_truncation_call(m_session_ptr->get_completed_frame(),
                                 m_frame_execution.truncate_pass_key,
                                 m_frame_execution.truncate_draw_key);
        }

        /* A frame that a recipe with a view anchor shift cannot give an eye is not played for one: it would be mono, and nobody would have said so. */
        if (m_recipe_applier_ptr->has_view_anchor_shift() )
        {
            std::string error;

            if (find_view_anchor_calls(m_session_ptr->get_completed_frame(),
                                      &error) == false)
            {
                m_host_ptr->fail(error);

                result = false;
            }
        }
    }

    /* The device is as the calls of a frame played as they are leave it, not as the binder believes. */
    if (is_rewritten        == false &&
        m_matrix_binder_ptr != nullptr)
    {
        m_matrix_binder_ptr->forget();
    }

    bool has_buffered_calls = false;

    if (result)
    {
        const auto& buffered_call_vec = buffer.get_call_vec();

        has_buffered_calls = (buffered_call_vec.empty() == false);
    }

    if (has_buffered_calls)
    {
        /* The frame is played more than once only if it is complete and nothing leaves it out, as the runner has it. */
        FrameExecutionOptions run_options = m_frame_execution;

        run_options.mode = m_frame_mode;

        uint32_t n_executions = 1;

        if (has_boundary &&
            skips == false)
        {
            if (m_frame_mode == FrameExecution::TWICE)
            {
                n_executions = 2;
            }
            else
            if (m_frame_mode == FrameExecution::VARIANTS)
            {
                n_executions = std::max<uint32_t>(1,
                                                  static_cast<uint32_t>(run_options.variant_vec.size() ) );
            }
        }

        set_executors_n_executions(n_executions);

        const auto begin_execution_func = [this, uses_history](const uint32_t& in_n_execution)
        {
            return begin_frame_execution(uses_history,
                                         in_n_execution);
        };

        const auto get_call_executor_func = [this](const Capture::APIDumpLoader::DumpedAPICall& in_call)
        {
            return m_host_ptr->get_call_executor_ptr(Capture::APIDumpLoader::get_api_group(in_call.func) );
        };

        const auto execute_call_func = [this, is_rewritten](const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                            const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                            const size_t&                                  in_n_call,
                                                            const uint32_t&                                in_n_execution)
        {
            bool is_ok = false;

            if (is_rewritten)
            {
                is_ok = execute_rewritten_call(in_call,
                                               in_chunk_source,
                                               in_n_call,
                                               in_n_execution);
            }
            else
            {
                is_ok = m_host_ptr->execute_call(in_call,
                                                 in_chunk_source);
            }

            return is_ok;
        };

        const auto capture_execution_func = [this, in_n_frame, uses_history](const uint32_t& in_n_execution)
        {
            return capture_execution(in_n_frame,
                                     in_n_execution,
                                     uses_history);
        };

        result = FrameRunner::run(buffer,
                                  has_boundary,
                                  skips,
                                  run_options,
                                  m_host_ptr->get_call_executor_ptr_vec(),
                                  begin_execution_func,
                                  get_call_executor_func,
                                  execute_call_func,
                                  capture_execution_func);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::execute_direct_frame(const uint32_t& in_n_frame,
                                                                          bool*           out_has_workload_ended_ptr)
{
    bool has_boundary = false;
    bool result       = true;

    set_executors_n_executions(1);

    *out_has_workload_ended_ptr = false;

    while (has_boundary == false)
    {
        const Capture::APIDumpLoader::DumpedAPICall* call_ptr = nullptr;

        if (read_next_call(&call_ptr) == false)
        {
            *out_has_workload_ended_ptr = true;

            break;
        }

        has_boundary = Capture::APIDumpLoader::is_frame_boundary(*call_ptr);

        if (has_boundary)
        {
            capture_execution(in_n_frame,
                              0,      /* in_n_execution */
                              false); /* in_uses_history */
        }

        if (m_host_ptr->execute_call(*call_ptr,
                                     m_host_ptr->get_chunk_source() ) == false)
        {
            result = false;

            break;
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::execute_draw_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                                       const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                                       const bool&                                    in_is_affected,
                                                                       const bool&                                    in_is_presented,
                                                                       const uint64_t&                                in_cover_key,
                                                                       const Analysis::Recipe::Eye&                   in_eye,
                                                                       const uint32_t&                                in_pass_structure_id)
{
    Stereo::Core::ScreenParallaxPlanner::Input input;
    bool                                       is_moved  = false;
    const uint64_t                             shape_key = m_session_ptr->get_completed_frame().shape_key;

    OverlayChunkSource overlay(in_chunk_source);

    const float                           picture_width = m_session_ptr->get_model().pass_structure_interner.get_structure(in_pass_structure_id).viewport.width;
    Stereo::Core::ScreenShiftPlan         plan;
    Capture::APIDumpLoader::DumpedAPICall rewritten;

    /* The scene (the kind of frame) knows where its surfaces are, and how far apart the eyes may be for the nearest of them. */
    input.convergence_distance = m_recipe_applier_ptr->get_scene_convergence_distance(shape_key);
    input.cover_key            = in_cover_key;
    input.eye                  = in_eye;
    input.eye_separation       = m_frame_eye_separation;
    input.has_extent           = Stereo::Replay::EngineDLL::D3D7DrawRewriter::find_extent(in_call,
                                                                                          in_chunk_source,
                                                                                          picture_width,
                                                                                         &input.reaches_both_sides);
    input.is_affected          = in_is_affected;
    input.is_overlay_pushed    = true;
    input.is_presented         = in_is_presented;
    input.picture_width        = picture_width;
    input.separation_limit     = m_recipe_applier_ptr->get_scene_max_separation(shape_key);

    plan = Stereo::Core::ScreenParallaxPlanner::plan(*m_recipe_applier_ptr,
                                                     input);

    switch (plan.kind)
    {
        case Stereo::Core::ScreenShiftKind::NONE:
        {
            break;
        }

        case Stereo::Core::ScreenShiftKind::PER_VERTEX:
        {
            Stereo::D3D7::ScreenParallax::Parameters parameters;

            parameters.convergence_distance  = plan.convergence_distance;
            parameters.eye                   = plan.eye;
            parameters.eye_separation        = plan.eye_separation;
            parameters.field_of_view_degrees = plan.field_of_view_degrees;
            parameters.picture_width         = plan.picture_width;

            is_moved = D3D7DrawRewriter::rewrite(in_call,
                                                   in_chunk_source,
                                                   parameters,
                                                  &overlay,
                                                  &rewritten);

            break;
        }

        case Stereo::Core::ScreenShiftKind::UNIFORM:
        {
            is_moved = D3D7DrawRewriter::rewrite_by(in_call,
                                                      in_chunk_source,
                                                      plan.uniform_shift_pixels,
                                                     &overlay,
                                                     &rewritten);

            break;
        }

        default:
        {
            AI_FAIL_UNSUPPORTED(plan.kind);
        }
    }

    return (is_moved) ? m_host_ptr->execute_call(rewritten,
                                                 overlay)
                      : m_host_ptr->execute_call(in_call,
                                                 in_chunk_source);
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::execute_frame(::Replay::ReplayEngineDLL::IFrameHost* in_host_ptr,
                                                                   bool*                                  out_has_workload_ended_ptr)
{
    bool           is_buffered = false;
    bool           is_dead     = false;
    const uint32_t n_frame     = in_host_ptr->get_n_frames_presented();
    bool           result      = false;

    /* A frame which a slice leaves out has no content which anything reads: it is played as it is, with only the calls that set state, which is quicker */
    if (m_slice_skipper_ptr != nullptr)
    {
        is_dead = m_slice_skipper_ptr->is_dead_frame(n_frame);
    }

    is_buffered = (m_frame_execution.mode != FrameExecution::DIRECT        &&
                   is_dead                == false                         &&
                   n_frame                >= m_frame_execution.first_frame &&
                   n_frame                <= m_frame_execution.last_frame);

    m_host_ptr = in_host_ptr;

    if (is_buffered)
    {
        result = execute_buffered_frame(n_frame,
                                        out_has_workload_ended_ptr);
    }
    else
    {
        /* A frame played as it is leaves the device as the application sets it, which the binder does not know. */
        if (m_matrix_binder_ptr != nullptr)
        {
            m_matrix_binder_ptr->forget();
        }

        result = execute_direct_frame(n_frame,
                                      out_has_workload_ended_ptr);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::execute_rewritten_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                                            const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                                            const size_t&                                  in_n_call,
                                                                            const uint32_t&                                in_n_execution)
{
    const uint64_t              call_index = m_session_ptr->get_completed_frame().first_call_index + in_n_call;
    Analysis::Recipe::Eye       eye        = m_frame_execution.eye;
    const auto&                 frame      = m_session_ptr->get_completed_frame();
    uint32_t                    pass_index = 0;
    Analysis::Recipe::CallPlace place;
    bool                        result     = false;

    /* Executed twice: the first execution is the left eye, the second the right one. */
    if (m_frame_execution.mode == FrameExecution::TWICE)
    {
        eye = (in_n_execution == 0) ? Analysis::Recipe::Eye::LEFT
                                    : Analysis::Recipe::Eye::RIGHT;
    }

    const auto is_draw_before_call_func = [](const Analysis::APIDumpAnalysis::DrawInstance& in_draw,
                                             const uint64_t&                                in_call_index)
    {
        return (in_draw.call_index < in_call_index);
    };

    /* The draws are in the order of the calls. */
    const auto draw_iterator = std::lower_bound(frame.draw_instance_vec.begin(),
                                                frame.draw_instance_vec.end  (),
                                                call_index,
                                                is_draw_before_call_func);

    /* The first call of a draw that the recipe pushes moves the viewport sideways, so that all of the calls of the draw (the glBegin and the vertices of a block
     * that a glEnd makes a draw of) are made with it; the call of the draw puts it back.
     */
    bool       has_pushed_viewport = true;
    const auto pushed_iterator     = m_pushed_draw_by_first_call_map.find(call_index);

    if (pushed_iterator != m_pushed_draw_by_first_call_map.end() )
    {
        const Analysis::APIDumpAnalysis::Viewport* viewport_ptr = nullptr;
        const float                                shift_pixels = get_push_shift_pixels(frame,
                                                                                        frame.draw_instance_vec.at(pushed_iterator->second),
                                                                                        eye,
                                                                                       &viewport_ptr);

        if (shift_pixels != 0.0f)
        {
            has_pushed_viewport = execute_viewport_call(in_call,
                                                        *viewport_ptr,
                                                        shift_pixels);
        }
    }

    if (has_pushed_viewport == false)
    {
        result = false;
    }
    else
    if (is_call_skipped(call_index) )
    {
        /* A call of a draw that is left out, before the call of the draw. */
        result = true;
    }
    else
    if (m_view_anchor_call_index_set.count(call_index) != 0)
    {
        /* The call that begins the camera of draws that are affected: it loads the shift of the eye. */
        result = execute_view_anchor_call(in_call,
                                          in_chunk_source,
                                          eye);
    }
    else
    if (draw_iterator             != frame.draw_instance_vec.end() &&
        draw_iterator->call_index == call_index)
    {
        const uint64_t pass_key = frame.pass_key_vec.at(draw_iterator->pass_index);

        /* A draw which is skipped needs nothing on the device. */
        if (is_draw_skipped_here(pass_key,
                                                  draw_iterator->structure_key) )
        {
            result = true;
        }
        else
        {
            const bool is_affected = is_draw_affected_here(pass_key,
                                                           *draw_iterator);

            /* What the draw is made with is put on the device first. */
            result = m_matrix_binder_ptr->before_draw(is_affected,
                                                      eye,
                                                      m_frame_eye_separation);

            if (result)
            {
                result = execute_draw_call(in_call,
                                           in_chunk_source,
                                           is_affected,
                                           (draw_iterator->pass_index < frame.pass_is_presented_vec.size() ) ? frame.pass_is_presented_vec.at(draw_iterator->pass_index)
                                                                                                             : false,
                                           draw_iterator->cover_key,
                                           eye,
                                           frame.pass_instance_vec.at(draw_iterator->pass_index).structure_id);
            }

            /* The viewport that was moved for a draw that the recipe pushes (before its first call) is put back. */
            if (result                                                                      &&
                m_pushed_draw_by_first_call_map.empty() == false)
            {
                const Analysis::APIDumpAnalysis::Viewport* viewport_ptr = nullptr;

                if (get_push_shift_pixels(frame,
                                          *draw_iterator,
                                          eye,
                                         &viewport_ptr) != 0.0f)
                {
                    result = execute_viewport_call(in_call,
                                                   *viewport_ptr,
                                                   0.0f);
                }
            }
        }
    }
    else
    if (m_recipe_applier_ptr->has_rule_for(in_call.func) == false)
    {
        /* Most calls are not about a matrix: they are played as they are, without looking for their pass. */
        result = m_host_ptr->execute_call(in_call,
                                          in_chunk_source);
    }
    else
    {
        place.shape_key = frame.shape_key;

        if (Analysis::APIDumpAnalysis::ModelBuilder::find_pass_index(frame,
                                                                     call_index,
                                                                    &pass_index) )
        {
            place.has_pass = true;
            place.pass_key = frame.pass_key_vec.at(pass_index);
        }

        /* A call which sets a matrix the recipe changes waits for the draw which needs it. */
        result = true;

        if (m_matrix_binder_ptr->on_call(in_call,
                                         in_chunk_source,
                                         place,
                                         eye,
                                         m_frame_eye_separation) == false)
        {
            result = m_host_ptr->execute_call(in_call,
                                              in_chunk_source);
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::execute_view_anchor_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                                              const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source,
                                                                              const Analysis::Recipe::Eye&                   in_eye)
{
    constexpr size_t   MATRIX_N_ELEMENTS = 16;
    constexpr uint32_t SHIFT_CHUNK_ID    = 0xfffffff1; /* no chunk of a workload has an ID this big */

    const auto                                        shift = m_recipe_applier_ptr->make_view_anchor_matrix(in_eye,
                                                                                                           m_frame_eye_separation,
                                                                                                           m_session_ptr->get_completed_frame().shape_key);
    Analysis::APIDumpAnalysis::Frontends::GlMatrix    loaded = shift;
    Capture::APIDumpLoader::DumpedAPICall             rewritten;
    bool                                              result = false;

    OverlayChunkSource overlay(in_chunk_source);

    /* What the call loads is what the camera of the program is made on, and the shift of the eye goes on top of it (first, as the left factor): a glLoadIdentity loads
     * nothing, so that the shift is all that is loaded; a matrix that the program loaded has the shift put before it. The call that loads is a glLoadMatrixf then.
     */
    switch (in_call.func)
    {
        case APIInterceptor::APIFUNCTION_GL_GLLOADIDENTITY:
        {
            result = true;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD:
        case APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF:
        {
            const bool                               is_double = (in_call.func == APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXD);
            const Capture::APIDumpLoader::DataChunk* chunk_ptr = nullptr;

            if (in_call.arg_in_vec.empty()                                                  == false &&
                in_call.arg_in_vec.at(0).type                                               == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
            {
                chunk_ptr = in_chunk_source.get_data_chunk_ptr(in_call.arg_in_vec.at(0).get_data_chunk_id() );
            }

            if (chunk_ptr != nullptr                                                                           &&
                chunk_ptr->size() == MATRIX_N_ELEMENTS * ((is_double) ? sizeof(double)
                                                                      : sizeof(float) ) )
            {
                Analysis::APIDumpAnalysis::Frontends::GlMatrix program_matrix = {};

                for (size_t n_element = 0;
                            n_element < MATRIX_N_ELEMENTS;
                          ++n_element)
                {
                    if (is_double)
                    {
                        double element = 0.0;

                        ::memcpy(&element,
                                 chunk_ptr->data() + n_element * sizeof(double),
                                 sizeof(double) );

                        program_matrix.at(n_element) = static_cast<float>(element);
                    }
                    else
                    {
                        ::memcpy(&program_matrix.at(n_element),
                                 chunk_ptr->data() + n_element * sizeof(float),
                                 sizeof(float) );
                    }
                }

                loaded = Analysis::APIDumpAnalysis::Frontends::GlMatrixMath::multiply(shift,
                                                                                      program_matrix);
                result = true;
            }
            else
            {
                m_host_ptr->fail("The matrix that begins the camera of a draw that the recipe affects cannot be read from the call.");
            }

            break;
        }

        default:
        {
            m_host_ptr->fail(std::string("The camera of a draw that the recipe affects begins with ")        +
                             Capture::APIDumpNames::get_function_name(in_call.func)                          +
                             ", which cannot take the shift of an eye: only a glLoadIdentity, a glLoadMatrixf or a glLoadMatrixd can.");

            break;
        }
    }

    if (result)
    {
        Capture::APIDumpLoader::DataChunk shift_chunk(sizeof(loaded) );

        ::memcpy(shift_chunk.data(),
                 loaded.data(),
                 sizeof(loaded) );

        overlay.set_chunk(SHIFT_CHUNK_ID,
                          shift_chunk);

        rewritten      = FrameBuffer::copy_call(in_call);
        rewritten.func = APIInterceptor::APIFUNCTION_GL_GLLOADMATRIXF;

        rewritten.arg_in_vec.assign(1,
                                    APIInterceptor::APIFunctionArgument::create_data_chunk_id(SHIFT_CHUNK_ID) );

        result = m_host_ptr->execute_call(rewritten,
                                          overlay);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::execute_viewport_call(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                                                            const Analysis::APIDumpAnalysis::Viewport&     in_viewport,
                                                                            const float&                                   in_shift_pixels)
{
    Capture::APIDumpLoader::DumpedAPICall rewritten = FrameBuffer::copy_call(in_call);

    rewritten.func = APIInterceptor::APIFUNCTION_GL_GLVIEWPORT;

    rewritten.arg_in_vec.clear ();
    rewritten.arg_out_vec.clear();
    rewritten.returned_value_ptr.reset();

    rewritten.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_i32(static_cast<int32_t>(::lroundf(in_viewport.x) + static_cast<int32_t>(::truncf(in_shift_pixels) ) ) ) );
    rewritten.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_i32(static_cast<int32_t>(::lroundf(in_viewport.y) ) ) );
    rewritten.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_i32(static_cast<int32_t>(::lroundf(in_viewport.width) ) ) );
    rewritten.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_i32(static_cast<int32_t>(::lroundf(in_viewport.height) ) ) );

    return m_host_ptr->execute_call(rewritten,
                                    m_host_ptr->get_chunk_source() );
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::find_pushed_draw_calls(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame)
{
    m_pushed_draw_by_first_call_map.clear();

    for (size_t n_draw = 0;
                n_draw < in_frame.draw_instance_vec.size();
              ++n_draw)
    {
        const auto&    draw     = in_frame.draw_instance_vec.at(n_draw);
        const uint64_t pass_key = in_frame.pass_key_vec.at(draw.pass_index);

        if (is_draw_pushed_here(pass_key,
                                draw)                  &&
            is_draw_skipped_here(pass_key,
                                 draw.structure_key) == false)
        {
            /* The calls of a draw begin with its own, unless the front end says that they begin earlier. */
            const uint64_t first_call = (draw.first_call_index < draw.call_index) ? draw.first_call_index
                                                                                  : draw.call_index;

            m_pushed_draw_by_first_call_map[first_call] = static_cast<uint32_t>(n_draw);
        }
    }
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::find_skipped_call_ranges(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame)
{
    m_skipped_call_range_vec.clear();

    for (const auto& draw : in_frame.draw_instance_vec)
    {
        const uint64_t pass_key = in_frame.pass_key_vec.at(draw.pass_index);

        if (draw.first_call_index < draw.call_index                               &&
            is_draw_skipped_here(pass_key,
                                                  draw.structure_key) )
        {
            m_skipped_call_range_vec.push_back({draw.first_call_index,
                                                draw.call_index - 1});
        }
    }
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::find_truncation_call(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                                                          const uint64_t&                                  in_truncate_pass_key,
                                                                          const uint64_t&                                  in_truncate_draw_key)
{
    for (const auto& draw : in_frame.draw_instance_vec)
    {
        const uint64_t pass_key = in_frame.pass_key_vec.at(draw.pass_index);

        if (pass_key             == in_truncate_pass_key &&
            draw.structure_key   == in_truncate_draw_key)
        {
            /* The last of them: the draws are in the order of the calls. */
            m_truncate_after_call = draw.call_index;
        }
    }
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::find_view_anchor_calls(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                                                            std::string*                                     out_error_ptr)
{
    std::set<uint64_t>                          affected_anchor_set;
    std::set<std::pair<uint64_t, uint64_t> >    anchorless_draw_set; /* the pass and the draw, by their keys */
    std::set<uint64_t>                          unaffected_anchor_set;
    bool                                        result = true;

    m_unflattened_draw_call_set.clear();
    m_view_anchor_call_index_set.clear();

    /* The replay gives the shift of an eye to a camera at its beginning, and all the draws that share it get it: a draw that is flat and shares the camera of a draw that is not keeps the rules (the live converter does not have the limit). */
    {
        std::set<uint64_t> shared_anchor_set;

        for (const auto& draw : in_frame.draw_instance_vec)
        {
            const uint64_t pass_key = in_frame.pass_key_vec.at(draw.pass_index);

            if (draw.view_anchor_call_index           != UINT64_MAX                                                                  &&
                is_draw_skipped_here(pass_key,
                                     draw.structure_key)                                      == false                                       &&
                m_recipe_applier_ptr->is_draw_affected(pass_key,
                                                       draw.structure_key)                      &&
                m_recipe_applier_ptr->is_cover_pushed (draw.cover_key)                         == false)
            {
                shared_anchor_set.insert(draw.view_anchor_call_index);
            }
        }

        for (const auto& draw : in_frame.draw_instance_vec)
        {
            if (shared_anchor_set.count(draw.view_anchor_call_index) != 0 &&
                m_recipe_applier_ptr->is_cover_pushed(draw.cover_key))
            {
                m_unflattened_draw_call_set.insert(draw.call_index);
            }
        }
    }

    for (const auto& draw : in_frame.draw_instance_vec)
    {
        const uint64_t pass_key = in_frame.pass_key_vec.at(draw.pass_index);

        /* A draw that is left out needs no camera. */
        if (is_draw_skipped_here(pass_key,
                                                  draw.structure_key) )
        {
            continue;
        }

        const bool is_affected = is_draw_affected_here(pass_key,
                                                       draw);

        if (draw.view_anchor_call_index != UINT64_MAX)
        {
            if (is_affected)
            {
                affected_anchor_set.insert(draw.view_anchor_call_index);
            }
            else
            {
                unaffected_anchor_set.insert(draw.view_anchor_call_index);
            }
        }
        else
        if (is_affected                                                                      &&
            m_recipe_applier_ptr->get_view_anchor_shift().leaves_draws_without_anchor == false)
        {
            anchorless_draw_set.insert({pass_key,
                                        draw.structure_key});
        }
    }

    /* All the kinds of draw are told, so that a recipe can be made to leave them alone in one go. */
    if (anchorless_draw_set.empty() == false)
    {
        char key_i8vec64[64];

        *out_error_ptr = "Frame "                                                                                                            +
                         std::to_string(in_frame.frame_index)                                                                                +
                         ": draws that the recipe affects have a camera that does not begin at a glLoadIdentity at the bottom of the "       +
                         "model-view stack, so that nothing can give them the shift of an eye; the recipe has to leave them alone (unaffected):";

        for (const auto& anchorless_draw : anchorless_draw_set)
        {
            ::snprintf(key_i8vec64,
                       sizeof(key_i8vec64),
                       " pass 0x%016llx draw 0x%016llx;",
                       static_cast<unsigned long long>(anchorless_draw.first),
                       static_cast<unsigned long long>(anchorless_draw.second) );

            *out_error_ptr += key_i8vec64;
        }

        result = false;
    }

    for (const uint64_t anchor : affected_anchor_set)
    {
        if (result == false)
        {
            break;
        }

        if (unaffected_anchor_set.count(anchor) != 0)
        {
            *out_error_ptr = "Frame "                                                                                                +
                             std::to_string(in_frame.frame_index)                                                                    +
                             ": the camera of draws that are affected by the recipe, and of draws that are not, begins with the same " +
                             "call (" + std::to_string(anchor) + "), which can give it the shift of an eye or not, and not both.";

            result = false;
        }
    }

    if (result)
    {
        m_view_anchor_call_index_set = affected_anchor_set;
    }

    return result;
}


bool Stereo::Replay::EngineDLL::StereoFrameExecutor::get_eye_history_need(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                                                          bool*                                            out_needs_history_ptr,
                                                                          std::string*                                     out_error_ptr) const
{
    const auto& model  = m_session_ptr->get_model();
    bool        result = true;

    *out_needs_history_ptr = false;

    for (const auto& id : in_frame.loop_carried_resource_id_vec)
    {
        const auto& resource = model.resource_vec.at(Analysis::APIDumpAnalysis::get_resource_id_value(id) - 1);

        const bool is_swap_chain_image = (resource.description.kind == Analysis::APIDumpAnalysis::ResourceKind::SWAP_CHAIN_IMAGE);

        if (is_swap_chain_image)
        {
            /* The player keeps the pictures of the eyes apart. */
            *out_needs_history_ptr = true;
        }
        else
        {
            Analysis::APIDumpAnalysis::ResourceIdentity identity;
            bool                                        is_per_eye = false;

            /* Each eye has a copy of a texture that the recipe says it has a copy of, so that nothing is carried over from one to the other. */
            if (Analysis::APIDumpAnalysis::ResourceIdentities::find(model.resource_vec,
                                                                    id,
                                                                   &identity) )
            {
                is_per_eye = m_recipe_applier_ptr->is_per_eye_texture(identity);
            }

            if (is_per_eye == false)
            {
                *out_error_ptr = "Frame "                                                                                                                        +
                                 std::to_string(in_frame.frame_index)                                                                                            +
                                 " carries "                                                                                                                     +
                                 resource.debug_name                                                                                                             +
                                 " over from the previous frame, which cannot be rendered for two eyes: the recipe does not say that each eye has a copy of it.";

                result = false;

                break;
            }
        }
    }

    return result;
}

float Stereo::Replay::EngineDLL::StereoFrameExecutor::get_eye_separation() const
{
    return m_eye_separation;
}

float Stereo::Replay::EngineDLL::StereoFrameExecutor::get_push_shift_pixels(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                                                          const Analysis::APIDumpAnalysis::DrawInstance&   in_draw,
                                                                          const Analysis::Recipe::Eye&                     in_eye,
                                                                          const Analysis::APIDumpAnalysis::Viewport**      out_viewport_ptr_ptr) const
{
    const uint64_t pass_key          = in_frame.pass_key_vec.at(in_draw.pass_index);
    const uint32_t pass_structure_id = in_frame.pass_instance_vec.at(in_draw.pass_index).structure_id;
    float          result            = 0.0f;

    *out_viewport_ptr_ptr = &m_session_ptr->get_model().pass_structure_interner.get_structure(pass_structure_id).viewport;

    if (is_draw_pushed_here(pass_key,
                            in_draw) )
    {
        result = Stereo::Core::PushShiftPlanner::plan(*m_recipe_applier_ptr,
                                                      in_eye,
                                                      m_recipe_applier_ptr->get_cover_push_parallax(in_draw.cover_key),
                                                      is_draw_affected_here(pass_key,
                                                                            in_draw),
                                                      (*out_viewport_ptr_ptr)->width,
                                                      m_frame_eye_separation,
                                                      m_recipe_applier_ptr->get_scene_max_separation(in_frame.shape_key) );
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::is_draw_affected_here(const uint64_t&                                in_pass_key,
                                                                           const Analysis::APIDumpAnalysis::DrawInstance& in_draw) const
{
    const bool is_affected_by_recipe = m_recipe_applier_ptr->is_draw_affected(in_pass_key,
                                                                              in_draw.structure_key);

    const bool is_flat = (m_recipe_applier_ptr->is_cover_pushed(in_draw.cover_key) &&
                          m_unflattened_draw_call_set.count(in_draw.call_index) == 0);

    return (is_affected_by_recipe &&
            is_flat == false);
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::is_draw_pushed_here(const uint64_t&                                in_pass_key,
                                                                         const Analysis::APIDumpAnalysis::DrawInstance& in_draw) const
{
    const bool is_pushed_by_name = m_recipe_applier_ptr->is_draw_pushed(in_pass_key,
                                                                        in_draw.structure_key);

    return (is_pushed_by_name ||
            m_recipe_applier_ptr->is_cover_pushed(in_draw.cover_key) );
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::is_draw_skipped_here(const uint64_t& in_pass_key,
                                                                          const uint64_t& in_draw_key) const
{
    bool result = m_recipe_applier_ptr->is_draw_skipped(in_pass_key,
                                                        in_draw_key);

    /* What the variant leaves out is left out as well. */
    if (result         == false &&
        m_variant_ptr  != nullptr)
    {
        result = (std::find(m_variant_ptr->skipped_pass_key_vec.begin(),
                            m_variant_ptr->skipped_pass_key_vec.end  (),
                            in_pass_key) != m_variant_ptr->skipped_pass_key_vec.end() );

        for (size_t n_pass_draw = 0;
                    n_pass_draw < m_variant_ptr->skipped_pass_draw_vec.size() && result == false;
                  ++n_pass_draw)
        {
            const auto& pass_draw = m_variant_ptr->skipped_pass_draw_vec.at(n_pass_draw);

            result = (pass_draw.pass_key == in_pass_key &&
                      pass_draw.draw_key == in_draw_key);
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::is_call_skipped(const uint64_t& in_call_index) const
{
    /* The ranges do not overlap, and are in the order of the calls. */
    const auto is_before_range_func = [](const uint64_t&                          in_index,
                                         const std::pair<uint64_t, uint64_t>&     in_range)
    {
        return (in_index < in_range.first);
    };

    const auto range_iterator = std::upper_bound(m_skipped_call_range_vec.begin(),
                                                 m_skipped_call_range_vec.end  (),
                                                 in_call_index,
                                                 is_before_range_func);
    bool       result         = false;

    if (range_iterator != m_skipped_call_range_vec.begin() )
    {
        const auto& range = *(range_iterator - 1);

        result = (in_call_index <= range.second);
    }

    /* What comes after the call that the frame is cut at. */
    if (m_truncate_after_call != UINT64_MAX &&
        in_call_index          > m_truncate_after_call)
    {
        result = true;
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::is_per_eye_texture(const Capture::APIDumpLoader::APIGroup& in_group,
                                                                        const uint64_t&                         in_address) const
{
    Analysis::APIDumpAnalysis::ResourceIdentity identity;
    Analysis::APIDumpAnalysis::ResourceView     view;
    bool                                        result   = false;

    /* The analysis has followed the call that made the object by the time the object is asked about: calls are read before they are played. */
    if (m_recipe_applier_ptr != nullptr &&
        m_session_ptr        != nullptr)
    {
        if (m_session_ptr->find_resource_view(in_group,
                                              in_address,
                                             &view) )
        {
            if (Analysis::APIDumpAnalysis::ResourceIdentities::find(m_session_ptr->get_model().resource_vec,
                                                                    view.resource_id,
                                                                   &identity) )
            {
                result = m_recipe_applier_ptr->is_per_eye_texture(identity);
            }
        }
    }

    return result;
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::on_frame_presented()
{
    /* What is not rendered for two eyes is rendered for the left one. */
    set_executors_eye(Analysis::Recipe::Eye::LEFT);
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::on_rewind()
{
    if (m_recipe_applier_ptr != nullptr)
    {
        m_matrix_binder_ptr->forget();

        m_session_ptr.reset(new Analysis::APIDumpAnalysis::Frontends::AnalysisSession(false) ); /* in_keeps_history */
    }

    m_eye_history = EyeHistory();
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::prepare_variant(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
                                                                     const FrameVariant*                              in_variant_ptr,
                                                                     std::string*                                     out_error_ptr)
{
    bool result = true;

    m_variant_ptr = in_variant_ptr;

    find_skipped_call_ranges(in_frame);
    find_pushed_draw_calls  (in_frame);

    /* The frame is cut as the variant says, if it does; otherwise as the options of the frame say, if they do. */
    m_truncate_after_call = UINT64_MAX;

    if (in_variant_ptr != nullptr &&
        in_variant_ptr->truncates_after_draw)
    {
        find_truncation_call(in_frame,
                             in_variant_ptr->truncate_pass_key,
                             in_variant_ptr->truncate_draw_key);
    }
    else
    if (m_frame_execution.truncates_after_draw)
    {
        find_truncation_call(in_frame,
                             m_frame_execution.truncate_pass_key,
                             m_frame_execution.truncate_draw_key);
    }

    /* A draw that is left out needs no camera, so the calls that begin the cameras are those of the draws that are played. */
    if (m_recipe_applier_ptr->has_view_anchor_shift() )
    {
        result = find_view_anchor_calls(in_frame,
                                        out_error_ptr);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::read_back_buffer(std::vector<uint8_t>* out_pixel_u8_vec_ptr,
                                                                      FrameImage*           out_image_ptr)
{
    bool result = false;

    for (const auto executor_ptr : m_host_ptr->get_call_executor_ptr_vec() )
    {
        if (executor_ptr->read_back_buffer(out_pixel_u8_vec_ptr,
                                           out_image_ptr) )
        {
            result = true;

            break;
        }
    }

    return result;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::read_next_call(const Capture::APIDumpLoader::DumpedAPICall** out_call_ptr_ptr)
{
    const bool result = m_host_ptr->read_next_call(out_call_ptr_ptr);

    /* The analysis follows every call, whether or not the frame is rewritten, since what it tracks depends on all of them. */
    if (result                   &&
        m_session_ptr != nullptr)
    {
        m_session_ptr->process_call(**out_call_ptr_ptr,
                                    m_host_ptr->get_segment() );
    }

    return result;
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::set_executors_eye(const Analysis::Recipe::Eye& in_eye)
{
    for (const auto executor_ptr : m_host_ptr->get_call_executor_ptr_vec() )
    {
        IEyeExecutor* const eye_executor_ptr = dynamic_cast<IEyeExecutor*>(executor_ptr);

        if (eye_executor_ptr != nullptr)
        {
            eye_executor_ptr->set_eye(in_eye);
        }
    }
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::set_executors_n_executions(const uint32_t& in_n_executions)
{
    for (const auto executor_ptr : m_host_ptr->get_call_executor_ptr_vec() )
    {
        IEyeExecutor* const eye_executor_ptr = dynamic_cast<IEyeExecutor*>(executor_ptr);

        if (eye_executor_ptr != nullptr)
        {
            eye_executor_ptr->set_n_executions(in_n_executions);
        }
    }
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::set_eye_separation(const float& in_eye_separation)
{
    m_eye_separation = in_eye_separation;
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::set_frame_execution(const FrameExecutionOptions& in_options)
{
    m_frame_execution = in_options;
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::set_hurry(const FrameHurry* in_hurry_ptr)
{
    m_hurry_ptr = in_hurry_ptr;
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::set_image_sink(IFrameImageSink* in_sink_ptr)
{
    m_image_sink_ptr = in_sink_ptr;
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::set_recipe(const Analysis::Recipe::Recipe& in_recipe)
{
    m_recipe_applier_ptr.reset(new Analysis::Recipe::RecipeApplier(in_recipe) );
    m_rewriter_ptr.reset      (new FrameRewriter                  (*m_recipe_applier_ptr) );

    const auto execute_call_func = [this](const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                          const ::Replay::ReplayEngineDLL::IChunkSource& in_chunk_source)
    {
        return m_host_ptr->execute_call(in_call,
                                        in_chunk_source);
    };

    m_matrix_binder_ptr.reset (new EyeMatrixBinder                                      (*m_rewriter_ptr,
                                                                                         execute_call_func) );
    m_session_ptr.reset       (new Analysis::APIDumpAnalysis::Frontends::AnalysisSession(false) ); /* in_keeps_history */
}

void Stereo::Replay::EngineDLL::StereoFrameExecutor::set_slice_skipper(const SliceSkipper* in_slice_skipper_ptr)
{
    m_slice_skipper_ptr = in_slice_skipper_ptr;
}

bool Stereo::Replay::EngineDLL::StereoFrameExecutor::write_back_buffer(const std::vector<uint8_t>& in_pixel_u8_vec)
{
    bool result = false;

    for (const auto executor_ptr : m_host_ptr->get_call_executor_ptr_vec() )
    {
        if (executor_ptr->write_back_buffer(in_pixel_u8_vec) )
        {
            result = true;

            break;
        }
    }

    return result;
}
