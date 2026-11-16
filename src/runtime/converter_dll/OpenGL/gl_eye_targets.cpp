/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_eye_targets.h"
#include "gl_driver_functions.h"
#include "own_call_scope.h"
#include <algorithm>
#include <array>

const GLbitfield Stereo::ConverterDLL::GLEyeTargets::COLOR_BUFFER_BIT          = 0x00004000;
const GLbitfield Stereo::ConverterDLL::GLEyeTargets::DEPTH_BUFFER_BIT          = 0x00000100;
const GLenum     Stereo::ConverterDLL::GLEyeTargets::FRAMEBUFFER               = 0x8D40;
const GLenum     Stereo::ConverterDLL::GLEyeTargets::PACK_ALIGNMENT            = 0x0D05;
const GLenum     Stereo::ConverterDLL::GLEyeTargets::PIXEL_PACK_BUFFER         = 0x88EB;
const GLenum     Stereo::ConverterDLL::GLEyeTargets::PIXEL_PACK_BUFFER_BINDING = 0x88ED;

Stereo::ConverterDLL::GLEyeTargets::GLEyeTargets()
    :m_context_ptr               (nullptr),
     m_depth_buffer_array        (),
     m_generation                (0),
     m_has_functions             (false),
     m_has_depth_probe           (false),
     m_has_readback_target       (false),
     m_height                    (0),
     m_is_depth_buffer_busy_array(),
     m_is_ready                  (false),
     m_probe_height              (0),
     m_probe_width               (0),
     m_readback_height           (0),
     m_readback_width            (0),
     m_width                     (0)
{
    /* Stub */
}

bool Stereo::ConverterDLL::GLEyeTargets::are_pack_modes_default()
{
    constexpr GLenum PACK_LSB_FIRST   = 0x0D01;
    constexpr GLenum PACK_ROW_LENGTH  = 0x0D02;
    constexpr GLenum PACK_SKIP_PIXELS = 0x0D04;
    constexpr GLenum PACK_SKIP_ROWS   = 0x0D03;
    constexpr GLenum PACK_SWAP_BYTES  = 0x0D00;

    constexpr std::array<GLenum, 5> MODE_ARRAY = {PACK_LSB_FIRST,
                                                  PACK_ROW_LENGTH,
                                                  PACK_SKIP_PIXELS,
                                                  PACK_SKIP_ROWS,
                                                  PACK_SWAP_BYTES};

    GLint buffer = 0;
    bool  result = false;

    GLDriverFunctions::get().get_integerv_func_ptr(PIXEL_PACK_BUFFER_BINDING,
                                                  &buffer);

    result = (buffer == 0);

    for (const GLenum mode : MODE_ARRAY)
    {
        GLint value = 0;

        GLDriverFunctions::get().get_integerv_func_ptr(mode,
                                                      &value);

        result = (result && value == 0);
    }

    return result;
}

