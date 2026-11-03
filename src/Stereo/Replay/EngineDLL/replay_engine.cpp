/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/frontends/analysis_session.h"
#include "Common/macros.h"
#include "Stereo/Core/screen_parallax_planner.h"
#include "d3d7_call_executor.h"
#include "d3d7_screen_parallax.h"
#include "d3d8_call_executor.h"
#include "frame_runner.h"
#include "gl_call_executor.h"
#include "null_call_executor.h"
#include "replay_engine.h"
#include "win32_call_executor.h"
#include <algorithm>
#include <stdio.h>
#include <chrono>
#include <Windows.h>
#include <mmsystem.h>

Stereo::Replay::EngineDLL::ReplayEngine::ReplayEngine()
    :m_eye_separation      (0.0f),
     m_frame_eye_separation(0.0f),
     m_image_sink_ptr      (nullptr),
     m_is_loaded    (false)
{
    for (const auto& group : Capture::APIDumpLoader::get_api_groups() )
    {
        m_executors.add(group,
                        std::make_shared<NullCallExecutor>() );
    }

    {
        const auto win32_executor_ptr = std::make_shared<Win32CallExecutor>(&m_remapper,
                                                                            [this]()
                                                                            {
                                                                                notify_window_closed();
                                                                            },
                                                                            [this](const WindowPlacement& in_placement)
                                                                            {
                                                                                notify_window_placed(in_placement);
                                                                            });

        m_executors.add(Capture::APIDumpLoader::APIGroup::GDI32,
                        win32_executor_ptr);
        m_executors.add(Capture::APIDumpLoader::APIGroup::USER32,
                        win32_executor_ptr);
        m_executors.add(Capture::APIDumpLoader::APIGroup::WGL,
                        win32_executor_ptr);
    }

    m_executors.add(Capture::APIDumpLoader::APIGroup::D3D7,
                    std::make_shared<D3D7CallExecutor>(&m_remapper) );

    m_executors.get_executor_ptr(Capture::APIDumpLoader::APIGroup::D3D7)->set_per_eye_predicate([this](const uint64_t& in_dumped_address)
                                                                                                {
                                                                                                    return is_per_eye_texture(Capture::APIDumpLoader::APIGroup::D3D7,
                                                                                                                              in_dumped_address);
                                                                                                });
    m_executors.add(Capture::APIDumpLoader::APIGroup::D3D8,
                    std::make_shared<D3D8CallExecutor>(&m_remapper) );
    m_executors.add(Capture::APIDumpLoader::APIGroup::GL,
                    std::make_shared<GlCallExecutor>(&m_remapper) );
}

Stereo::Replay::EngineDLL::ReplayEngine::~ReplayEngine()
{
    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        m_control.is_stop_requested = true;
    }

    m_control.cv.notify_all();

    if (m_thread.joinable() )
    {
        m_thread.join();
    }
}

void Stereo::Replay::EngineDLL::ReplayEngine::add_call_interceptor(ICallInterceptor* in_interceptor_ptr)
{
    std::lock_guard<std::mutex> lock(m_listeners.mutex);

    m_listeners.interceptor_ptr_vec.push_back(in_interceptor_ptr);

    m_listeners.n_interceptors = m_listeners.interceptor_ptr_vec.size();
}

void Stereo::Replay::EngineDLL::ReplayEngine::add_replay_observer(IReplayObserver* in_observer_ptr)
{
    m_observers.add(in_observer_ptr);
}

