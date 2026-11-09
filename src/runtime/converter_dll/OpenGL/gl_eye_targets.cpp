/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_eye_targets.h"
#include "gl_driver_functions.h"
#include "own_call_scope.h"
#include <algorithm>

namespace
{
    constexpr GLenum COLOR_ATTACHMENT0        = 0x8CE0;
    constexpr GLenum DEPTH24_STENCIL8         = 0x88F0;
    constexpr GLenum DEPTH_STENCIL_ATTACHMENT = 0x821A;
    constexpr GLenum DRAW_FRAMEBUFFER         = 0x8CA9;
    constexpr GLenum FRAMEBUFFER              = 0x8D40;
    constexpr GLenum FRAMEBUFFER_COMPLETE     = 0x8CD5;
    constexpr GLenum PACK_ALIGNMENT           = 0x0D05;
    constexpr GLenum READ_FRAMEBUFFER         = 0x8CA8;
    constexpr GLenum RENDERBUFFER             = 0x8D41;
    constexpr GLenum SCISSOR_TEST             = 0x0C11;

    /* A depth is read for how deep the scene is, and a copy of it this wide says that. */
    constexpr uint32_t DEPTH_PROBE_WIDTH = 320;
}

Stereo::ConverterDLL::GlEyeTargets::GlEyeTargets()
    :m_context_ptr    (nullptr),
     m_has_functions  (false),
     m_has_depth_probe(false),
     m_height         (0),
     m_is_ready       (false),
     m_probe_height   (0),
     m_probe_width    (0),
     m_width          (0)
{
    /* Stub */
}

bool Stereo::ConverterDLL::GlEyeTargets::bind_eye(const Analysis::Recipe::Eye& in_eye)
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

void Stereo::ConverterDLL::GlEyeTargets::blit_left_to_window()
{
    const OwnCallScope own_call_scope;

    if (m_is_ready                                         &&
        m_functions.blit_framebuffer_func_ptr != nullptr)
    {
        constexpr GLbitfield COLOR_BUFFER_BIT = 0x00004000;

        /* A blit is cut to the scissor box if the test is on, which the application has made the box of something else. */
        const bool is_scissor_on = (GlDriverFunctions::get().is_enabled_func_ptr(SCISSOR_TEST) != GL_FALSE);

        if (is_scissor_on)
        {
            GlDriverFunctions::get().disable_func_ptr(SCISSOR_TEST);
        }

        m_functions.bind_framebuffer_func_ptr(READ_FRAMEBUFFER,
                                              m_left.framebuffer);
        m_functions.bind_framebuffer_func_ptr(DRAW_FRAMEBUFFER,
                                              0);
        m_functions.blit_framebuffer_func_ptr(0,
                                              0,
                                              static_cast<GLint>(m_width),
                                              static_cast<GLint>(m_height),
                                              0,
                                              0,
                                              static_cast<GLint>(m_width),
                                              static_cast<GLint>(m_height),
                                              COLOR_BUFFER_BIT,
                                              GL_NEAREST);
        m_functions.bind_framebuffer_func_ptr(FRAMEBUFFER,
                                              m_left.framebuffer);

        if (is_scissor_on)
        {
            GlDriverFunctions::get().enable_func_ptr(SCISSOR_TEST);
        }
    }
}