bool Stereo::ConverterDLL::GLEyeTargets::begin_depth_read(uint32_t* out_slot_ptr)
{
    constexpr size_t N_BYTES_PER_DEPTH = sizeof(float);
    constexpr GLenum STREAM_READ       = 0x88E1;

    const OwnCallScope own_call_scope;
    bool               result = false;

    if (has_depth_read_buffers() )
    {
        size_t n_slot = 0;

        while (n_slot < N_DEPTH_READ_SLOTS             &&
               m_is_depth_buffer_busy_array.at(n_slot) )
        {
            n_slot++;
        }

        if (n_slot < N_DEPTH_READ_SLOTS)
        {
            GLint saved_alignment   = 4;
            GLint saved_pack_buffer = 0;

            GLDriverFunctions::get().get_integerv_func_ptr(PIXEL_PACK_BUFFER_BINDING,
                                                          &saved_pack_buffer);

            /* The buffer of a slot is made when the slot is first used, with room for the small copy of the depth. */
            if (m_depth_buffer_array.at(n_slot) == 0)
            {
                m_functions.gen_buffers_func_ptr(1,
                                                &m_depth_buffer_array.at(n_slot) );
                m_functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                                 m_depth_buffer_array.at(n_slot) );
                m_functions.buffer_data_func_ptr(PIXEL_PACK_BUFFER,
                                                 static_cast<ptrdiff_t>(m_probe_width) * m_probe_height * N_BYTES_PER_DEPTH,
                                                 nullptr,
                                                 STREAM_READ);
            }
            else
            {
                m_functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                                 m_depth_buffer_array.at(n_slot) );
            }

            /* The depth buffer is copied to the small copy, and from there to the buffer of the slot. The graphics card does both without the CPU waiting. */
            blit_left_eye(m_picture,
                          m_depth_probe.framebuffer,
                          m_probe_width,
                          m_probe_height,
                          DEPTH_BUFFER_BIT);

            m_functions.bind_framebuffer_func_ptr(FRAMEBUFFER,
                                                  m_depth_probe.framebuffer);

            GLDriverFunctions::get().get_integerv_func_ptr(PACK_ALIGNMENT,
                                                          &saved_alignment);
            GLDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                           1);

            /* With a buffer bound, the pixels go into it starting at its beginning. */
            GLDriverFunctions::get().read_pixels_func_ptr(0,
                                                          0,
                                                          static_cast<GLsizei>(m_probe_width),
                                                          static_cast<GLsizei>(m_probe_height),
                                                          GL_DEPTH_COMPONENT,
                                                          GL_FLOAT,
                                                          nullptr);

            GLDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                           saved_alignment);

            /* The pixel-pack buffer that the application had bound is bound again. */
            m_functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                             static_cast<GLuint>(saved_pack_buffer) );

            bind_eye(Analysis::Recipe::Eye::LEFT);

            m_is_depth_buffer_busy_array.at(n_slot) = true;

            *out_slot_ptr = static_cast<uint32_t>(n_slot);
            result        = true;
        }
    }

    return result;
}

bool Stereo::ConverterDLL::GLEyeTargets::bind_eye(const Analysis::Recipe::Eye& in_eye)
{
    const OwnCallScope own_call_scope;

    if (m_is_ready)
    {
        m_functions.bind_framebuffer_func_ptr(FRAMEBUFFER,
                                              (in_eye == Analysis::Recipe::Eye::LEFT) ? m_left.framebuffer
                                                                                       : m_right.framebuffer);
    }

    return m_is_ready;
}

void Stereo::ConverterDLL::GLEyeTargets::blit_left_eye(const Stereo::Core::PixelRect& in_source,
                                                       const uint32_t&                in_destination_framebuffer,
                                                       const uint32_t&                in_destination_width,
                                                       const uint32_t&                in_destination_height,
                                                       const GLbitfield&              in_mask)
{
    constexpr GLenum DRAW_FRAMEBUFFER = 0x8CA9;
    constexpr GLenum READ_FRAMEBUFFER = 0x8CA8;
    constexpr GLenum SCISSOR_TEST     = 0x0C11;

    /* With the scissor test on, a blit is clipped to the scissor box, which the application has set for its own drawing. */
    const bool is_scissor_on = (GLDriverFunctions::get().is_enabled_func_ptr(SCISSOR_TEST) != GL_FALSE);

    if (is_scissor_on)
    {
        GLDriverFunctions::get().disable_func_ptr(SCISSOR_TEST);
    }

    m_functions.bind_framebuffer_func_ptr(READ_FRAMEBUFFER,
                                          m_left.framebuffer);
    m_functions.bind_framebuffer_func_ptr(DRAW_FRAMEBUFFER,
                                          in_destination_framebuffer);
    m_functions.blit_framebuffer_func_ptr(static_cast<GLint>(in_source.x),
                                          static_cast<GLint>(in_source.y),
                                          static_cast<GLint>(in_source.x + in_source.width),
                                          static_cast<GLint>(in_source.y + in_source.height),
                                          0,
                                          0,
                                          static_cast<GLint>(in_destination_width),
                                          static_cast<GLint>(in_destination_height),
                                          in_mask,
                                          GL_NEAREST);

    if (is_scissor_on)
    {
        GLDriverFunctions::get().enable_func_ptr(SCISSOR_TEST);
    }
}