bool Stereo::Replay::EngineDLL::ReplayEngine::begin_frame_execution(const bool&     in_uses_history,
                                                                    const uint32_t& in_n_execution)
{
    constexpr uint32_t N_LEFT_EXECUTION  = 0;
    constexpr uint32_t N_RIGHT_EXECUTION = 1;

    bool result = true;

    /* The eye of the execution, for the objects which each eye has a copy of. */
    {
        const Analysis::RecipeLoader::Eye eye = (m_frame_execution.mode == FrameExecution::TWICE) ? ((in_n_execution == N_LEFT_EXECUTION) ? Analysis::RecipeLoader::Eye::LEFT
                                                                                                                                         : Analysis::RecipeLoader::Eye::RIGHT)
                                                                                                  : m_frame_execution.eye;

        for (const auto executor_ptr : m_executors.get_executor_ptr_vec() )
        {
            executor_ptr->set_eye(eye);
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
        fail("The back buffer cannot be kept apart for each eye.");
    }

    return result;
}

bool Stereo::Replay::EngineDLL::ReplayEngine::capture_execution(const uint32_t& in_n_frame,
                                                                const uint32_t& in_n_execution,
                                                                const bool&     in_uses_history)
{
    FrameImage            image;
    bool                  is_ok       = true;
    std::vector<uint8_t>* pixel_ptr   = (in_uses_history) ? &m_eye_history.snapshot_array.at(in_n_execution).pixel_u8_vec
                                                          : nullptr;
    const bool            wants_image = (m_image_sink_ptr                                    != nullptr                       &&
                                         in_n_frame                                          >= m_frame_execution.first_frame &&
                                         in_n_frame                                          <= m_frame_execution.last_frame  &&
                                         m_image_sink_ptr->wants_frame_image(in_n_frame,
                                                                             in_n_execution) );

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
                fail("The back buffer cannot be kept apart for each eye.");

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

void Stereo::Replay::EngineDLL::ReplayEngine::complete_frame(bool* out_is_done_ptr)
{
    bool     is_interrupted = false;
    uint32_t n_frame        = 0;
    bool     should_stop    = false;

    for (const auto executor_ptr : m_executors.get_executor_ptr_vec() )
    {
        executor_ptr->on_frame_presented();

        /* What is not rendered for two eyes is rendered for the left one. */
        executor_ptr->set_eye(Analysis::RecipeLoader::Eye::LEFT);
    }

    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        n_frame        = m_control.n_frames_presented;
        is_interrupted = (m_control.is_reset_requested || m_control.is_stop_requested);
        should_stop    = (is_interrupted               || m_control.should_play       == false);

        m_control.n_frames_presented++;
    }

    notify_frame_presented(n_frame);

    if (should_stop)
    {
        /* Reset and stop are handled by the caller. */
        if (is_interrupted == false)
        {
            set_state(PlaybackState::PAUSED);
        }

        *out_is_done_ptr = true;
    }
}

bool Stereo::Replay::EngineDLL::ReplayEngine::execute_buffered_frame(const uint32_t& in_n_frame,
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
                        m_walker);

        has_boundary = Capture::APIDumpLoader::is_frame_boundary(*call_ptr);
    }

    if (has_boundary)
    {
        m_pacer.wait_for_frame(buffer.get_call_vec ().back().timestamp_qpc,
                               m_walker.get_segment().qpc_frequency);
    }

    /* Classify the frame by the shape the analysis found. */
    if (has_boundary                     &&
        m_recipe_applier_ptr != nullptr)
    {
        const auto& frame = m_session_ptr->get_completed_frame();

        if (m_recipe_applier_ptr->is_frame_known(frame.shape_key) )
        {
            is_rewritten = true;

            if (m_frame_execution.mode == FrameExecution::TWICE)
            {
                std::string error;

                result = get_eye_history_need(frame,
                                             &uses_history,
                                             &error);

                if (result == false)
                {
                    fail(error);
                }
            }
        }
        else
        {
            switch (m_recipe_applier_ptr->get_unknown_frame_policy() )
            {
                case Analysis::RecipeLoader::UnknownFramePolicy::FAIL:
                {
                    char message_i8vec128[128];

                    ::snprintf(message_i8vec128,
                               sizeof(message_i8vec128),
                               "Frame %u is of a shape that the recipe does not know (0x%016llx).",
                               in_n_frame,
                               static_cast<unsigned long long>(frame.shape_key) );

                    fail(message_i8vec128);

                    result = false;

                    break;
                }

                case Analysis::RecipeLoader::UnknownFramePolicy::SKIP:
                {
                    skips = true;

                    break;
                }

                case Analysis::RecipeLoader::UnknownFramePolicy::MONO:
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

    /* The device is as the calls of a frame played as they are leave it, not as the binder believes. */
    if (is_rewritten        == false &&
        m_matrix_binder_ptr != nullptr)
    {
        m_matrix_binder_ptr->forget();
    }

    if (result                                 &&
        buffer.get_call_vec().empty() == false)
    {
        result = FrameRunner::run(buffer,
                                  has_boundary,
                                  skips,
                                  m_frame_execution,
                                  m_executors.get_executor_ptr_vec(),
                                  [this, uses_history](const uint32_t& in_n_execution)
                                  {
                                      return begin_frame_execution(uses_history,
                                                                   in_n_execution);
                                  },
                                  [this](const Capture::APIDumpLoader::DumpedAPICall& in_call)
                                  {
                                      return m_executors.get_executor_ptr(Capture::APIDumpLoader::get_api_group(in_call.func) );
                                  },
                                  [this, is_rewritten](const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                       const IChunkSource&                          in_chunk_source,
                                                       const size_t&                                in_n_call,
                                                       const uint32_t&                              in_n_execution)
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
                                          is_ok = execute_call(in_call,
                                                               in_chunk_source);
                                      }

                                      return is_ok;
                                  },
                                  [this, in_n_frame, uses_history](const uint32_t& in_n_execution)
                                  {
                                      return capture_execution(in_n_frame,
                                                               in_n_execution,
                                                               uses_history);
                                  });
    }

    return result;
}

