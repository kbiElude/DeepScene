/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d8_call_executor.h"
#include "gl_call_executor.h"
#include "null_call_executor.h"
#include "replay_engine.h"
#include "win32_call_executor.h"
#include <algorithm>
#include <chrono>
#include <Windows.h>
#include <mmsystem.h>

Replay::StereoReplayEngineDLL::ReplayEngine::ReplayEngine()
    :m_is_loaded(false)
{
    for (const auto& group : Capture::APIDumpLoader::get_api_groups() )
    {
        m_executor_ptr_map[group] = std::make_shared<NullCallExecutor>();
    }

    {
        const auto win32_executor_ptr = std::make_shared<Win32CallExecutor>(&m_remapper,
                                                                             [this](const WindowPlacement& in_placement)
                                                                             {
                                                                                 notify_window_placed(in_placement);
                                                                             });

        m_executor_ptr_map[Capture::APIDumpLoader::APIGroup::GDI32]  = win32_executor_ptr;
        m_executor_ptr_map[Capture::APIDumpLoader::APIGroup::USER32] = win32_executor_ptr;
        m_executor_ptr_map[Capture::APIDumpLoader::APIGroup::WGL]    = win32_executor_ptr;
    }

    m_executor_ptr_map[Capture::APIDumpLoader::APIGroup::D3D8] = std::make_shared<D3D8CallExecutor>(&m_remapper);
    m_executor_ptr_map[Capture::APIDumpLoader::APIGroup::GL]   = std::make_shared<GlCallExecutor>(&m_remapper);
}

Replay::StereoReplayEngineDLL::ReplayEngine::~ReplayEngine()
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

void Replay::StereoReplayEngineDLL::ReplayEngine::add_call_interceptor(ICallInterceptor* in_interceptor_ptr)
{
    std::lock_guard<std::mutex> lock(m_listeners.mutex);

    m_listeners.interceptor_ptr_vec.push_back(in_interceptor_ptr);
}

void Replay::StereoReplayEngineDLL::ReplayEngine::add_replay_observer(IReplayObserver* in_observer_ptr)
{
    std::lock_guard<std::mutex> lock(m_listeners.mutex);

    m_listeners.observer_ptr_vec.push_back(in_observer_ptr);
}

bool Replay::StereoReplayEngineDLL::ReplayEngine::execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    std::string                    error;
    std::vector<ICallInterceptor*> interceptor_ptr_vec;
    uint32_t                       n_frame        = 0;
    bool                           result         = false;
    bool                           should_execute = true;

    {
        std::lock_guard<std::mutex> lock(m_listeners.mutex);

        interceptor_ptr_vec = m_listeners.interceptor_ptr_vec;
    }

    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        n_frame = m_control.n_frames_presented;
    }

    /* Every interceptor gets to see the call, even if another one has skipped it already. */
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
        const auto executor_iterator = m_executor_ptr_map.find(Capture::APIDumpLoader::get_api_group(in_call.func) );

        if (executor_iterator == m_executor_ptr_map.end() )
        {
            fail("The workload holds a call of an unknown API.");

            goto end;
        }

        if (executor_iterator->second->execute(in_call,
                                               m_walker,
                                              &error) == false)
        {
            fail(error);

            goto end;
        }

        {
            std::lock_guard<std::mutex> lock(m_control.mutex);

            m_control.n_calls_executed++;
        }
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

void Replay::StereoReplayEngineDLL::ReplayEngine::fail(const std::string& in_message)
{
    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        m_control.error_message = in_message;
        m_control.should_play   = false;
    }

    set_state   (PlaybackState::FAILED);
    notify_error(in_message);
}

std::vector<Replay::StereoReplayEngineDLL::ICallExecutor*> Replay::StereoReplayEngineDLL::ReplayEngine::get_executor_ptr_vec() const
{
    std::vector<ICallExecutor*> result_vec;

    /* An executor can serve more than one API. */
    for (const auto& executor_item : m_executor_ptr_map)
    {
        ICallExecutor* const executor_ptr = executor_item.second.get();

        if (std::find(result_vec.begin(),
                      result_vec.end  (),
                      executor_ptr) == result_vec.end() )
        {
            result_vec.push_back(executor_ptr);
        }
    }

    return result_vec;
}

std::vector<Replay::StereoReplayEngineDLL::IReplayObserver*> Replay::StereoReplayEngineDLL::ReplayEngine::get_observer_ptr_vec() const
{
    std::lock_guard<std::mutex> lock(m_listeners.mutex);

    return m_listeners.observer_ptr_vec;
}

Replay::StereoReplayEngineDLL::IPlaybackController* Replay::StereoReplayEngineDLL::ReplayEngine::get_playback_controller_ptr()
{
    return this;
}

Replay::StereoReplayEngineDLL::PlaybackStatus Replay::StereoReplayEngineDLL::ReplayEngine::get_status() const
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

