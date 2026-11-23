/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "common/vtable.h"
#include "interception/hooks/D3D8/types.h"
#include "d3d8_host_memory_exporter.h"
#include "pixel_row_writer.h"
#include <string.h>

Stereo::ConverterDLL::D3D8HostMemoryExporter::D3D8HostMemoryExporter(Stereo::Transport::FrameChannel*     inout_channel_ptr,
                                                                     const Analysis::Recipe::FramePacing& in_frame_pacing)
    :HostMemoryExporterBase(inout_channel_ptr,
                            in_frame_pacing),
     m_eye_destination_ptr(nullptr),
     m_eye_is_ok   (false),
     m_eye_pitch   (0),
     m_issue_cost  (4.0),
     m_job_set     (0),
     m_job_wait    (2.0),
     m_lock_cost   (2.0),
     m_pace_wait   (0.0),
     m_next_set    (0),
     m_write_cost  (8.0)
{
    const auto write_job_func = [this]()
    {
        write_job();
    };

    const auto write_right_eye_func = [this]()
    {
        write_right_eye();
    };

    m_eye_thread_ptr.reset(new Utils::Parallel::JobThread(write_right_eye_func) );
    m_job_thread_ptr.reset(new Utils::Parallel::JobThread(write_job_func) );
}

Stereo::ConverterDLL::D3D8HostMemoryExporter::~D3D8HostMemoryExporter()
{
    /* The job thread goes first: it hands the right eye to the other thread. */
    m_job_thread_ptr->stop();
    m_eye_thread_ptr->stop();

    for (auto& set : m_staging_array)
    {
        for (auto& staging : set)
        {
            unlock_staging (&staging);
            release_staging(&staging);
        }
    }
}

bool Stereo::ConverterDLL::D3D8HostMemoryExporter::ensure_staging(void*                                             in_device_ptr,
                                                                  const Interception::Hooks::D3D8::D3DSURFACE_DESC& in_description,
                                                                  Staging*                                          inout_staging_ptr)
{
    bool         result        = false;
    void** const device_vtable = APIInterceptor::get_vtable_ptr(in_device_ptr);

    if (inout_staging_ptr->surface_ptr        != nullptr               &&
        inout_staging_ptr->description.Width  == in_description.Width  &&
        inout_staging_ptr->description.Height == in_description.Height &&
        inout_staging_ptr->description.Format == in_description.Format)
    {
        result = true;

        goto end;
    }

    unlock_staging (inout_staging_ptr);
    release_staging(inout_staging_ptr);

    /* The surface may not be lockable, but a system-memory copy of it always is. */
    if (SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_CREATEIMAGESURFACE>(device_vtable,
                                                                                                                     Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_CREATEIMAGESURFACE)(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(in_device_ptr),
                                                                                                                                                                                                  in_description.Width,
                                                                                                                                                                                                  in_description.Height,
                                                                                                                                                                                                  in_description.Format,
                                                                                                                                                                                                 &inout_staging_ptr->surface_ptr) ))
    {
        inout_staging_ptr->description = in_description;
        result                         = true;
    }

end:
    return result;
}