bool Stereo::Replay::EngineDLL::ReplayEngine::execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                           const IChunkSource&                          in_chunk_source)
{
    std::string                    error;
    std::vector<ICallInterceptor*> interceptor_ptr_vec;
    uint32_t                       n_frame        = 0;
    bool                           result         = false;
    bool                           should_execute = true;

    n_frame = m_control.n_frames_presented;

    /* Most replays have no interceptor, and then nothing is locked or copied for the call. */
    if (m_listeners.n_interceptors != 0)
    {
        std::lock_guard<std::mutex> lock(m_listeners.mutex);

        interceptor_ptr_vec = m_listeners.interceptor_ptr_vec;
    }

    /* Every interceptor sees the call, even if another one has skipped it. */
    for (auto interceptor_ptr : interceptor_ptr_vec)
    {
        if (interceptor_ptr->should_execute_call(in_call,
                                                 n_frame) == false)
        {
            should_execute = false;
        }
    }

    if (should_execute)
    {
        const auto group = Capture::APIDumpLoader::get_api_group(in_call.func);

        if (m_executors.has_executor(group) == false)
        {
            fail("The workload holds a call of an unknown API.");

            goto end;
        }

        if (m_executors.get_executor_ptr(group)->execute(in_call,
                                                         in_chunk_source,
                                                        &error) == false)
        {
            fail(error);

            goto end;
        }

        m_control.n_calls_executed++;
    }

    for (auto interceptor_ptr : interceptor_ptr_vec)
    {
        interceptor_ptr->on_call_replayed(in_call,
                                          n_frame,
                                          should_execute);
    }

    result = true;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::ReplayEngine::execute_draw_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                const IChunkSource&                          in_chunk_source,
                                                                const bool&                                  in_is_affected,
                                                                const bool&                                  in_is_presented,
                                                                const uint64_t&                              in_cover_key,
                                                                const Analysis::RecipeLoader::Eye&           in_eye,
                                                                const uint32_t&                              in_pass_structure_id)
{
    const uint64_t                        shape_key = m_session_ptr->get_completed_frame().shape_key;
    Stereo::Core::ScreenParallaxPlanner::Input input;
    bool                                  is_moved  = false;
    OverlayChunkSource                    overlay(in_chunk_source);
    Stereo::Core::ScreenShiftPlan         plan;
    Capture::APIDumpLoader::DumpedAPICall rewritten;

    /* The scene (the kind of frame) knows where its surfaces are, and how far apart the eyes may be for the nearest of them. */
    input.convergence_distance = m_recipe_applier_ptr->get_scene_convergence_distance(shape_key);
    input.cover_key            = in_cover_key;
    input.eye                  = in_eye;
    input.eye_separation       = m_frame_eye_separation;
    input.is_affected          = in_is_affected;
    input.is_overlay_pushed    = true;
    input.is_presented         = in_is_presented;
    input.picture_width        = m_session_ptr->get_model().pass_structure_interner.get_structure(in_pass_structure_id).viewport.width;
    input.separation_limit     = m_recipe_applier_ptr->get_scene_max_separation(shape_key);

    plan = Stereo::Core::ScreenParallaxPlanner::plan(*m_recipe_applier_ptr,
                                                     input);

    switch (plan.kind)
    {
        case Stereo::Core::ScreenShiftPlan::Kind::NONE:
        {
            break;
        }

        case Stereo::Core::ScreenShiftPlan::Kind::PER_VERTEX:
        {
            D3D7ScreenParallax::Parameters parameters;

            parameters.convergence_distance  = plan.convergence_distance;
            parameters.eye                   = plan.eye;
            parameters.eye_separation        = plan.eye_separation;
            parameters.field_of_view_degrees = plan.field_of_view_degrees;
            parameters.picture_width         = plan.picture_width;

            is_moved = D3D7ScreenParallax::rewrite(in_call,
                                                   in_chunk_source,
                                                   parameters,
                                                  &overlay,
                                                  &rewritten);

            break;
        }

        case Stereo::Core::ScreenShiftPlan::Kind::UNIFORM:
        {
            is_moved = D3D7ScreenParallax::rewrite_by(in_call,
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

    return (is_moved) ? execute_call(rewritten,
                                     overlay)
                      : execute_call(in_call,
                                     in_chunk_source);
}

bool Stereo::Replay::EngineDLL::ReplayEngine::execute_rewritten_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                     const IChunkSource&                          in_chunk_source,
                                                                     const size_t&                                in_n_call,
                                                                     const uint32_t&                              in_n_execution)
{
    const uint64_t                     call_index = m_session_ptr->get_completed_frame().first_call_index + in_n_call;
    Analysis::RecipeLoader::Eye        eye        = m_frame_execution.eye;
    const auto&                        frame      = m_session_ptr->get_completed_frame();
    uint32_t                           pass_index = 0;
    Analysis::RecipeApplier::CallPlace place;
    bool                               result     = false;

    /* Executed twice: the first execution is the left eye, the second the right one. */
    if (m_frame_execution.mode == FrameExecution::TWICE)
    {
        eye = (in_n_execution == 0) ? Analysis::RecipeLoader::Eye::LEFT
                                    : Analysis::RecipeLoader::Eye::RIGHT;
    }

    /* The draws are in the order of the calls. */
    const auto draw_iterator = std::lower_bound(frame.draw_instance_vec.begin(),
                                                frame.draw_instance_vec.end  (),
                                                call_index,
                                                [](const Analysis::APIDumpAnalysis::DrawInstance& in_draw,
                                                   const uint64_t&                                in_call_index)
                                                {
                                                    return (in_draw.call_index < in_call_index);
                                                });

    if (draw_iterator             != frame.draw_instance_vec.end() &&
        draw_iterator->call_index == call_index)
    {
        const uint64_t pass_key = frame.pass_key_vec.at(draw_iterator->pass_index);

        /* A draw which is skipped needs nothing on the device. */
        if (m_recipe_applier_ptr->is_draw_skipped(pass_key,
                                                  draw_iterator->structure_key) )
        {
            result = true;
        }
        else
        {
            const bool is_affected = m_recipe_applier_ptr->is_draw_affected(pass_key,
                                                                            draw_iterator->structure_key);

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
        }
    }
    else
    if (m_recipe_applier_ptr->has_rule_for(in_call.func) == false)
    {
        /* Most calls are not about a matrix: they are played as they are, without looking for their pass. */
        result = execute_call(in_call,
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
            result = execute_call(in_call,
                                  in_chunk_source);
        }
    }

    return result;
}

void Stereo::Replay::EngineDLL::ReplayEngine::fail(const std::string& in_message)
{
    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        m_control.error_message = in_message;
        m_control.should_play   = false;
    }

    set_state   (PlaybackState::FAILED);
    notify_error(in_message);
}

bool Stereo::Replay::EngineDLL::ReplayEngine::get_eye_history_need(const Analysis::APIDumpAnalysis::CompletedFrame& in_frame,
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
                *out_error_ptr = "Frame "                                                                    +
                                 std::to_string(in_frame.frame_index)                                        +
                                 " carries "                                                                 +
                                 resource.debug_name                                                         +
                                 " over from the previous frame, which cannot be rendered for two eyes: the recipe does not say that each eye has a copy of it.";

                result = false;

                break;
            }
        }
    }

    return result;
}