bool Replay::StereoReplayEngineDLL::ReplayEngine::load(const char*  in_filename_ptr,
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

void Replay::StereoReplayEngineDLL::ReplayEngine::notify_error(const std::string& in_message)
{
    const auto observer_ptr_vec = get_observer_ptr_vec();

    for (auto observer_ptr : observer_ptr_vec)
    {
        observer_ptr->on_error(in_message);
    }
}

void Replay::StereoReplayEngineDLL::ReplayEngine::notify_frame_presented(const uint32_t& in_n_frame)
{
    const auto observer_ptr_vec = get_observer_ptr_vec();

    for (auto observer_ptr : observer_ptr_vec)
    {
        observer_ptr->on_frame_presented(in_n_frame);
    }
}

void Replay::StereoReplayEngineDLL::ReplayEngine::notify_idle()
{
    for (const auto executor_ptr : get_executor_ptr_vec() )
    {
        executor_ptr->on_idle();
    }
}

void Replay::StereoReplayEngineDLL::ReplayEngine::notify_window_placed(const WindowPlacement& in_placement)
{
    const auto observer_ptr_vec = get_observer_ptr_vec();

    for (auto observer_ptr : observer_ptr_vec)
    {
        observer_ptr->on_window_placed(in_placement);
    }
}

void Replay::StereoReplayEngineDLL::ReplayEngine::pause()
{
    std::lock_guard<std::mutex> lock(m_control.mutex);

    m_control.should_play = false;
}

void Replay::StereoReplayEngineDLL::ReplayEngine::play()
{
    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        m_control.should_play = true;
    }

    m_control.cv.notify_all();
}

void Replay::StereoReplayEngineDLL::ReplayEngine::play_until_frame_boundary()
{
    bool is_done = false;

    set_state(PlaybackState::PLAYING);

    /* A pause, or a reset, must not make the replay hurry to catch up. */
    m_pacer.reset();

    while (is_done == false)
    {
        const Capture::APIDumpLoader::DumpedAPICall* call_ptr = nullptr;

        if (m_walker.read_next_call(&call_ptr) == false)
        {
            {
                std::lock_guard<std::mutex> lock(m_control.mutex);

                m_control.should_play = false;
            }

            set_state(PlaybackState::FINISHED);

            is_done = true;

            continue;
        }

        const bool is_frame_boundary = Capture::APIDumpLoader::is_frame_boundary(call_ptr->func);

        if (is_frame_boundary)
        {
            m_pacer.wait_for_frame(call_ptr->timestamp_qpc,
                                   m_walker.get_segment().qpc_frequency);
        }

        if (execute_call(*call_ptr) == false)
        {
            is_done = true;

            continue;
        }

        if (is_frame_boundary)
        {
            for (const auto executor_ptr : get_executor_ptr_vec() )
            {
                executor_ptr->on_frame_presented();
            }

            bool     is_interrupted = false;
            uint32_t n_frame        = 0;
            bool     should_stop    = false;

            {
                std::lock_guard<std::mutex> lock(m_control.mutex);

                n_frame        = m_control.n_frames_presented;
                is_interrupted = (m_control.is_reset_requested ||
                                  m_control.is_stop_requested);
                should_stop    = (is_interrupted ||
                                  m_control.should_play == false);

                m_control.n_frames_presented++;
            }

            notify_frame_presented(n_frame);

            if (should_stop)
            {
                /* A reset or a stop is handled by the caller; only a pause leaves a state to set. */
                if (is_interrupted == false)
                {
                    set_state(PlaybackState::PAUSED);
                }

                is_done = true;
            }
        }
    }
}

void Replay::StereoReplayEngineDLL::ReplayEngine::remove_call_interceptor(ICallInterceptor* in_interceptor_ptr)
{
    std::lock_guard<std::mutex> lock(m_listeners.mutex);

    m_listeners.interceptor_ptr_vec.erase(std::remove(m_listeners.interceptor_ptr_vec.begin(),
                                                      m_listeners.interceptor_ptr_vec.end  (),
                                                      in_interceptor_ptr),
                                          m_listeners.interceptor_ptr_vec.end() );
}

void Replay::StereoReplayEngineDLL::ReplayEngine::remove_replay_observer(IReplayObserver* in_observer_ptr)
{
    std::lock_guard<std::mutex> lock(m_listeners.mutex);

    m_listeners.observer_ptr_vec.erase(std::remove(m_listeners.observer_ptr_vec.begin(),
                                                   m_listeners.observer_ptr_vec.end  (),
                                                   in_observer_ptr),
                                       m_listeners.observer_ptr_vec.end() );
}

void Replay::StereoReplayEngineDLL::ReplayEngine::reset()
{
    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        m_control.is_reset_requested = true;
        m_control.should_play        = false;
    }

    m_control.cv.notify_all();
}

void Replay::StereoReplayEngineDLL::ReplayEngine::reset_executors(const bool& in_is_final)
{
    /* The executors which came later depend on the ones which came earlier (a device needs its window). */
    const auto executor_ptr_vec = get_executor_ptr_vec();

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

bool Replay::StereoReplayEngineDLL::ReplayEngine::rewind()
{
    std::string error;
    const bool  is_rewound = m_walker.rewind(&error);

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

void Replay::StereoReplayEngineDLL::ReplayEngine::set_speed(const double& in_speed)
{
    m_pacer.set_speed(in_speed);
}

void Replay::StereoReplayEngineDLL::ReplayEngine::set_state(const PlaybackState& in_state)
{
    PlaybackState old_state;

    {
        std::lock_guard<std::mutex> lock(m_control.mutex);

        old_state       = m_control.state;
        m_control.state = in_state;
    }

    if (old_state != in_state)
    {
        const auto observer_ptr_vec = get_observer_ptr_vec();

        for (auto observer_ptr : observer_ptr_vec)
        {
            observer_ptr->on_state_changed(old_state,
                                           in_state);
        }
    }
}

void Replay::StereoReplayEngineDLL::ReplayEngine::thread_main()
{
    constexpr std::chrono::milliseconds IDLE_INTERVAL(10);

    /* Sleep() is exact to a millisecond only if asked for. */
    ::timeBeginPeriod(1);

    const auto can_proceed_func = [this]()
    {
        const bool is_playable = (m_control.state == PlaybackState::IDLE ||
                                  m_control.state == PlaybackState::PAUSED);

        return (m_control.is_reset_requested ||
                m_control.is_stop_requested  ||
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
