/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "gl_eye_targets.h"
#include "gl_driver_functions.h"
#include "own_call_scope.h"


namespace
{
    constexpr GLenum COLOR_ATTACHMENT0        = 0x8CE0;
    constexpr GLenum DEPTH24_STENCIL8         = 0x88F0;
    constexpr GLenum DEPTH_STENCIL_ATTACHMENT = 0x821A;
    constexpr GLenum DRAW_FRAMEBUFFER         = 0x8CA9;
    constexpr GLenum FRAMEBUFFER              = 0x8D40;
    constexpr GLenum FRAMEBUFFER_COMPLETE     = 0x8CD5;
    constexpr GLenum READ_FRAMEBUFFER         = 0x8CA8;
    constexpr GLenum RENDERBUFFER             = 0x8D41;
}

Stereo::ConverterDLL::GlEyeTargets::GlEyeTargets()
    :m_context_ptr  (nullptr),
     m_has_functions(false),
     m_height       (0),
     m_is_ready     (false),
     m_width        (0)
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

    if (make_target(&m_left)  == false ||
        make_target(&m_right) == false)
    {
        release_targets();

        goto end;
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

bool Stereo::ConverterDLL::GlEyeTargets::make_target(EyeTarget* out_target_ptr)
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
                                              static_cast<GLsizei>(m_width),
                                              static_cast<GLsizei>(m_height) );
    m_functions.bind_renderbuffer_func_ptr   (RENDERBUFFER,
                                              out_target_ptr->depth_renderbuffer);
    m_functions.renderbuffer_storage_func_ptr(RENDERBUFFER,
                                              DEPTH24_STENCIL8,
                                              static_cast<GLsizei>(m_width),
                                              static_cast<GLsizei>(m_height) );
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

bool Stereo::ConverterDLL::GlEyeTargets::read_eye(const Analysis::Recipe::Eye& in_eye,
                                                  std::vector<uint8_t>*        out_rgba_u8_vec_ptr)
{
    constexpr GLenum PACK_ALIGNMENT = 0x0D05;

    const OwnCallScope own_call_scope;
    GLint              saved_alignment = 4;
    const bool         result          = m_is_ready;

    if (m_is_ready)
    {
        out_rgba_u8_vec_ptr->resize(static_cast<size_t>(m_width) * m_height * 4);

        GlDriverFunctions::get().get_integerv_func_ptr(PACK_ALIGNMENT,
                                                       &saved_alignment);
        GlDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                       1);

        bind_eye(in_eye);

        GlDriverFunctions::get().read_pixels_func_ptr(0,
                                                      0,
                                                      static_cast<GLsizei>(m_width),
                                                      static_cast<GLsizei>(m_height),
                                                      GL_RGBA,
                                                      GL_UNSIGNED_BYTE,
                                                      out_rgba_u8_vec_ptr->data() );

        GlDriverFunctions::get().pixel_storei_func_ptr(PACK_ALIGNMENT,
                                                       saved_alignment);

        bind_eye(Analysis::Recipe::Eye::LEFT);
    }

    return result;
}

void Stereo::ConverterDLL::GlEyeTargets::release_targets()
{
    for (EyeTarget* target_ptr : {&m_left, &m_right})
    {
        if (target_ptr->framebuffer != 0)
        {
            m_functions.delete_framebuffers_func_ptr (1,
                                                     &target_ptr->framebuffer);
        }

        if (target_ptr->color_renderbuffer != 0)
        {
            m_functions.delete_renderbuffers_func_ptr(1,
                                                     &target_ptr->color_renderbuffer);
        }

        if (target_ptr->depth_renderbuffer != 0)
        {
            m_functions.delete_renderbuffers_func_ptr(1,
                                                     &target_ptr->depth_renderbuffer);
        }

        *target_ptr = EyeTarget();
    }

    m_is_ready = false;
}
