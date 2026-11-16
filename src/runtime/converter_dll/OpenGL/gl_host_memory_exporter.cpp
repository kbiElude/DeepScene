/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_host_memory_exporter.h"
#include "pixel_row_writer.h"
#include <stdio.h>
#include <string.h>
#include <vector>

const uint32_t Stereo::ConverterDLL::GLHostMemoryExporter::BYTES_PER_PIXEL = 4;
const uint32_t Stereo::ConverterDLL::GLHostMemoryExporter::N_EYES          = 2;

Stereo::ConverterDLL::GLHostMemoryExporter::GLHostMemoryExporter(Stereo::Transport::FrameChannel*     inout_channel_ptr,
                                                                 const Analysis::Recipe::FramePacing& in_frame_pacing)
    :HostMemoryExporterBase(inout_channel_ptr,
                            in_frame_pacing)
{
    const auto write_job_func = [this]()
    {
        write_job();
    };

    m_job_thread_ptr.reset(new Utils::Parallel::JobThread(write_job_func) );
}

Stereo::ConverterDLL::GLHostMemoryExporter::~GLHostMemoryExporter()
{
    m_job_thread_ptr->stop();
}

Stereo::Transport::ExportStatus Stereo::ConverterDLL::GLHostMemoryExporter::export_frame(const uint64_t&                        in_frame_index,
                                                                                         const Stereo::Transport::ExportSource& in_source)
{
    const auto*                     images_ptr = static_cast<const GLFrameImages*>(in_source.left_surface_ptr);
    Stereo::Transport::ExportStatus result     = Stereo::Transport::ExportStatus::FAILED;

    if (is_host_memory_negotiated() == false)
    {
        result = Stereo::Transport::ExportStatus::NOT_READY;

        goto end;
    }

    if (images_ptr                  == nullptr ||
        in_source.right_surface_ptr == nullptr ||
        images_ptr->left_bgra_ptr   == nullptr ||
        images_ptr->right_bgra_ptr  == nullptr ||
        images_ptr->width  * N_EYES >  Stereo::Transport::FrameChannel::get_max_width () ||
        images_ptr->height          >  Stereo::Transport::FrameChannel::get_max_height() )
    {
        goto end;
    }

    {
        const auto prepare_func = [&]()
        {
            m_job.frame_index       = in_frame_index;
            m_job.images            = *images_ptr;
            m_job.timestamp_seconds = in_source.timestamp_seconds;
        };

        /* The thread is still working on the previous frame, so this frame is left out. */
        result = (m_job_thread_ptr->try_queue(prepare_func) ) ? Stereo::Transport::ExportStatus::EXPORTED
                                                              : Stereo::Transport::ExportStatus::NOT_READY;
    }

end:
    return result;
}

void Stereo::ConverterDLL::GLHostMemoryExporter::wait_for_pace()
{
    pace();
}