Stereo::Transport::ExportStatus Stereo::ConverterDLL::D3D8HostMemoryExporter::export_frame(const uint64_t&                        in_frame_index,
                                                                                           const Stereo::Transport::ExportSource& in_source)
{
    Interception::Hooks::D3D8::IDirect3DSurface8*                  back_buffer_ptr   = nullptr;
    std::array<Interception::Hooks::D3D8::D3DSURFACE_DESC, N_EYES> description_array;
    Job                                                            job;
    uint32_t                                                       n_eyes            = (in_source.right_surface_ptr != nullptr) ? 2 : 1;
    uint32_t                                                       n_locked          = 0;
    Stereo::Transport::ExportStatus                                result            = Stereo::Transport::ExportStatus::FAILED;
    const uint32_t                                                 set               = m_next_set;
    std::array<void*, N_EYES>                                      surface_ptr_array = {in_source.left_surface_ptr, in_source.right_surface_ptr};
    void** const                                                   device_vtable     = APIInterceptor::get_vtable_ptr(in_source.api_device_ptr);

    const auto copy_rects_func      = APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_COPYRECTS>    (device_vtable,
                                                                                                                                    Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_COPYRECTS);
    const auto get_back_buffer_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DDEVICE8_GETBACKBUFFER>(device_vtable,
                                                                                                                                    Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DDEVICE8_GETBACKBUFFER);

    /* The other side has not said that it accepts what the exporter passes on. This is not a failure: the frame is left out. */
    if (is_host_memory_negotiated() == false)
    {
        result = Stereo::Transport::ExportStatus::NOT_READY;

        goto end;
    }

    if (surface_ptr_array.at(0) == nullptr)
    {
        if (FAILED(get_back_buffer_func(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*>(in_source.api_device_ptr),
                                        0, /* BackBuffer */
                                        Interception::Hooks::D3D8::D3DBACKBUFFER_TYPE_MONO,
                                       &back_buffer_ptr) ))
        {
            goto end;
        }

        surface_ptr_array.at(0) = back_buffer_ptr;
    }

    /* Reads the size and format of each eye and makes sure each eye has a surface in system memory. The surface this set held for the previous
     * frame is reused if the size and format are the same. This set is not the one the thread is copying from: the thread unlocks its set
     * when it finishes, and this set has been unlocked since the previous frame.
     */
    for (uint32_t n_eye = 0;
                  n_eye < n_eyes;
                ++n_eye)
    {
        void** const surface_vtable = APIInterceptor::get_vtable_ptr(surface_ptr_array.at(n_eye) );

        if (FAILED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_GETDESC>(surface_vtable,
                                                                                                            Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_GETDESC)(static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(surface_ptr_array.at(n_eye) ),
                                                                                                                                                                              &description_array.at(n_eye) ) ))
        {
            goto end;
        }

        if (ensure_staging(in_source.api_device_ptr,
                           description_array.at(n_eye),
                          &m_staging_array.at(set).at(n_eye) ) == false)
        {
            goto end;
        }
    }

    if (n_eyes == 2)
    {
        const auto& left_description  = description_array.at(0);
        const auto& right_description = description_array.at(1);

        if (left_description.Width  != right_description.Width ||
            left_description.Height != right_description.Height)
        {
            goto end;
        }
    }

    /* Both copies are requested before either is waited for, so that the GPU makes them one after the other with no CPU work in between. */
    {
        const double issue_start_ms = Utils::Statistics::SeriesStats::get_now();

        for (uint32_t n_eye = 0;
                      n_eye < n_eyes;
                    ++n_eye)
        {
            if (FAILED(copy_rects_func(static_cast<Interception::Hooks::D3D8::IDirect3DDevice8*> (in_source.api_device_ptr),
                                       static_cast<Interception::Hooks::D3D8::IDirect3DSurface8*>(surface_ptr_array.at(n_eye) ),
                                       nullptr,
                                       0,
                                       m_staging_array.at(set).at(n_eye).surface_ptr,
                                       nullptr) ))
            {
                goto end;
            }
        }

        m_issue_cost.add(Utils::Statistics::SeriesStats::get_now() - issue_start_ms);
    }

    {
        const double lock_start_ms = Utils::Statistics::SeriesStats::get_now();

        for (uint32_t n_eye = 0;
                      n_eye < n_eyes;
                    ++n_eye)
        {
            if (lock_staging(&m_staging_array.at(set).at(n_eye),
                            &job.source_ptr_array.at(n_eye),
                            &job.source_pitch_array.at(n_eye) ) == false)
            {
                goto end;
            }

            ++n_locked;

            job.description_array.at(n_eye) = description_array.at(n_eye);
        }

        m_lock_cost.add(Utils::Statistics::SeriesStats::get_now() - lock_start_ms);
    }

    job.info.frame_index       = in_frame_index;
    job.info.height            = description_array.at(0).Height;
    job.info.n_eyes            = n_eyes;
    job.info.timestamp_seconds = in_source.timestamp_seconds;
    job.info.width             = description_array.at(0).Width * n_eyes;
    job.picture                = in_source.picture;

    /* The thread must have finished the previous frame, whose surfaces are in the other set, before it can be given this one. */
    wait_for_job();

    {
        const auto prepare_func = [&]()
        {
            m_job     = job;
            m_job_set = set;
        };

        m_job_thread_ptr->queue(prepare_func);
    }

    m_next_set = (set + 1) % N_SETS;
    result     = Stereo::Transport::ExportStatus::EXPORTED;