void Stereo::ConverterDLL::GLEyeTargets::blit_left_to_window()
{
    const OwnCallScope own_call_scope;

    if (m_is_ready                                         &&
        m_functions.blit_framebuffer_func_ptr != nullptr)
    {
        blit_left_eye(get_whole_eye(),
                      0,
                      m_width,
                      m_height,
                      COLOR_BUFFER_BIT);

        m_functions.bind_framebuffer_func_ptr(FRAMEBUFFER,
                                              m_left.framebuffer);
    }
}

bool Stereo::ConverterDLL::GLEyeTargets::ensure(const void*                    in_context_ptr,
                                                const uint32_t&                in_width,
                                                const uint32_t&                in_height,
                                                const Stereo::Core::PixelRect& in_picture)
{
    /* The depth is read to measure how deep the scene is, and a copy of this width is enough for that. */
    constexpr uint32_t DEPTH_PROBE_WIDTH = 320;

    const OwnCallScope own_call_scope;

    if (m_is_ready                              &&
        m_context_ptr     == in_context_ptr     &&
        m_width           == in_width           &&
        m_height          == in_height          &&
        m_picture.height  == in_picture.height  &&
        m_picture.width   == in_picture.width   &&
        m_picture.x       == in_picture.x       &&
        m_picture.y       == in_picture.y)
    {
        goto end;
    }

    if (m_is_ready)
    {
        if (m_context_ptr != in_context_ptr)
        {
            /* A different context: the targets belong to the first context, and a second context is not converted. */
            goto end;
        }

        release_targets();
    }

    if (m_has_functions == false)
    {
        m_has_functions = m_functions.load();
    }

    if (m_has_functions                      == false ||
        in_width                             == 0     ||
        in_height                            == 0     ||
        in_picture.height                    == 0     ||
        in_picture.width                     == 0     ||
        in_picture.x + in_picture.width      >  in_width ||
        in_picture.y + in_picture.height     >  in_height)
    {
        goto end;
    }

    m_context_ptr = in_context_ptr;
    m_height      = in_height;
    m_picture     = in_picture;
    m_width       = in_width;

    if (make_target(m_width,
                    m_height,
                   &m_left)  == false ||
        make_target(m_width,
                    m_height,
                   &m_right) == false)
    {
        release_targets();

        goto end;
    }

    /* The small depth copy is not needed for rendering. If it cannot be made, the depth is read from the full buffer. */
    if (m_functions.blit_framebuffer_func_ptr != nullptr)
    {
        m_probe_width     = (std::min)(m_picture.width,
                                     DEPTH_PROBE_WIDTH);
        m_probe_height    = (std::max<uint32_t>)(1,
                                               static_cast<uint32_t>(static_cast<uint64_t>(m_picture.height) * m_probe_width / m_picture.width) );
        m_has_depth_probe = make_target(m_probe_width,
                                        m_probe_height,
                                       &m_depth_probe);

        if (m_has_depth_probe == false)
        {
            release_target(&m_depth_probe);
        }
    }

    m_is_ready = true;

    m_functions.bind_framebuffer_func_ptr(FRAMEBUFFER,
                                          m_left.framebuffer);

end:
    return m_is_ready;
}

bool Stereo::ConverterDLL::GLEyeTargets::finish_depth_read(const uint32_t&  in_slot,
                                                           const DepthFunc& in_depth_func)
{
    constexpr GLenum READ_ONLY = 0x88B8;

    const OwnCallScope own_call_scope;
    bool               result = false;

    if (in_slot < N_DEPTH_READ_SLOTS)
    {
        if (m_is_depth_buffer_busy_array.at(in_slot) &&
            m_depth_buffer_array.at(in_slot) != 0)
        {
            GLint saved_pack_buffer = 0;

            GLDriverFunctions::get().get_integerv_func_ptr(PIXEL_PACK_BUFFER_BINDING,
                                                          &saved_pack_buffer);

            m_functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                             m_depth_buffer_array.at(in_slot) );

            /* The read began a frame ago, so it is normally finished. Mapping the buffer waits if it is not. */
            const void* const data_ptr = m_functions.map_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                                                         READ_ONLY);

            if (data_ptr != nullptr)
            {
                in_depth_func(static_cast<const float*>(data_ptr),
                              static_cast<size_t>(m_probe_width) * m_probe_height);

                m_functions.unmap_buffer_func_ptr(PIXEL_PACK_BUFFER);

                result = true;
            }

            m_functions.bind_buffer_func_ptr(PIXEL_PACK_BUFFER,
                                             static_cast<GLuint>(saved_pack_buffer) );

            m_is_depth_buffer_busy_array.at(in_slot) = false;
        }
    }

    return result;
}

