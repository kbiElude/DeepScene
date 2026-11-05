/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "stereo_d3d7_call_executor.h"
#include "stereo_replay_engine.h"

Stereo::Replay::EngineDLL::StereoReplayEngine::StereoReplayEngine()
    :m_paced_executor(&m_stereo_executor)
{
    const auto d3d7_executor_ptr = std::make_shared<StereoD3D7CallExecutor>(m_engine.get_remapper_ptr() );

    /* Asked when an object is made, whether each eye has a copy of it. */
    d3d7_executor_ptr->set_per_eye_predicate([this](const uint64_t& in_dumped_address)
                                             {
                                                 return m_stereo_executor.is_per_eye_texture(Capture::APIDumpLoader::APIGroup::D3D7,
                                                                                             in_dumped_address);
                                             });

    m_engine.add_call_executor(Capture::APIDumpLoader::APIGroup::D3D7,
                               d3d7_executor_ptr);
    m_engine.set_frame_executor(&m_paced_executor);
}

Stereo::Replay::EngineDLL::StereoReplayEngine::~StereoReplayEngine()
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::add_call_interceptor(ICallInterceptor* in_interceptor_ptr)
{
    m_engine.add_call_interceptor(in_interceptor_ptr);
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::add_replay_observer(IReplayObserver* in_observer_ptr)
{
    m_engine.add_replay_observer(in_observer_ptr);
}

float Stereo::Replay::EngineDLL::StereoReplayEngine::get_eye_separation() const
{
    return m_stereo_executor.get_eye_separation();
}

Stereo::Replay::EngineDLL::IPlaybackController* Stereo::Replay::EngineDLL::StereoReplayEngine::get_playback_controller_ptr()
{
    return this;
}

Stereo::Replay::EngineDLL::PlaybackStatus Stereo::Replay::EngineDLL::StereoReplayEngine::get_status() const
{
    return m_engine.get_status();
}

bool Stereo::Replay::EngineDLL::StereoReplayEngine::load(const char*  in_filename_ptr,
                                                         std::string* out_error_ptr)
{
    return m_engine.load(in_filename_ptr,
                         out_error_ptr);
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::pause()
{
    m_paced_executor.clear_unpaced_frame();

    m_engine.pause();
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::play()
{
    m_paced_executor.clear_unpaced_frame();

    m_engine.play();
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::play_to_frame(const uint32_t& in_n_frame)
{
    /* The frames up to the one asked for are hurried through; it is shown, and the replay pauses there. */
    m_paced_executor.set_unpaced_frame(in_n_frame);

    m_engine.play_to_frame(in_n_frame);
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::remove_call_interceptor(ICallInterceptor* in_interceptor_ptr)
{
    m_engine.remove_call_interceptor(in_interceptor_ptr);
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::remove_replay_observer(IReplayObserver* in_observer_ptr)
{
    m_engine.remove_replay_observer(in_observer_ptr);
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::reset()
{
    m_paced_executor.clear_unpaced_frame();

    m_engine.reset();
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::set_eye_separation(const float& in_eye_separation)
{
    m_stereo_executor.set_eye_separation(in_eye_separation);
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::set_frame_execution(const FrameExecutionOptions& in_options)
{
    m_stereo_executor.set_frame_execution(in_options);
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::set_frame_image_sink(IFrameImageSink* in_sink_ptr)
{
    m_stereo_executor.set_image_sink(in_sink_ptr);
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::set_recipe(const Analysis::RecipeLoader::Recipe& in_recipe)
{
    m_stereo_executor.set_recipe(in_recipe);
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::set_speed(const double& in_speed)
{
    m_paced_executor.set_speed(in_speed);
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::set_windows_visible(const bool& in_are_visible)
{
    m_engine.set_windows_visible(in_are_visible);
}