end:
    if (result != Stereo::Transport::ExportStatus::EXPORTED)
    {
        for (uint32_t n_eye = 0;
                      n_eye < n_locked;
                    ++n_eye)
        {
            unlock_staging(&m_staging_array.at(set).at(n_eye) );
        }
    }

    if (back_buffer_ptr != nullptr)
    {
        void** const vtable = APIInterceptor::get_vtable_ptr(back_buffer_ptr);

        APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_RELEASE>(vtable,
                                                                                                 Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_RELEASE)(back_buffer_ptr);
    }

    return result;
}

bool Stereo::ConverterDLL::D3D8HostMemoryExporter::lock_staging(Staging*        inout_staging_ptr,
                                                                const uint8_t** out_bits_ptr_ptr,
                                                                uint32_t*       out_pitch_ptr)
{
    Interception::Hooks::D3D8::D3DLOCKED_RECT locked_rect = {};
    bool                                      result      = false;
    void** const                              vtable      = APIInterceptor::get_vtable_ptr(inout_staging_ptr->surface_ptr);

    if (SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_LOCKRECT>(vtable,
                                                                                                            Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_LOCKRECT)(inout_staging_ptr->surface_ptr,
                                                                                                                                                                               &locked_rect,
                                                                                                                                                                                nullptr,
                                                                                                                                                                                Interception::Hooks::D3D8::D3DLOCK_READONLY) ))
    {
        *out_bits_ptr_ptr           = static_cast<const uint8_t*>(locked_rect.pBits);
        *out_pitch_ptr              = static_cast<uint32_t>(locked_rect.Pitch);
        inout_staging_ptr->is_locked = true;
        result                      = true;
    }

    return result;
}

void Stereo::ConverterDLL::D3D8HostMemoryExporter::release_staging(Staging* inout_staging_ptr)
{
    if (inout_staging_ptr->surface_ptr != nullptr)
    {
        void** const vtable = APIInterceptor::get_vtable_ptr(inout_staging_ptr->surface_ptr);

        APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_RELEASE>(vtable,
                                                                                                 Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_RELEASE)(inout_staging_ptr->surface_ptr);
    }

    *inout_staging_ptr = Staging();
}

std::string Stereo::ConverterDLL::D3D8HostMemoryExporter::take_timing_text()
{
    std::lock_guard<std::mutex> lock(m_series_mutex);

    return m_issue_cost.take_text("asking for the copies")        +
           "; "                                                   +
           m_lock_cost.take_text ("locking")                      +
           "; "                                                   +
           m_job_wait.take_text  ("waiting for the thread")       +
           "; "                                                   +
           m_write_cost.take_text("copy to the channel (thread)") +
           "; "                                                   +
           m_pace_wait.take_text ("waiting for the screen");
}

void Stereo::ConverterDLL::D3D8HostMemoryExporter::unlock_staging(Staging* inout_staging_ptr)
{
    if (inout_staging_ptr->is_locked)
    {
        void** const vtable = APIInterceptor::get_vtable_ptr(inout_staging_ptr->surface_ptr);

        APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3DSURFACE8_UNLOCKRECT>(vtable,
                                                                                                    Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3DSURFACE8_UNLOCKRECT)(inout_staging_ptr->surface_ptr);

        inout_staging_ptr->is_locked = false;
    }
}