const void* Stereo::ConverterDLL::GLEyeTargets::get_context_ptr() const
{
    return m_context_ptr;
}

const Stereo::ConverterDLL::GLExtensionFunctions& Stereo::ConverterDLL::GLEyeTargets::get_functions() const
{
    return m_functions;
}

uint32_t Stereo::ConverterDLL::GLEyeTargets::get_generation() const
{
    return m_generation;
}

uint32_t Stereo::ConverterDLL::GLEyeTargets::get_height() const
{
    return m_height;
}

const Stereo::Core::PixelRect& Stereo::ConverterDLL::GLEyeTargets::get_picture() const
{
    return m_picture;
}

Stereo::Core::PixelRect Stereo::ConverterDLL::GLEyeTargets::get_whole_eye() const
{
    Stereo::Core::PixelRect result;

    result.height = m_height;
    result.width  = m_width;

    return result;
}

uint32_t Stereo::ConverterDLL::GLEyeTargets::get_width() const
{
    return m_width;
}

bool Stereo::ConverterDLL::GLEyeTargets::has_depth_read_buffers() const
{
    return (m_is_ready                                     &&
            m_has_depth_probe                              &&
            m_functions.bind_buffer_func_ptr    != nullptr &&
            m_functions.buffer_data_func_ptr    != nullptr &&
            m_functions.delete_buffers_func_ptr != nullptr &&
            m_functions.gen_buffers_func_ptr    != nullptr &&
            m_functions.map_buffer_func_ptr     != nullptr &&
            m_functions.unmap_buffer_func_ptr   != nullptr);
}

bool Stereo::ConverterDLL::GLEyeTargets::is_ready() const
{
    return m_is_ready;
}

bool Stereo::ConverterDLL::GLEyeTargets::make_target(const uint32_t& in_width,
                                                     const uint32_t& in_height,
                                                     EyeTarget*      out_target_ptr)
{
    constexpr GLenum COLOR_ATTACHMENT0        = 0x8CE0;
    constexpr GLenum DEPTH24_STENCIL8         = 0x88F0;
    constexpr GLenum DEPTH_STENCIL_ATTACHMENT = 0x821A;
    constexpr GLenum FRAMEBUFFER_COMPLETE     = 0x8CD5;
    constexpr GLenum RENDERBUFFER             = 0x8D41;

    m_functions.gen_renderbuffers_func_ptr(1,
                                          &out_target_ptr->color_renderbuffer);
    m_functions.gen_renderbuffers_func_ptr(1,
                                          &out_target_ptr->depth_renderbuffer);
    m_functions.gen_framebuffers_func_ptr (1,
                                          &out_target_ptr->framebuffer);

    m_functions.bind_renderbuffer_func_ptr   (RENDERBUFFER,
                                              out_target_ptr->color_renderbuffer);
    m_functions.renderbuffer_storage_func_ptr(RENDERBUFFER,
                                              GL_RGBA8,
                                              static_cast<GLsizei>(in_width),
                                              static_cast<GLsizei>(in_height) );
    m_functions.bind_renderbuffer_func_ptr   (RENDERBUFFER,
                                              out_target_ptr->depth_renderbuffer);
    m_functions.renderbuffer_storage_func_ptr(RENDERBUFFER,
                                              DEPTH24_STENCIL8,
                                              static_cast<GLsizei>(in_width),
                                              static_cast<GLsizei>(in_height) );
    m_functions.bind_renderbuffer_func_ptr   (RENDERBUFFER,
                                              0);

    m_functions.bind_framebuffer_func_ptr        (FRAMEBUFFER,
                                                  out_target_ptr->framebuffer);
    m_functions.framebuffer_renderbuffer_func_ptr(FRAMEBUFFER,
                                                  COLOR_ATTACHMENT0,
                                                  RENDERBUFFER,
                                                  out_target_ptr->color_renderbuffer);
    m_functions.framebuffer_renderbuffer_func_ptr(FRAMEBUFFER,
                                                  DEPTH_STENCIL_ATTACHMENT,
                                                  RENDERBUFFER,
                                                  out_target_ptr->depth_renderbuffer);

    return (m_functions.check_framebuffer_status_func_ptr(FRAMEBUFFER) == FRAMEBUFFER_COMPLETE);
}

