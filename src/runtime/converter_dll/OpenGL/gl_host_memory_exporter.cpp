/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_host_memory_exporter.h"
#include <stdio.h>
#include <string.h>
#include <vector>

namespace
{
    constexpr uint32_t BYTES_PER_PIXEL = 4;
    constexpr uint32_t N_EYES          = 2;

    /* Puts the rows of an image of an eye, from the bottom up as OpenGL gives them, into the image of both eyes with the top row first, as R, G, B, A turned
     * into B, G, R, A. The destination is pitch bytes from a row to the next, the eye is the place of the image across.
     */
    void write_eye_rows(const uint8_t*  in_rgba_ptr,
                        const uint32_t& in_width,
                        const uint32_t& in_height,
                        const uint32_t& in_eye,
                        const bool&     in_is_top_first,
                        uint8_t*        out_destination_ptr,
                        const uint32_t& in_pitch)
    {
        for (uint32_t n_row = 0;
                      n_row < in_height;
                    ++n_row)
        {
            const uint8_t* const source_ptr      = in_rgba_ptr + static_cast<size_t>(n_row) * in_width * BYTES_PER_PIXEL;
            const uint32_t       destination_row = (in_is_top_first) ? (in_height - 1 - n_row)
                                                                     : n_row;
            uint8_t* const       destination_ptr = out_destination_ptr + static_cast<size_t>(destination_row) * in_pitch + static_cast<size_t>(in_eye) * in_width * BYTES_PER_PIXEL;

            for (uint32_t n_column = 0;
                          n_column < in_width;
                        ++n_column)
            {
                destination_ptr[n_column * BYTES_PER_PIXEL + 0] = source_ptr[n_column * BYTES_PER_PIXEL + 2];
                destination_ptr[n_column * BYTES_PER_PIXEL + 1] = source_ptr[n_column * BYTES_PER_PIXEL + 1];
                destination_ptr[n_column * BYTES_PER_PIXEL + 2] = source_ptr[n_column * BYTES_PER_PIXEL + 0];
                destination_ptr[n_column * BYTES_PER_PIXEL + 3] = 0xFF;
            }
        }
    }
}

Stereo::ConverterDLL::GlHostMemoryExporter::GlHostMemoryExporter(Stereo::Transport::FrameChannel* inout_channel_ptr)
    :HostMemoryExporter(inout_channel_ptr)
{
    /* Stub */
}

Stereo::Transport::ExportStatus Stereo::ConverterDLL::GlHostMemoryExporter::export_frame(const uint64_t&                        in_frame_index,
                                                                                          const Stereo::Transport::ExportSource& in_source)
{
    const auto*                     images_ptr = static_cast<const GlFrameImages*>(in_source.left_surface_ptr);
    Stereo::Transport::ExportStatus result     = Stereo::Transport::ExportStatus::FAILED;

    if (is_host_memory_negotiated() == false)
    {
        result = Stereo::Transport::ExportStatus::NOT_READY;

        goto end;
    }

    if (images_ptr                             == nullptr ||
        in_source.right_surface_ptr            == nullptr ||
        images_ptr->left_rgba_ptr              == nullptr ||
        images_ptr->right_rgba_ptr             == nullptr ||
        images_ptr->width  * N_EYES            >  Stereo::Transport::FrameChannel::get_max_width () ||
        images_ptr->height                     >  Stereo::Transport::FrameChannel::get_max_height() )
    {
        goto end;
    }

    {
        Stereo::Transport::FrameInfo info;

        info.frame_index       = in_frame_index;
        info.height            = images_ptr->height;
        info.n_eyes            = N_EYES;
        info.timestamp_seconds = in_source.timestamp_seconds;
        info.width             = images_ptr->width * N_EYES;

        uint8_t* const destination_ptr = get_channel_ptr()->begin_write_ptr(info);
        const uint32_t pitch           = Stereo::Transport::FrameChannel::get_pitch(info.width);

        if (destination_ptr != nullptr)
        {
            write_eye_rows(images_ptr->left_rgba_ptr,
                           images_ptr->width,
                           images_ptr->height,
                           0, /* in_eye */
                           true,
                           destination_ptr,
                           pitch);
            write_eye_rows(images_ptr->right_rgba_ptr,
                           images_ptr->width,
                           images_ptr->height,
                           1, /* in_eye */
                           true,
                           destination_ptr,
                           pitch);

            get_channel_ptr()->end_write();

            result = Stereo::Transport::ExportStatus::EXPORTED;
        }
    }

end:
    return result;
}

void Stereo::ConverterDLL::GlHostMemoryExporter::wait_for_pace()
{
    wait_for_screen();
}

bool Stereo::ConverterDLL::GlHostMemoryExporter::write_debug_image(const GlFrameImages& in_images,
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

    if (in_images.left_rgba_ptr  != nullptr &&
        in_images.right_rgba_ptr != nullptr &&
        n_bytes                  != 0)
    {
        /* A BMP has its bottom row first, as OpenGL does. */
        write_eye_rows(in_images.left_rgba_ptr,
                       in_images.width,
                       in_images.height,
                       0, /* in_eye */
                       false,
                       pixel_u8_vec.data(),
                       pitch);
        write_eye_rows(in_images.right_rgba_ptr,
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