void Stereo::ConverterDLL::D3D8HostMemoryExporter::wait_for_job()
{
    const double wait_start_ms = Utils::Statistics::SeriesStats::get_now();

    const auto unlock_surfaces_func = [this]()
    {
        for (uint32_t n_eye = 0;
                      n_eye < m_job.info.n_eyes;
                    ++n_eye)
        {
            unlock_staging(&m_staging_array.at(m_job_set).at(n_eye) );
        }
    };

    m_job_thread_ptr->wait(unlock_surfaces_func);

    {
        std::lock_guard<std::mutex> lock(m_series_mutex);

        m_job_wait.add(Utils::Statistics::SeriesStats::get_now() - wait_start_ms);
    }
}

void Stereo::ConverterDLL::D3D8HostMemoryExporter::wait_for_pace()
{
    const double start_ms = Utils::Statistics::SeriesStats::get_now();

    if (pace() )
    {
        std::lock_guard<std::mutex> lock(m_series_mutex);

        m_pace_wait.add(Utils::Statistics::SeriesStats::get_now() - start_ms);
    }
}

bool Stereo::ConverterDLL::D3D8HostMemoryExporter::write_eye(const uint32_t& in_eye,
                                                             uint8_t*        out_destination_ptr,
                                                             const uint32_t& in_pitch)
{
    const auto& description = m_job.description_array.at(in_eye);

    const bool is_written = PixelRowWriter::write_eye(description.Format,
                                                      description.Width,
                                                      description.Height,
                                                      m_job.source_ptr_array.at  (in_eye),
                                                      m_job.source_pitch_array.at(in_eye),
                                                      in_eye,
                                                      out_destination_ptr,
                                                      in_pitch,
                                                     &m_converted_u8_vec_array.at(in_eye) );

    if (is_written)
    {
        PixelRowWriter::blank_bars(m_job.picture,
                                   description.Width,
                                   description.Height,
                                   in_eye,
                                   out_destination_ptr,
                                   in_pitch);
        PixelRowWriter::make_opaque(description.Width,
                                    description.Height,
                                    in_eye,
                                    out_destination_ptr,
                                    in_pitch);
    }

    return is_written;
}

void Stereo::ConverterDLL::D3D8HostMemoryExporter::write_job()
{
    const double   start_ms        = Utils::Statistics::SeriesStats::get_now   ();
    uint8_t*       destination_ptr = get_channel_ptr()->begin_write_ptr        (m_job.info);
    bool           is_ok           = (destination_ptr != nullptr);
    const uint32_t pitch           = Stereo::Transport::FrameChannel::get_pitch(m_job.info.width);

    /* A second thread copies the right eye while this thread copies the left. The two threads do not touch the same memory. One eye is
     * 30 MB at 3200 x 2400, and copying it on a single thread is slow.
     */
    if (is_ok                 &&
        m_job.info.n_eyes > 1)
    {
        const auto prepare_func = [&]()
        {
            m_eye_destination_ptr = destination_ptr;
            m_eye_is_ok           = false;
            m_eye_pitch           = pitch;
        };

        m_eye_thread_ptr->queue(prepare_func);
    }

    if (is_ok)
    {
        is_ok = write_eye(0,
                          destination_ptr,
                          pitch);
    }

    if (destination_ptr   != nullptr &&
        m_job.info.n_eyes >  1)
    {
        const auto collect_func = [&]()
        {
            is_ok = (is_ok && m_eye_is_ok);
        };

        m_eye_thread_ptr->wait(collect_func);
    }

    if (is_ok)
    {
        get_channel_ptr()->end_write();
    }

    {
        std::lock_guard<std::mutex> lock(m_series_mutex);

        m_write_cost.add(Utils::Statistics::SeriesStats::get_now() - start_ms);
    }
}

void Stereo::ConverterDLL::D3D8HostMemoryExporter::write_right_eye()
{
    m_eye_is_ok = write_eye(1,
                            m_eye_destination_ptr,
                            m_eye_pitch);
}