float Stereo::Replay::EngineDLL::ReplayEngine::get_eye_separation() const
{
    return m_eye_separation;
}

Stereo::Replay::EngineDLL::IPlaybackController* Stereo::Replay::EngineDLL::ReplayEngine::get_playback_controller_ptr()
{
    return this;
}

Stereo::Replay::EngineDLL::PlaybackStatus Stereo::Replay::EngineDLL::ReplayEngine::get_status() const
{
    PlaybackStatus result;

    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        result.error_message      = m_control.error_message;
        result.n_calls_executed   = m_control.n_calls_executed;
        result.n_frames_presented = m_control.n_frames_presented;
        result.state              = m_control.state;
    }

    return result;
}

bool Stereo::Replay::EngineDLL::ReplayEngine::load(const char*  in_filename_ptr,
                                                   std::string* out_error_ptr)
{
    bool result = false;

    if (m_is_loaded)
    {
        if (out_error_ptr != nullptr)
        {
            *out_error_ptr = "A workload has been loaded already.";
        }

        goto end;
    }

    if (m_walker.open(in_filename_ptr,
                      out_error_ptr) == false)
    {
        goto end;
    }

    m_is_loaded = true;
    m_thread    = std::thread(&ReplayEngine::thread_main,
                              this);
    result      = true;

end:
    return result;
}