bool Stereo::ConverterDLL::GlEyeTargets::ensure(const void*     in_context_ptr,
                                                const uint32_t& in_width,
                                                const uint32_t& in_height)
{
    const OwnCallScope own_call_scope;

    if (m_is_ready                      &&
        m_context_ptr == in_context_ptr &&
        m_width       == in_width       &&
        m_height      == in_height)
    {
        goto end;
    }

    if (m_is_ready)
    {
        if (m_context_ptr != in_context_ptr)
        {
            /* Another context: the targets are those of the first, and a second context is not converted. */
            goto end;
        }

        release_targets();
    }

    if (m_has_functions == false)
    {
        m_has_functions = m_functions.load();
    }

    if (m_has_functions == false ||
        in_width        == 0     ||
        in_height       == 0)
    {
        goto end;
    }

    m_context_ptr = in_context_ptr;
    m_height      = in_height;
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

    /* The copy of the depth is not needed to render: if it cannot be made the depth is read from the buffer itself. */
    if (m_functions.blit_framebuffer_func_ptr != nullptr)
    {
        m_probe_width     = (std::min)(m_width,
                                     DEPTH_PROBE_WIDTH);
        m_probe_height    = (std::max<uint32_t>)(1,
                                               static_cast<uint32_t>(static_cast<uint64_t>(m_height) * m_probe_width / m_width) );
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

const void* Stereo::ConverterDLL::GlEyeTargets::get_context_ptr() const
{
    return m_context_ptr;
}

const Stereo::ConverterDLL::GlExtensionFunctions& Stereo::ConverterDLL::GlEyeTargets::get_functions() const
{
    return m_functions;
}

uint32_t Stereo::ConverterDLL::GlEyeTargets::get_height() const
{
    return m_height;
}

uint32_t Stereo::ConverterDLL::GlEyeTargets::get_width() const
{
    return m_width;
}

bool Stereo::ConverterDLL::GlEyeTargets::is_ready() const
{
    return m_is_ready;
}

bool Stereo::ConverterDLL::GlEyeTargets::make_target(const uint32_t& in_width,
                                                     const uint32_t& in_height,
                                                     EyeTarget*      out_target_ptr)
{
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

bool Stereo::ConverterDLL::GlEyeTargets::read_depth(std::vector<float>* out_depth_f32_vec_ptr,
                                                    uint32_t*           out_width_ptr,
                                                    uint32_t*           out_height_ptr)
{
    const OwnCallScope own_call_scope;
    GLint              saved_alignment = 4;
    const bool         result          = m_is_ready;

    if (m_is_ready)
    {
        const uint32_t width  = (m_has_depth_probe) ? m_probe_width
                                                    : m_width;
        const uint32_t height = (m_has_depth_probe) ? m_probe_height
                                                    : m_height;

        out_depth_f32_vec_ptr->resize(static_cast<size_t>(width) * height);

        *out_height_ptr = height;
        *out_width_ptr  = width;

        if (m_has_depth_probe)
        {
            constexpr GLbitfield DEPTH_BUFFER_BIT = 0x00000100;

            /* The depth of the whole buffer goes to the small one, by the graphics card; a blit is cut to the scissor box if the test is on. */
            const bool is_scissor_on = (GlDriverFunctions::get().is_enabled_func_ptr(SCISSOR_TEST) != GL_FALSE);

            if (is_scissor_on)
            {
                GlDriverFunctions::get().disable_func_ptr(SCISSOR_TEST);
            }

            m_functions.bind_framebuffer_func_ptr(READ_FRAMEBUFFER,
                                                  m_left.framebuffer);
            m_functions.bind_framebuffer_func_ptr(DRAW_FRAMEBUFFER,
                                                  m_depth_probe.framebuffer);
            m_functions.blit_framebuffer_func_ptr(0,
                                                  0,
                                                  static_cast<GLint>(m_width),
                                                  static_cast<GLint>(m_height),
                                                  0,
                                                  0,
                                                  static_cast<GLint>(m_probe_width),
                                                  static_cast<GLint>(m_probe_height),
                                                  DEPTH_BUFFER_BIT,
                                                  GL_NEAREST);

            if (is_scissor_on)
            {
                GlDriverFunctions::get().enable_func_ptr(SCISSOR_TEST);
            }

            m_functions.bind_framebuffer_func_ptr(FRAMEBUFFER,
                                                  m_depth_probe.framebuffer);
        }
        else
        {
            bind_eye(Analysis::Recipe::Eye::LEFT);
        }

        GlDriverFunctions::get().get_integerv_func_ptr(PACK_ALIGNMENT,
                                                       &saved_alignment);
        GlDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                       1);

        GlDriverFunctions::get().read_pixels_func_ptr(0,
                                                      0,
                                                      static_cast<GLsizei>(width),
                                                      static_cast<GLsizei>(height),
                                                      GL_DEPTH_COMPONENT,
                                                      GL_FLOAT,
                                                      out_depth_f32_vec_ptr->data() );

        GlDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                       saved_alignment);

        bind_eye(Analysis::Recipe::Eye::LEFT);
    }

    return result;
}

bool Stereo::ConverterDLL::GlEyeTargets::read_eye(const Analysis::Recipe::Eye& in_eye,
                                                  void*                        out_pixels_ptr)
{
    constexpr GLenum BGRA = 0x80E1;

    const OwnCallScope own_call_scope;
    GLint              saved_alignment = 4;
    const bool         result          = m_is_ready;

    if (m_is_ready)
    {
        GlDriverFunctions::get().get_integerv_func_ptr(PACK_ALIGNMENT,
                                                       &saved_alignment);
        GlDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                       1);

        bind_eye(in_eye);

        GlDriverFunctions::get().read_pixels_func_ptr(0,
                                                      0,
                                                      static_cast<GLsizei>(m_width),
                                                      static_cast<GLsizei>(m_height),
                                                      BGRA,
                                                      GL_UNSIGNED_BYTE,
                                                      out_pixels_ptr);

        GlDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                       saved_alignment);

        bind_eye(Analysis::Recipe::Eye::LEFT);
    }

    return result;
}

void Stereo::ConverterDLL::GlEyeTargets::release_target(EyeTarget* inout_target_ptr)
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

void Stereo::ConverterDLL::GlEyeTargets::release_targets()
{
    release_target(&m_depth_probe);
    release_target(&m_left);
    release_target(&m_right);

    m_has_depth_probe = false;
    m_is_ready        = false;
}
