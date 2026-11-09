/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_frame_reader.h"
#include "own_call_scope.h"
#include <thread>

namespace
{
    constexpr GLenum PIXEL_PACK_BUFFER = 0x88EB;
    constexpr GLenum READ_ONLY         = 0x88B8;
    constexpr GLenum STREAM_READ       = 0x88E1;
}

Stereo::ConverterDLL::GlFrameReader::GlFrameReader()
    :m_n_reads(0)
{
    /* Stub */
}

void Stereo::ConverterDLL::GlFrameReader::finish_set(const GlEyeTargets& in_targets,
                                                     Set*                inout_set_ptr)
{
    const auto& functions = in_targets.get_functions();

    /* What was given of the set is being copied by another thread, which is done when it lets go of its token: far less than a frame. */
    while (inout_set_ptr->consumer.expired() == false)
    {
        std::this_thread::yield();
    }

    if (inout_set_ptr->is_mapped)
    {
        for (size_t n_eye = 0;
                    n_eye < N_EYES;
                  ++n_eye)
        {
            functions.bind_buffer_func_ptr (PIXEL_PACK_BUFFER,
                                            inout_set_ptr->buffer_array.at(n_eye) );
            functions.unmap_buffer_func_ptr(PIXEL_PACK_BUFFER);
        }

        functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                       0);

        inout_set_ptr->is_mapped = false;
    }
}

bool Stereo::ConverterDLL::GlFrameReader::read(GlEyeTargets*  inout_targets_ptr,
                                               GlFrameImages* out_images_ptr)
{
    const OwnCallScope own_call_scope;
    const auto&        functions   = inout_targets_ptr->get_functions();
    const bool         has_buffers = (functions.bind_buffer_func_ptr != nullptr);
    const uint32_t     height      = inout_targets_ptr->get_height();
    const uint32_t     width       = inout_targets_ptr->get_width();
    Set&               current     = m_set_array.at( m_n_reads      % N_SETS);
    Set&               previous    = m_set_array.at( (m_n_reads + 1) % N_SETS);
    bool               result      = false;

    if (inout_targets_ptr->is_ready() == false)
    {
        goto end;
    }

    /* What was read for another size is of no use. */
    if (current.width  != width  ||
        current.height != height)
    {
        release_set(*inout_targets_ptr,
                    &current);
        release_set(*inout_targets_ptr,
                    &previous);

        current.height  = height;
        current.width   = width;
        previous.height = height;
        previous.width  = width;
    }

    finish_set(*inout_targets_ptr,
               &current);

    if (has_buffers)
    {
        const ptrdiff_t n_bytes = static_cast<ptrdiff_t>(width) * height * 4;

        for (size_t n_eye = 0;
                    n_eye < N_EYES;
                  ++n_eye)
        {
            if (current.buffer_array.at(n_eye) == 0)
            {
                functions.gen_buffers_func_ptr(1,
                                              &current.buffer_array.at(n_eye) );
                functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                               current.buffer_array.at(n_eye) );
                functions.buffer_data_func_ptr(PIXEL_PACK_BUFFER,
                                               n_bytes,
                                               nullptr,
                                               STREAM_READ);
            }
            else
            {
                functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                               current.buffer_array.at(n_eye) );
            }

            /* With a buffer bound the pixels go to it, at its start. */
            inout_targets_ptr->read_eye( (n_eye == 0) ? Analysis::Recipe::Eye::LEFT
                                                      : Analysis::Recipe::Eye::RIGHT,
                                        nullptr);
        }

        functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                       0);

        current.has_frame = true;

        /* The frame before has been on its way for a whole frame. */
        if (previous.has_frame)
        {
            bool is_mapped = true;

            for (size_t n_eye = 0;
                        n_eye < N_EYES;
                      ++n_eye)
            {
                functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                               previous.buffer_array.at(n_eye) );

                previous.mapped_ptr_array.at(n_eye) = static_cast<const uint8_t*>(functions.map_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                                                                                                READ_ONLY) );

                is_mapped = (is_mapped && previous.mapped_ptr_array.at(n_eye) != nullptr);
            }

            functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                           0);

            previous.has_frame = false;
            previous.is_mapped = true;

            if (is_mapped)
            {
                const std::shared_ptr<const void> token = std::make_shared<int>(0);

                previous.consumer = token;

                out_images_ptr->consumer_token = token;
                out_images_ptr->height         = height;
                out_images_ptr->left_bgra_ptr  = previous.mapped_ptr_array.at(0);
                out_images_ptr->right_bgra_ptr = previous.mapped_ptr_array.at(1);
                out_images_ptr->width          = width;

                result = true;
            }
        }
    }
    else
    {
        for (size_t n_eye = 0;
                    n_eye < N_EYES;
                  ++n_eye)
        {
            current.pixel_u8_vec_array.at(n_eye).resize(static_cast<size_t>(width) * height * 4);

            inout_targets_ptr->read_eye( (n_eye == 0) ? Analysis::Recipe::Eye::LEFT
                                                      : Analysis::Recipe::Eye::RIGHT,
                                        current.pixel_u8_vec_array.at(n_eye).data() );
        }

        const std::shared_ptr<const void> token = std::make_shared<int>(0);

        current.consumer = token;

        out_images_ptr->consumer_token = token;
        out_images_ptr->height         = height;
        out_images_ptr->left_bgra_ptr  = current.pixel_u8_vec_array.at(0).data();
        out_images_ptr->right_bgra_ptr = current.pixel_u8_vec_array.at(1).data();
        out_images_ptr->width          = width;

        result = true;
    }

end:
    m_n_reads++;

    return result;
}

void Stereo::ConverterDLL::GlFrameReader::release_set(const GlEyeTargets& in_targets,
                                                      Set*                inout_set_ptr)
{
    const auto& functions = in_targets.get_functions();

    finish_set(in_targets,
               inout_set_ptr);

    for (auto& buffer : inout_set_ptr->buffer_array)
    {
        if (buffer != 0 &&
            functions.delete_buffers_func_ptr != nullptr)
        {
            functions.delete_buffers_func_ptr(1,
                                             &buffer);
        }

        buffer = 0;
    }

    for (auto& pixel_vec : inout_set_ptr->pixel_u8_vec_array)
    {
        pixel_vec.clear();
        pixel_vec.shrink_to_fit();
    }

    inout_set_ptr->has_frame = false;
}