bool Stereo::Replay::EngineDLL::ReplayEngine::is_per_eye_texture(const Capture::APIDumpLoader::APIGroup& in_group,
                                                                 const uint64_t&                         in_address) const
{
    Analysis::APIDumpAnalysis::ResourceIdentity identity;
    Analysis::APIDumpAnalysis::ResourceView     view;
    bool                                        result = false;

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

void Stereo::Replay::EngineDLL::ReplayEngine::notify_error(const std::string& in_message)
{
    m_observers.for_each([&](IReplayObserver* in_observer_ptr)
                         {
                             in_observer_ptr->on_error(in_message);
                         });
}

void Stereo::Replay::EngineDLL::ReplayEngine::notify_frame_presented(const uint32_t& in_n_frame)
{
    m_observers.for_each([&](IReplayObserver* in_observer_ptr)
                         {
                             in_observer_ptr->on_frame_presented(in_n_frame);
                         });
}

void Stereo::Replay::EngineDLL::ReplayEngine::notify_idle()
{
    for (const auto executor_ptr : m_executors.get_executor_ptr_vec() )
    {
        executor_ptr->on_idle();
    }
}

void Stereo::Replay::EngineDLL::ReplayEngine::notify_window_closed()
{
    m_observers.for_each([](IReplayObserver* in_observer_ptr)
                         {
                             in_observer_ptr->on_window_closed();
                         });
}

void Stereo::Replay::EngineDLL::ReplayEngine::notify_window_placed(const WindowPlacement& in_placement)
{
    m_observers.for_each([&](IReplayObserver* in_observer_ptr)
                         {
                             in_observer_ptr->on_window_placed(in_placement);
                         });
}

void Stereo::Replay::EngineDLL::ReplayEngine::pause()
{
    std::lock_guard<std::mutex> lock(m_control.mutex);

    m_control.should_play = false;
}

void Stereo::Replay::EngineDLL::ReplayEngine::play()
{
    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        m_control.should_play = true;
    }

    m_control.cv.notify_all();
}

void Stereo::Replay::EngineDLL::ReplayEngine::play_until_frame_boundary()
{
    bool is_at_frame_start = true;
    bool is_done           = false;

    set_state(PlaybackState::PLAYING);

    /* A pause, or a reset, must not make the replay hurry to catch up. */
    m_pacer.reset();

    while (is_done == false)
    {
        const Capture::APIDumpLoader::DumpedAPICall* call_ptr = nullptr;
        uint32_t                                     n_frame  = 0;

        {
            std::lock_guard<std::mutex> lock(m_control.mutex);

            n_frame = m_control.n_frames_presented;
        }

        const bool is_buffered = (is_at_frame_start                                       &&
                                  m_frame_execution.mode != FrameExecution::DIRECT        &&
                                  n_frame                >= m_frame_execution.first_frame &&
                                  n_frame                <= m_frame_execution.last_frame);

        /* A frame played as it is leaves the device as the application sets it, which the binder does not know. */
        if (is_at_frame_start              &&
            is_buffered         == false   &&
            m_matrix_binder_ptr != nullptr)
        {
            m_matrix_binder_ptr->forget();
        }

        if (is_buffered)
        {
            bool has_workload_ended = false;

            if (execute_buffered_frame(n_frame,
                                      &has_workload_ended) == false)
            {
                is_done = true;
            }
            else
            if (has_workload_ended)
            {
                {
                    std::lock_guard<std::mutex> lock(m_control.mutex);

                    m_control.should_play = false;
                }

                set_state(PlaybackState::FINISHED);

                is_done = true;
            }
            else
            {
                complete_frame(&is_done);
            }

            continue;
        }

        if (read_next_call(&call_ptr) == false)
        {
            {
                std::lock_guard<std::mutex> lock(m_control.mutex);

                m_control.should_play = false;
            }

            set_state(PlaybackState::FINISHED);

            is_done = true;

            continue;
        }

        const bool is_frame_boundary = Capture::APIDumpLoader::is_frame_boundary(*call_ptr);

        is_at_frame_start = false;

        if (is_frame_boundary)
        {
            m_pacer.wait_for_frame(call_ptr->timestamp_qpc,
                                   m_walker.get_segment().qpc_frequency);

            capture_execution(n_frame,
                              0,      /* in_n_execution */
                              false); /* in_uses_history */
        }

        if (execute_call(*call_ptr,
                         m_walker) == false)
        {
            is_done = true;

            continue;
        }

        if (is_frame_boundary)
        {
            is_at_frame_start = true;

            complete_frame(&is_done);
        }
    }
}