bool Stereo::ConverterDLL::GLEyeTargets::read_depth(std::vector<float>* out_depth_f32_vec_ptr,
                                                    uint32_t*           out_width_ptr,
                                                    uint32_t*           out_height_ptr)
{
    const OwnCallScope own_call_scope;
    GLint              saved_alignment = 4;
    const bool         result          = m_is_ready;

    if (m_is_ready)
    {
        const uint32_t width  = (m_has_depth_probe) ? m_probe_width
                                                    : m_picture.width;
        const uint32_t height = (m_has_depth_probe) ? m_probe_height
                                                    : m_picture.height;
        const GLint    x      = (m_has_depth_probe) ? 0
                                                    : static_cast<GLint>(m_picture.x);
        const GLint    y      = (m_has_depth_probe) ? 0
                                                    : static_cast<GLint>(m_picture.y);

        out_depth_f32_vec_ptr->resize(static_cast<size_t>(width) * height);

        *out_height_ptr = height;
        *out_width_ptr  = width;

        if (m_has_depth_probe)
        {
            /* The graphics card scales the whole depth buffer down to the small copy. */
            blit_left_eye(m_picture,
                          m_depth_probe.framebuffer,
                          m_probe_width,
                          m_probe_height,
                          DEPTH_BUFFER_BIT);

            m_functions.bind_framebuffer_func_ptr(FRAMEBUFFER,
                                                  m_depth_probe.framebuffer);
        }
        else
        {
            bind_eye(Analysis::Recipe::Eye::LEFT);
        }

        GLDriverFunctions::get().get_integerv_func_ptr(PACK_ALIGNMENT,
                                                       &saved_alignment);
        GLDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                       1);

        GLDriverFunctions::get().read_pixels_func_ptr(x,
                                                      y,
                                                      static_cast<GLsizei>(width),
                                                      static_cast<GLsizei>(height),
                                                      GL_DEPTH_COMPONENT,
                                                      GL_FLOAT,
                                                      out_depth_f32_vec_ptr->data() );

        GLDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                       saved_alignment);

        bind_eye(Analysis::Recipe::Eye::LEFT);
    }

    return result;
}

bool Stereo::ConverterDLL::GLEyeTargets::read_eye(const Analysis::Recipe::Eye& in_eye,
                                                  void*                        out_pixels_ptr)
{
    constexpr GLenum BGRA = 0x80E1;

    const OwnCallScope own_call_scope;
    GLint              saved_alignment = 4;
    const bool         result          = m_is_ready;

    if (m_is_ready)
    {
        GLDriverFunctions::get().get_integerv_func_ptr(PACK_ALIGNMENT,
                                                       &saved_alignment);
        GLDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                       1);

        bind_eye(in_eye);

        GLDriverFunctions::get().read_pixels_func_ptr(0,
                                                      0,
                                                      static_cast<GLsizei>(m_width),
                                                      static_cast<GLsizei>(m_height),
                                                      BGRA,
                                                      GL_UNSIGNED_BYTE,
                                                      out_pixels_ptr);

        GLDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                       saved_alignment);

        bind_eye(Analysis::Recipe::Eye::LEFT);
    }

    return result;
}

