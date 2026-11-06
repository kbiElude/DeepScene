/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/workload_identity.h"
#include "analysis/api_dump_dependencies/deps_file.h"
#include "analysis/api_dump_dependencies/frame_slice_planner.h"
#include "stereo_d3d7_call_executor.h"
#include "stereo_replay_engine.h"

Stereo::Replay::EngineDLL::StereoReplayEngine::StereoReplayEngine()
    :m_has_dependencies(false),
     m_has_seek_target(false),
     m_present_skipper(&m_hurry),
     m_paced_executor (&m_stereo_executor,
                       &m_hurry)
{
    m_stereo_executor.set_hurry        (&m_hurry);
    m_stereo_executor.set_slice_skipper(&m_slice_skipper);

    m_engine.add_call_interceptor(&m_present_skipper);
    m_engine.add_call_interceptor(&m_slice_skipper);

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
    /* What was known of another workload is not of this one. */
    m_has_dependencies = false;

    m_has_seek_target = false;

    m_engine.set_chunk_wanted_func(Capture::APIDumpLoader::ChunkWantedFunc() );
    m_slice_skipper.clear_slice();

    const bool result = m_engine.load(in_filename_ptr,
                                      out_error_ptr);

    if (result)
    {
        m_workload_filename = in_filename_ptr;
    }

    return result;
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::pause()
{
    m_hurry.clear();

    m_engine.pause();
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::play()
{
    m_hurry.clear();

    m_engine.play();
}

void Stereo::Replay::EngineDLL::StereoReplayEngine::play_to_frame(const uint32_t& in_n_frame)
{
    /* What the frames before it make is what it and the frames after it read: the rest may be left out, if the dependencies are known. */
    if (m_has_dependencies)
    {
        /* The reader decodes segments ahead of the replay with the chunks of the dead frames left out, for the slice that was set when it was started. A
         * new slice while the replay has not reached the last target needs the chunks that the old one left out, so the replay starts over.
         */
        const bool is_last_seek_unfinished = (m_has_seek_target                                        &&
                                              m_engine.get_status().n_frames_presented <= m_seek_target_frame);

        m_slice_skipper.set_slice(Analysis::APIDumpDependencies::FrameSlicePlanner::plan(m_dependencies,
                                                                                         in_n_frame) );

        /* Before the reset, since the replay thread starts the reader again as soon as it is asked to. */
        m_engine.set_chunk_wanted_func(m_slice_skipper.get_chunk_wanted_func() );

        if (is_last_seek_unfinished)
        {
            m_engine.reset();
        }

        m_has_seek_target   = true;
        m_seek_target_frame = in_n_frame;
    }

    /* The frames up to the one asked for are hurried through; it is shown, and the replay pauses there. */
    m_hurry.set_until_frame(in_n_frame);

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
    m_has_seek_target = false;

    m_engine.set_chunk_wanted_func(Capture::APIDumpLoader::ChunkWantedFunc() );
    m_slice_skipper.clear_slice();
    m_hurry.clear();

    m_engine.reset();
}

bool Stereo::Replay::EngineDLL::StereoReplayEngine::set_dependencies_file(const char*  in_filename_ptr,
                                                                          std::string* out_error_ptr)
{
    Analysis::APIDumpDependencies::DependencyAnalysis dependencies;
    std::string                                       reason;
    bool                                              result       = false;

    if (Analysis::APIDumpDependencies::DepsFile::read(in_filename_ptr,
                                                     &dependencies,
                                                      out_error_ptr) == false)
    {
        goto end;
    }

    if (m_workload_filename.empty() == false                                                                                                                      &&
        Analysis::APIDumpAnalysis::WorkloadIdentity::verify(m_workload_filename,
                                                            dependencies.workload_reference,
                                                           &reason) == Analysis::APIDumpAnalysis::WorkloadMatch::MISMATCH)
    {
        *out_error_ptr = std::string(in_filename_ptr) + " is not of the workload which was loaded: " + reason;

        goto end;
    }

    m_dependencies     = std::move(dependencies);
    m_has_dependencies = true;
    result             = true;

end:
    return result;
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

void Stereo::Replay::EngineDLL::StereoReplayEngine::set_recipe(const Analysis::Recipe::Recipe& in_recipe)
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