bool Stereo::Replay::EngineDLL::ReplayEngine::read_back_buffer(std::vector<uint8_t>* out_pixel_u8_vec_ptr,
                                                               FrameImage*           out_image_ptr)
{
    bool result = false;

    for (const auto executor_ptr : m_executors.get_executor_ptr_vec() )
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

bool Stereo::Replay::EngineDLL::ReplayEngine::read_next_call(const Capture::APIDumpLoader::DumpedAPICall** out_call_ptr_ptr)
{
    const bool result = m_walker.read_next_call(out_call_ptr_ptr);

    /* The analysis follows every call, whether or not the frame is rewritten, since what it tracks depends on all of them. */
    if (result                   &&
        m_session_ptr != nullptr)
    {
        m_session_ptr->process_call(**out_call_ptr_ptr,
                                    m_walker.get_segment() );
    }

    return result;
}

void Stereo::Replay::EngineDLL::ReplayEngine::remove_call_interceptor(ICallInterceptor* in_interceptor_ptr)
{
    std::lock_guard<std::mutex> lock(m_listeners.mutex);

    m_listeners.interceptor_ptr_vec.erase(std::remove(m_listeners.interceptor_ptr_vec.begin(),
                                                      m_listeners.interceptor_ptr_vec.end  (),
                                                      in_interceptor_ptr),
                                          m_listeners.interceptor_ptr_vec.end() );

    m_listeners.n_interceptors = m_listeners.interceptor_ptr_vec.size();
}

void Stereo::Replay::EngineDLL::ReplayEngine::remove_replay_observer(IReplayObserver* in_observer_ptr)
{
    m_observers.remove(in_observer_ptr);
}

void Stereo::Replay::EngineDLL::ReplayEngine::reset()
{
    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        m_control.is_reset_requested = true;
        m_control.should_play        = false;
    }

    m_control.cv.notify_all();
}

void Stereo::Replay::EngineDLL::ReplayEngine::reset_executors(const bool& in_is_final)
{
    /* The executors which came later depend on the ones which came earlier (a device needs its window). */
    const auto executor_ptr_vec = m_executors.get_executor_ptr_vec();

    for (auto iterator  = executor_ptr_vec.rbegin();
              iterator != executor_ptr_vec.rend  ();
            ++iterator)
    {
        if (in_is_final)
        {
            (*iterator)->shutdown();
        }
        else
        {
            (*iterator)->reset();
        }
    }

    m_remapper.clear();
}

bool Stereo::Replay::EngineDLL::ReplayEngine::rewind()
{
    std::string error;
    const bool  is_rewound = m_walker.rewind(&error);

    if (m_recipe_applier_ptr != nullptr)
    {
        m_matrix_binder_ptr->forget();

        m_session_ptr.reset(new Analysis::APIDumpAnalysis::Frontends::AnalysisSession(false) ); /* in_keeps_history */
    }

    m_eye_history = EyeHistory();

    reset_executors(false); /* in_is_final */

    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        m_control.error_message      = "";
        m_control.n_calls_executed   = 0;
        m_control.n_frames_presented = 0;
    }

    if (is_rewound == false)
    {
        fail(error);
    }

    return is_rewound;
}