bool Stereo::ConverterDLL::GLEyeTargets::read_picture(const uint32_t&       in_width,
                                                      const uint32_t&       in_height,
                                                      std::vector<uint8_t>* out_rgba_u8_vec_ptr)
{
    constexpr size_t N_CHANNELS = 4;

    const OwnCallScope own_call_scope;
    GLint              saved_alignment = 4;
    bool               result          = false;

    if (m_is_ready                                         &&
        m_functions.blit_framebuffer_func_ptr != nullptr   &&
        in_width                              != 0         &&
        in_height                             != 0)
    {
        /* The target is made once per size and kept, because the picture is read from it again and again. */
        if (m_readback_width  != in_width ||
            m_readback_height != in_height)
        {
            release_target(&m_readback_target);

            m_has_readback_target = make_target(in_width,
                                                in_height,
                                               &m_readback_target);
            m_readback_height     = in_height;
            m_readback_width      = in_width;

            if (m_has_readback_target == false)
            {
                release_target(&m_readback_target);
            }

            bind_eye(Analysis::Recipe::Eye::LEFT);
        }

        if (m_has_readback_target)
        {
            const bool are_modes_default = are_pack_modes_default();

            if (are_modes_default)
            {
                out_rgba_u8_vec_ptr->resize(static_cast<size_t>(in_width) * in_height * N_CHANNELS);

                blit_left_eye(m_picture,
                              m_readback_target.framebuffer,
                              in_width,
                              in_height,
                              COLOR_BUFFER_BIT);

                m_functions.bind_framebuffer_func_ptr(FRAMEBUFFER,
                                                      m_readback_target.framebuffer);

                GLDriverFunctions::get().get_integerv_func_ptr(PACK_ALIGNMENT,
                                                              &saved_alignment);
                GLDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                               1);

                GLDriverFunctions::get().read_pixels_func_ptr(0,
                                                              0,
                                                              static_cast<GLsizei>(in_width),
                                                              static_cast<GLsizei>(in_height),
                                                              GL_RGBA,
                                                              GL_UNSIGNED_BYTE,
                                                              out_rgba_u8_vec_ptr->data() );

                GLDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                               saved_alignment);

                bind_eye(Analysis::Recipe::Eye::LEFT);

                result = true;
            }
        }
    }

    return result;
}

void Stereo::ConverterDLL::GLEyeTargets::release_target(EyeTarget* inout_target_ptr)
{
    if (inout_target_ptr->framebuffer != 0)
    {
        m_functions.delete_framebuffers_func_ptr (1,
                                                 &inout_target_ptr->framebuffer);
    }

    if (inout_target_ptr->color_renderbuffer != 0)
    {
        m_functions.delete_renderbuffers_func_ptr(1,
                                                 &inout_target_ptr->color_renderbuffer);
    }

    if (inout_target_ptr->depth_renderbuffer != 0)
    {
        m_functions.delete_renderbuffers_func_ptr(1,
                                                 &inout_target_ptr->depth_renderbuffer);
    }

    *inout_target_ptr = EyeTarget();
}

void Stereo::ConverterDLL::GLEyeTargets::release_targets()
{
    release_target(&m_depth_probe);
    release_target(&m_left);
    release_target(&m_readback_target);
    release_target(&m_right);

    /* The buffers of the graphics card that the depth was read into are released. A read that was in progress is lost with them. */
    for (size_t n_slot = 0;
                n_slot < N_DEPTH_READ_SLOTS;
              ++n_slot)
    {
        if (m_depth_buffer_array.at(n_slot) != 0)
        {
            m_functions.delete_buffers_func_ptr(1,
                                               &m_depth_buffer_array.at(n_slot) );
        }

        m_depth_buffer_array.at(n_slot)         = 0;
        m_is_depth_buffer_busy_array.at(n_slot) = false;
    }

    m_generation++;

    m_has_depth_probe     = false;
    m_has_readback_target = false;
    m_is_ready            = false;
    m_readback_height     = 0;
    m_readback_width      = 0;
}
