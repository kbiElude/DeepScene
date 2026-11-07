/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "analysis/api_dump_analysis/workload_identity.h"
#include "capture/api_dump_loader/workload_reader.h"

Analysis::APIDumpAnalysis::Frontends::AnalysisSession::AnalysisSession(const bool& in_keeps_history)
    :m_builder               (in_keeps_history),
     m_d3d7_frontend         (&m_builder),
     m_d3d8_frontend         (&m_builder),
     m_max_frames            (0),
     m_n_calls_by_group_array(),
     m_n_calls_since_frame   (0),
     m_n_reader_threads      (0),
     m_skip_data_chunks_above_n_bytes(0)
{
    /* Stub */
}

bool Analysis::APIDumpAnalysis::Frontends::AnalysisSession::find_resource_view(const Capture::APIDumpLoader::APIGroup&  in_group,
                                                                               const uint64_t&                          in_address,
                                                                               Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const
{
    bool result = false;

    switch (in_group)
    {
        case Capture::APIDumpLoader::APIGroup::D3D7:
        {
            result = m_d3d7_frontend.find_resource_view(in_address,
                                                        out_view_ptr);

            break;
        }

        case Capture::APIDumpLoader::APIGroup::D3D8:
        {
            result = m_d3d8_frontend.find_resource_view(in_address,
                                                        out_view_ptr);

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

const Analysis::APIDumpAnalysis::CompletedFrame& Analysis::APIDumpAnalysis::Frontends::AnalysisSession::get_completed_frame() const
{
    return m_builder.get_completed_frame();
}

Analysis::APIDumpAnalysis::Frontends::IAPIFrontend* Analysis::APIDumpAnalysis::Frontends::AnalysisSession::get_frontend_ptr(const Capture::APIDumpLoader::APIGroup& in_group)
{
    IAPIFrontend* result_ptr = nullptr;

    switch (in_group)
    {
        case Capture::APIDumpLoader::APIGroup::D3D7:
        {
            result_ptr = &m_d3d7_frontend;

            break;
        }

        case Capture::APIDumpLoader::APIGroup::D3D8:
        {
            result_ptr = &m_d3d8_frontend;

            break;
        }

        default:
        {
            break;
        }
    }

    return result_ptr;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::AnalysisSession::get_last_draw_cover_key() const
{
    return m_builder.get_last_draw_cover_key();
}

uint64_t Analysis::APIDumpAnalysis::Frontends::AnalysisSession::get_last_draw_signature_key() const
{
    return m_builder.get_last_draw_signature_key();
}

const Analysis::APIDumpAnalysis::Model& Analysis::APIDumpAnalysis::Frontends::AnalysisSession::get_model() const
{
    return m_builder.get_model();
}

Analysis::APIDumpAnalysis::Frontends::SessionProgress Analysis::APIDumpAnalysis::Frontends::AnalysisSession::get_progress() const
{
    const auto&     model  = m_builder.get_model();
    SessionProgress result;

    result.n_calls           = m_summary.n_calls;
    result.n_frame_shapes    = model.frame_shape_interner.get_n_structures   ();
    result.n_frames          = m_builder.get_n_frames                        ();
    result.n_pass_structures = model.pass_structure_interner.get_n_structures();
    result.n_segments        = m_summary.n_segments;

    if (m_summary.qpc_frequency != 0)
    {
        result.demo_seconds = static_cast<double>(m_summary.last_timestamp_qpc - m_summary.first_timestamp_qpc) /
                              static_cast<double>(m_summary.qpc_frequency);
    }

    return result;
}

Analysis::APIDumpAnalysis::WorkloadSummary Analysis::APIDumpAnalysis::Frontends::AnalysisSession::get_summary() const
{
    WorkloadSummary result = m_summary;

    for (size_t n_group = 0;
                n_group < N_API_GROUPS;
              ++n_group)
    {
        if (m_n_calls_by_group_array.at(n_group) != 0)
        {
            result.n_calls_by_group_map[static_cast<Capture::APIDumpLoader::APIGroup>(n_group)] = m_n_calls_by_group_array.at(n_group);
        }
    }

    for (const IAPIFrontend* frontend_ptr : std::array<const IAPIFrontend*, 2>{&m_d3d7_frontend, &m_d3d8_frontend})
    {
        for (const auto& hazard_pair : frontend_ptr->get_n_resolution_hazards() )
        {
            result.n_resolution_hazards_map[hazard_pair.first] += hazard_pair.second;
        }

        result.n_unfollowed_calls  += frontend_ptr->get_n_unfollowed_calls ();
        result.n_unsupported_calls += frontend_ptr->get_n_unsupported_calls();
    }

    result.n_trailing_calls = m_n_calls_since_frame;

    return result;
}

const Analysis::APIDumpAnalysis::Viewport& Analysis::APIDumpAnalysis::Frontends::AnalysisSession::get_viewport() const
{
    return m_builder.get_viewport();
}

Analysis::APIDumpAnalysis::WorkloadReference Analysis::APIDumpAnalysis::Frontends::AnalysisSession::get_workload_reference() const
{
    return m_workload_reference;
}

bool Analysis::APIDumpAnalysis::Frontends::AnalysisSession::has_reached_frame_limit() const
{
    const auto n_frames = m_builder.get_n_frames();

    return (m_max_frames != 0             &&
            n_frames     >= m_max_frames);
}

bool Analysis::APIDumpAnalysis::Frontends::AnalysisSession::process_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                                         const Capture::APIDumpLoader::Workload&      in_segment)
{
    const auto    group          = Capture::APIDumpLoader::get_api_group(in_call.func);
    IAPIFrontend* frontend_ptr   = get_frontend_ptr                     (group);
    bool          is_frame_ended = false;
    const auto    n_frames       = m_builder.get_n_frames();

    if (m_summary.n_calls == 0)
    {
        m_summary.first_timestamp_qpc = in_call.timestamp_qpc;
    }

    m_summary.last_timestamp_qpc = in_call.timestamp_qpc;

    m_n_calls_by_group_array.at(static_cast<size_t>(group) )++;

    m_builder.begin_call(m_summary.n_calls,
                         in_call.timestamp_qpc);

    if (frontend_ptr != nullptr)
    {
        frontend_ptr->on_call(in_call,
                              in_segment);
    }
    else
    {
        m_summary.n_calls_ignored++;
    }

    m_summary.n_calls++;
    m_n_calls_since_frame++;

    is_frame_ended = (m_builder.get_n_frames() != n_frames);

    /* The call that ends a frame belongs to that frame. */
    if (is_frame_ended)
    {
        m_n_calls_since_frame = 0;
    }

    return is_frame_ended;
}

void Analysis::APIDumpAnalysis::Frontends::AnalysisSession::process_segment(const Capture::APIDumpLoader::Workload& in_segment)
{
    m_summary.n_segments++;

    m_summary.pointer_n_bytes = in_segment.pointer_n_bytes;
    m_summary.qpc_frequency   = in_segment.qpc_frequency;

    for (const auto& call : *in_segment.dumped_api_call_vec_ptr)
    {
        process_call(call,
                     in_segment);

        if (has_reached_frame_limit() )
        {
            break;
        }
    }
}

bool Analysis::APIDumpAnalysis::Frontends::AnalysisSession::run(const char*         in_filename_ptr,
                                                                const ProgressFunc& in_progress_func,
                                                                std::string*        out_error_ptr)
{
    Capture::APIDumpLoader::WorkloadReader::Options reader_options;
    Capture::APIDumpLoader::Workload                segment;

    reader_options.n_threads                      = m_n_reader_threads;
    reader_options.skip_data_chunks_above_n_bytes = m_skip_data_chunks_above_n_bytes;

    auto reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(in_filename_ptr,
                                                                     reader_options,
                                                                     out_error_ptr);
    bool result     = (reader_ptr != nullptr);

    if (result)
    {
        std::string identity_error;

        m_workload_reference.filename = in_filename_ptr;

        /* A workload which is read by the loader but not by this is still analyzed; it just cannot be told from another by its reference. */
        WorkloadIdentity::read(in_filename_ptr,
                              &m_workload_reference,
                              &identity_error);
    }

    while (result                                           &&
           has_reached_frame_limit      ()         == false &&
           reader_ptr->read_next_segment(&segment) )
    {
        process_segment(segment);

        if (in_progress_func)
        {
            in_progress_func(get_progress() );
        }
    }

    if (result)
    {
        m_summary.is_truncated = reader_ptr->is_truncated();
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::AnalysisSession::set_access_sink(Analysis::APIDumpAnalysis::IAccessSink* in_sink_ptr)
{
    m_builder.set_access_sink(in_sink_ptr);
}

void Analysis::APIDumpAnalysis::Frontends::AnalysisSession::set_max_frames(const uint64_t& in_max_frames)
{
    m_max_frames = in_max_frames;
}

void Analysis::APIDumpAnalysis::Frontends::AnalysisSession::set_n_reader_threads(const uint32_t& in_n_threads)
{
    m_n_reader_threads = in_n_threads;
}

void Analysis::APIDumpAnalysis::Frontends::AnalysisSession::set_skip_data_chunks_above_n_bytes(const uint32_t& in_n_bytes)
{
    m_skip_data_chunks_above_n_bytes = in_n_bytes;
}

Analysis::APIDumpAnalysis::Model Analysis::APIDumpAnalysis::Frontends::AnalysisSession::take_model()
{
    return m_builder.take_model();
}