void Stereo::Replay::EngineDLL::ReplayEngine::set_eye_separation(const float& in_eye_separation)
{
    m_eye_separation = in_eye_separation;
}

void Stereo::Replay::EngineDLL::ReplayEngine::set_frame_execution(const FrameExecutionOptions& in_options)
{
    m_frame_execution = in_options;
}

void Stereo::Replay::EngineDLL::ReplayEngine::set_frame_image_sink(IFrameImageSink* in_sink_ptr)
{
    m_image_sink_ptr = in_sink_ptr;
}

void Stereo::Replay::EngineDLL::ReplayEngine::set_recipe(const Analysis::RecipeLoader::Recipe& in_recipe)
{
    m_recipe_applier_ptr.reset(new Analysis::RecipeApplier::RecipeApplier(in_recipe) );
    m_rewriter_ptr.reset      (new FrameRewriter(*m_recipe_applier_ptr) );
    m_matrix_binder_ptr.reset (new EyeMatrixBinder(*m_rewriter_ptr,
                                                   [this](const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                          const IChunkSource&                          in_chunk_source)
                                                   {
                                                       return execute_call(in_call,
                                                                           in_chunk_source);
                                                   }) );
    m_session_ptr.reset       (new Analysis::APIDumpAnalysis::Frontends::AnalysisSession(false) ); /* in_keeps_history */
}

void Stereo::Replay::EngineDLL::ReplayEngine::set_speed(const double& in_speed)
{
    m_pacer.set_speed(in_speed);
}

void Stereo::Replay::EngineDLL::ReplayEngine::set_state(const PlaybackState& in_state)
{
    PlaybackState old_state;

    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        old_state       = m_control.state;
        m_control.state = in_state;
    }

    if (old_state != in_state)
    {
        m_observers.for_each([&](IReplayObserver* in_observer_ptr)
                             {
                                 in_observer_ptr->on_state_changed(old_state,
                                                                   in_state);
                             });
    }
}

void Stereo::Replay::EngineDLL::ReplayEngine::set_windows_visible(const bool& in_are_visible)
{
    for (const auto executor_ptr : m_executors.get_executor_ptr_vec() )
    {
        executor_ptr->set_windows_visible(in_are_visible);
    }
}

void Stereo::Replay::EngineDLL::ReplayEngine::thread_main()
{
    constexpr std::chrono::milliseconds IDLE_INTERVAL(10);

    /* Sleep() is exact to a millisecond only if asked for. */
    ::timeBeginPeriod(1);

    const auto can_proceed_func = [this]()
    {
        const bool is_playable = (m_control.state == PlaybackState::IDLE ||
                                  m_control.state == PlaybackState::PAUSED);

        return (m_control.is_reset_requested           ||
                m_control.is_stop_requested            ||
                (m_control.should_play && is_playable) );
    };
    bool is_stopping = false;

    while (is_stopping == false)
    {
        bool should_reset = false;

        {
            std::unique_lock<std::mutex> lock(m_control.mutex);

            /* The windows of the replay belong to this thread: while it waits, they are given their messages. */
            while (can_proceed_func() == false)
            {
                if (m_control.cv.wait_for(lock,
                                          IDLE_INTERVAL,
                                          can_proceed_func) == false)
                {
                    lock.unlock();

                    notify_idle();

                    lock.lock();
                }
            }

            is_stopping                  = m_control.is_stop_requested;
            should_reset                 = m_control.is_reset_requested;
            m_control.is_reset_requested = false;
        }

        if (is_stopping)
        {
            continue;
        }

        if (should_reset)
        {
            /* The first frame is replayed, so that it is on the screen, and the replay waits there for play(): a reset
             * does not ask to play, so the replay stops at the first frame boundary.
             */
            if (rewind() )
            {
                play_until_frame_boundary();
            }
        }
        else
        {
            play_until_frame_boundary();
        }
    }

    reset_executors(true); /* in_is_final */

    ::timeEndPeriod(1);
}

bool Stereo::Replay::EngineDLL::ReplayEngine::write_back_buffer(const std::vector<uint8_t>& in_pixel_u8_vec)
{
    bool result = false;

    for (const auto executor_ptr : m_executors.get_executor_ptr_vec() )
    {
        if (executor_ptr->write_back_buffer(in_pixel_u8_vec) )
        {
            result = true;

            break;
        }
    }

    return result;
}