bool Stereo::ConverterDLL::GLHostMemoryExporter::write_debug_image(const GLFrameImages& in_images,
                                                                   const std::string&   in_path)
{
    constexpr uint32_t HEADER_N_BYTES   = 54;
    constexpr uint32_t INFO_N_BYTES     = 40;
    constexpr uint16_t N_BITS_PER_PIXEL = 32;
    constexpr uint16_t N_PLANES         = 1;

    const uint32_t       width   = in_images.width * N_EYES;
    const uint32_t       pitch   = width * BYTES_PER_PIXEL;
    const uint32_t       n_bytes = pitch * in_images.height;
    std::vector<uint8_t> pixel_u8_vec(n_bytes);
    bool                 result  = false;

    if (in_images.left_bgra_ptr  != nullptr &&
        in_images.right_bgra_ptr != nullptr &&
        n_bytes                  != 0)
    {
        /* A BMP stores its bottom row first, as OpenGL does. */
        write_eye_rows(in_images.left_bgra_ptr,
                       in_images.width,
                       in_images.height,
                       0, /* in_eye */
                       false,
                       pixel_u8_vec.data(),
                       pitch);
        write_eye_rows(in_images.right_bgra_ptr,
                       in_images.width,
                       in_images.height,
                       1, /* in_eye */
                       false,
                       pixel_u8_vec.data(),
                       pitch);

        FILE* file_ptr = nullptr;

        if (::fopen_s(&file_ptr,
                      in_path.c_str(),
                      "wb") == 0)
        {
            uint8_t header_u8vec54[HEADER_N_BYTES] = {};

            const uint32_t file_n_bytes   = HEADER_N_BYTES + n_bytes;
            const uint32_t data_offset    = HEADER_N_BYTES;
            const int32_t  bmp_width      = static_cast<int32_t>(width);
            const int32_t  bmp_height     = static_cast<int32_t>(in_images.height);

            header_u8vec54[0] = 'B';
            header_u8vec54[1] = 'M';

            ::memcpy(header_u8vec54 + 2,  &file_n_bytes,     sizeof(file_n_bytes) );
            ::memcpy(header_u8vec54 + 10, &data_offset,      sizeof(data_offset) );
            ::memcpy(header_u8vec54 + 14, &INFO_N_BYTES,     sizeof(INFO_N_BYTES) );
            ::memcpy(header_u8vec54 + 18, &bmp_width,        sizeof(bmp_width) );
            ::memcpy(header_u8vec54 + 22, &bmp_height,       sizeof(bmp_height) );
            ::memcpy(header_u8vec54 + 26, &N_PLANES,         sizeof(N_PLANES) );
            ::memcpy(header_u8vec54 + 28, &N_BITS_PER_PIXEL, sizeof(N_BITS_PER_PIXEL) );

            result = (::fwrite(header_u8vec54,
                               1,
                               HEADER_N_BYTES,
                               file_ptr) == HEADER_N_BYTES &&
                      ::fwrite(pixel_u8_vec.data(),
                               1,
                               n_bytes,
                               file_ptr) == n_bytes);

            ::fclose(file_ptr);
        }
    }

    return result;
}

void Stereo::ConverterDLL::GLHostMemoryExporter::write_eye_rows(const uint8_t*  in_bgra_ptr,
                                                                const uint32_t& in_width,
                                                                const uint32_t& in_height,
                                                                const uint32_t& in_eye,
                                                                const bool&     in_is_turned_over,
                                                                uint8_t*        out_destination_ptr,
                                                                const uint32_t& in_pitch)
{
    const size_t row_n_bytes = static_cast<size_t>(in_width) * BYTES_PER_PIXEL;

    for (uint32_t n_row = 0;
                  n_row < in_height;
                ++n_row)
    {
        const uint32_t destination_row = (in_is_turned_over) ? (in_height - 1 - n_row)
                                                             : n_row;

        ::memcpy(out_destination_ptr + static_cast<size_t>(destination_row) * in_pitch + static_cast<size_t>(in_eye) * row_n_bytes,
                 in_bgra_ptr         + static_cast<size_t>(n_row)           * row_n_bytes,
                 row_n_bytes);
    }
}

void Stereo::ConverterDLL::GLHostMemoryExporter::write_job()
{
    Job job = m_job;

    {
        Stereo::Transport::FrameInfo info;

        info.frame_index       = job.frame_index;
        info.height            = job.images.height;
        info.n_eyes            = N_EYES;
        info.timestamp_seconds = job.timestamp_seconds;
        info.width             = job.images.width * N_EYES;

        uint8_t* const destination_ptr = get_channel_ptr()->begin_write_ptr(info);
        const uint32_t pitch           = Stereo::Transport::FrameChannel::get_pitch(info.width);

        if (destination_ptr != nullptr)
        {
            write_eye_rows(job.images.left_bgra_ptr,
                           job.images.width,
                           job.images.height,
                           0, /* in_eye */
                           true,
                           destination_ptr,
                           pitch);
            write_eye_rows(job.images.right_bgra_ptr,
                           job.images.width,
                           job.images.height,
                           1, /* in_eye */
                           true,
                           destination_ptr,
                           pitch);

            PixelRowWriter::blank_bars(job.images.picture,
                                       job.images.width,
                                       job.images.height,
                                       0, /* in_eye */
                                       destination_ptr,
                                       pitch);
            PixelRowWriter::blank_bars(job.images.picture,
                                       job.images.width,
                                       job.images.height,
                                       1, /* in_eye */
                                       destination_ptr,
                                       pitch);

            get_channel_ptr()->end_write();
        }
    }

    /* The image memory goes back to the reader once all copies of the token are gone. */
    job   = Job();
    m_job = Job();
}
