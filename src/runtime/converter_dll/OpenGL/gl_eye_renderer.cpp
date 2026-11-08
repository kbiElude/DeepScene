/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "common/argument_reader.h"
#include "common/callbacks.h"
#include "gl_eye_renderer.h"
#include "gl_driver_functions.h"
#include "own_call_scope.h"
#include <algorithm>


namespace
{
    constexpr GLenum  COMPILE_AND_EXECUTE = 0x1301;
    constexpr GLenum  MODELVIEW           = 0x1700;
    constexpr GLenum  TEXTURE0            = 0x84C0;
    constexpr GLenum  TEXTURE_MAG_FILTER  = 0x2800;
    constexpr GLenum  TEXTURE_MIN_FILTER  = 0x2801;
    constexpr GLenum  TEXTURE_WRAP_S      = 0x2802;
    constexpr GLenum  TEXTURE_WRAP_T      = 0x2803;
    constexpr GLenum  TEXTURE_HEIGHT      = 0x1001;
    constexpr GLenum  TEXTURE_INTERNAL_FORMAT = 0x1003;
    constexpr GLenum  TEXTURE_WIDTH       = 0x1000;

}

Stereo::ConverterDLL::GlEyeRenderer::GlEyeRenderer(Stereo::Core::StereoCore* in_core_ptr,
                                                   const bool&               in_is_shown_elsewhere)
    :m_bound_texture_array(),
     m_active_unit        (0),
     m_block_class        (Analysis::Recipe::DrawClass::UNAFFECTED),
     m_block_has_draws    (false),
     m_block_list         (0),
     m_core_ptr           (in_core_ptr),
     m_get_chunk_func     (),
     m_has_failed         (false),
     m_is_in_app_list     (false),
     m_is_in_block        (false),
     m_is_shown_elsewhere (in_is_shown_elsewhere),
     m_n_failed_attempts  (0)
{
    /* Stub */
}

const Stereo::ConverterDLL::GlEyeRenderer::Statistics& Stereo::ConverterDLL::GlEyeRenderer::get_statistics() const
{
    return m_statistics;
}

bool Stereo::ConverterDLL::GlEyeRenderer::is_ready() const
{
    return m_targets.is_ready();
}

bool Stereo::ConverterDLL::GlEyeRenderer::make_twin(const uint32_t& in_texture,
                                                    const bool&     in_has_level_size,
                                                    const int32_t&  in_level)
{
    const OwnCallScope own_call_scope;
    GLuint             twin    = 0;
    bool               result  = false;
    GLint              height  = 0;
    GLint              format  = 0;
    GLint              width   = 0;
    std::array<GLint, 4> parameter_array = {};

    if (in_has_level_size)
    {
        GlDriverFunctions::get().get_tex_level_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                                    in_level,
                                                                    TEXTURE_WIDTH,
                                                                    &width);
        GlDriverFunctions::get().get_tex_level_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                                    in_level,
                                                                    TEXTURE_HEIGHT,
                                                                    &height);
        GlDriverFunctions::get().get_tex_level_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                                    in_level,
                                                                    TEXTURE_INTERNAL_FORMAT,
                                                                    &format);

        if (width  <= 0 ||
            height <= 0)
        {
            goto end;
        }
    }

    GlDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_MIN_FILTER,
                                                          &parameter_array.at(0) );
    GlDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_MAG_FILTER,
                                                          &parameter_array.at(1) );
    GlDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_WRAP_S,
                                                          &parameter_array.at(2) );
    GlDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_WRAP_T,
                                                          &parameter_array.at(3) );

    GlDriverFunctions::get().gen_textures_func_ptr(1,
                                                   &twin);
    GlDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                   twin);

    GlDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_MIN_FILTER,
                                                     parameter_array.at(0) );
    GlDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_MAG_FILTER,
                                                     parameter_array.at(1) );
    GlDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_WRAP_S,
                                                     parameter_array.at(2) );
    GlDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_WRAP_T,
                                                     parameter_array.at(3) );

    if (in_has_level_size)
    {
        GlDriverFunctions::get().tex_image_2d_func_ptr(GL_TEXTURE_2D,
                                                       in_level,
                                                       format,
                                                       width,
                                                       height,
                                                       0, /* border */
                                                       GL_RGBA,
                                                       GL_UNSIGNED_BYTE,
                                                       nullptr);
    }

    GlDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                   in_texture);

    m_twin_map[in_texture] = twin;
    m_statistics.n_twin_textures++;

    result = true;

end:
    return result;
}

void Stereo::ConverterDLL::GlEyeRenderer::mirror_copy(const APIInterceptor::APIFunction&         in_function,
                                                      const uint32_t&                            in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t N_COPY_IMAGE_ARGUMENTS     = 8;
    constexpr uint32_t N_COPY_SUB_IMAGE_ARGUMENTS = 8;

    const bool     is_sub  = (in_function == APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D);
    const uint32_t texture = m_bound_texture_array.at(m_active_unit);

    std::array<int32_t, 8> value_array = {};
    uint32_t               target      = 0;

    if (texture != 0                                                              &&
        in_n_arguments == ( (is_sub) ? N_COPY_SUB_IMAGE_ARGUMENTS : N_COPY_IMAGE_ARGUMENTS) &&
        APIInterceptor::read_number(in_arguments_ptr[0],
                                   &target) )
    {
        bool is_read = true;

        for (uint32_t n_argument = 1;
                      n_argument < in_n_arguments;
                    ++n_argument)
        {
            is_read &= APIInterceptor::read_number(in_arguments_ptr[n_argument],
                                                  &value_array.at(n_argument) );
        }

        if (is_read && target == GL_TEXTURE_2D)
        {
            const OwnCallScope own_call_scope;
            const auto         twin_iterator = m_twin_map.find(texture);
            bool               has_twin      = (twin_iterator != m_twin_map.end() );

            if (has_twin == false)
            {
                has_twin = make_twin(texture,
                                     is_sub,
                                     value_array.at(1) );
            }

            if (has_twin)
            {
                const uint32_t twin = m_twin_map.at(texture);

                m_targets.bind_eye(Analysis::Recipe::Eye::RIGHT);

                GlDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                               twin);

                if (is_sub)
                {
                    GlDriverFunctions::get().copy_tex_sub_image_2d_func_ptr(target,
                                                                            value_array.at(1),
                                                                            value_array.at(2),
                                                                            value_array.at(3),
                                                                            value_array.at(4),
                                                                            value_array.at(5),
                                                                            value_array.at(6),
                                                                            value_array.at(7) );
                }
                else
                {
                    GlDriverFunctions::get().copy_tex_image_2d_func_ptr(target,
                                                                        value_array.at(1),
                                                                        value_array.at(2),
                                                                        value_array.at(3),
                                                                        value_array.at(4),
                                                                        value_array.at(5),
                                                                        value_array.at(6),
                                                                        value_array.at(7) );
                }

                GlDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                               texture);

                m_targets.bind_eye(Analysis::Recipe::Eye::LEFT);

                m_statistics.n_texture_copies++;
            }
            else
            {
                m_statistics.n_unmirrored_calls++;

                note_once("copy_no_twin",
                          "A copy of the framebuffer to a texture was not made for the right eye: the twin of the texture cannot be made");
            }
        }
    }
}

void Stereo::ConverterDLL::GlEyeRenderer::mirror_texture_parameter(const APIInterceptor::APIFunction&         in_function,
                                                                   const uint32_t&                            in_n_arguments,
                                                                   const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t N_ARGUMENTS = 3;

    const uint32_t texture = m_bound_texture_array.at(m_active_unit);
    const auto     twin_it = m_twin_map.find(texture);

    uint32_t target = 0;
    uint32_t name   = 0;
    float    value  = 0.0f;

    if (twin_it     != m_twin_map.end() &&
        in_n_arguments == N_ARGUMENTS   &&
        APIInterceptor::read_number(in_arguments_ptr[0],
                                   &target)                       &&
        APIInterceptor::read_number(in_arguments_ptr[1],
                                   &name)                         &&
        APIInterceptor::read_number(in_arguments_ptr[2],
                                   &value)                        &&
        target == GL_TEXTURE_2D)
    {
        const OwnCallScope own_call_scope;

        GlDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                       twin_it->second);

        if (in_function == APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI)
        {
            GlDriverFunctions::get().tex_parameteri_func_ptr(target,
                                                             name,
                                                             static_cast<GLint>(value) );
        }
        else
        {
            GlDriverFunctions::get().tex_parameterf_func_ptr(target,
                                                             name,
                                                             value);
        }

        GlDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                       texture);
    }
}

void Stereo::ConverterDLL::GlEyeRenderer::note_once(const std::string& in_key,
                                                    const std::string& in_note)
{
    if (std::find(m_noted_key_vec.begin(),
                  m_noted_key_vec.end  (),
                  in_key) == m_noted_key_vec.end() )
    {
        m_noted_key_vec.push_back(in_key);
        m_note_vec.push_back     (in_note);
    }
}

bool Stereo::ConverterDLL::GlEyeRenderer::on_frame_ending(GlFrameImages* out_images_ptr)
{
    bool result = false;

    if (m_targets.is_ready() == false)
    {
        goto end;
    }

    {
        const bool is_left_read  = m_targets.read_eye(Analysis::Recipe::Eye::LEFT,
                                                     &m_left_rgba_u8_vec);
        const bool is_right_read = m_targets.read_eye(Analysis::Recipe::Eye::RIGHT,
                                                     &m_right_rgba_u8_vec);

        if (is_left_read && is_right_read)
        {
            out_images_ptr->height         = m_targets.get_height();
            out_images_ptr->left_rgba_ptr  = m_left_rgba_u8_vec.data ();
            out_images_ptr->right_rgba_ptr = m_right_rgba_u8_vec.data();
            out_images_ptr->width          = m_targets.get_width ();

            result = true;
        }

        /* A program that shows the images elsewhere has no use for the window; otherwise the window shows the left eye. */
        if (m_is_shown_elsewhere == false)
        {
            m_targets.blit_left_to_window();
        }

        m_invisible_windows.keep_invisible();
    }

end:
    return result;
}

void Stereo::ConverterDLL::GlEyeRenderer::on_post(const APIInterceptor::APIFunction&         in_function,
                                                  const uint32_t&                            in_n_arguments_out,
                                                  const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr)
{
    (void) in_n_arguments_out;
    (void) in_arguments_out_ptr;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS:
        {
            /* The window may have another size now: the framebuffers are made again, for the frame that begins. */
            try_to_prepare();

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLEND:
        {
            if (m_is_in_block)
            {
                const OwnCallScope own_call_scope;

                GlDriverFunctions::get().end_list_func_ptr();

                m_is_in_block = false;

                if (m_block_has_draws == false ||
                    m_block_class     != Analysis::Recipe::DrawClass::SKIPPED)
                {
                    const GLuint list = m_block_list;

                    const auto call_list_func = [list]()
                    {
                        GlDriverFunctions::get().call_list_func_ptr(list);
                    };

                    play_eyes(call_list_func,
                              (m_block_has_draws) ? m_block_class
                                                  : Analysis::Recipe::DrawClass::UNAFFECTED);

                    m_statistics.n_block_draws++;
                }
                else
                {
                    m_statistics.n_skipped_draws++;
                }

                m_block_has_draws = false;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_WGL_WGLMAKECURRENT:
        {
            try_to_prepare();

            break;
        }

        default:
        {
            break;
        }
    }
}

bool Stereo::ConverterDLL::GlEyeRenderer::on_pre(const APIInterceptor::APIFunction&         in_function,
                                                 const uint32_t&                            in_n_arguments,
                                                 const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                 const bool&                                in_has_draws,
                                                 const Analysis::Recipe::DrawClass&         in_draw_class)
{
    bool result = true;

    /* A list that the application compiles makes nothing now: what is in it is made when it is called. */
    if (m_is_in_app_list)
    {
        if (in_function == APIInterceptor::APIFUNCTION_GL_GLENDLIST)
        {
            m_is_in_app_list = false;
        }

        goto end;
    }

    if (m_targets.is_ready() == false)
    {
        if (in_function == APIInterceptor::APIFUNCTION_GL_GLBEGIN ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLCLEAR)
        {
            try_to_prepare();
        }
    }

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLACCUM:
        case APIInterceptor::APIFUNCTION_GL_GLBITMAP:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYPIXELS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWPIXELS:
        {
            if (m_targets.is_ready() )
            {
                m_statistics.n_unmirrored_calls++;

                note_once("pixels",
                          "A call that draws pixels (glAccum, glBitmap, glCopyPixels, glDrawPixels) was made for the left eye only");
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLACTIVETEXTURE:
        {
            uint32_t texture = 0;

            if (in_n_arguments > 0 &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &texture) )
            {
                m_active_unit = std::min<uint32_t>(texture - TEXTURE0,
                                                   static_cast<uint32_t>(N_TEXTURE_UNITS - 1) );
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLBEGIN:
        {
            /* The block is compiled, not made: it is made for each eye when it ends, when it is known what the recipe says of it. */
            if (m_targets.is_ready() )
            {
                const OwnCallScope own_call_scope;

                if (m_block_list == 0)
                {
                    m_block_list = GlDriverFunctions::get().gen_lists_func_ptr(1);
                }

                GlDriverFunctions::get().new_list_func_ptr(m_block_list,
                                                           GL_COMPILE);

                m_is_in_block = true;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE:
        {
            uint32_t name   = 0;
            uint32_t target = 0;

            if (in_n_arguments > 1                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &target)                      &&
                APIInterceptor::read_number(in_arguments_ptr[1],
                                           &name)                        &&
                target == GL_TEXTURE_2D)
            {
                m_bound_texture_array.at(m_active_unit) = name;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS:
        case APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS:
        {
            if (in_has_draws          &&
                m_targets.is_ready() )
            {
                result = on_pre_draw(in_function,
                                     in_n_arguments,
                                     in_arguments_ptr,
                                     in_draw_class);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCLEAR:
        {
            uint32_t mask = 0;

            if (m_targets.is_ready()                                     &&
                in_n_arguments > 0                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &mask) )
            {
                const OwnCallScope own_call_scope;

                m_targets.bind_eye(Analysis::Recipe::Eye::RIGHT);

                GlDriverFunctions::get().clear_func_ptr(mask);

                m_targets.bind_eye(Analysis::Recipe::Eye::LEFT);

                m_statistics.n_clears++;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D:
        {
            if (m_targets.is_ready() )
            {
                mirror_copy(in_function,
                            in_n_arguments,
                            in_arguments_ptr);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDELETETEXTURES:
        {
            const void* names_ptr = nullptr;
            int32_t     count     = 0;

            if (in_n_arguments > 1                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &count)                       &&
                count > 0)
            {
                names_ptr = resolve_pointer(in_arguments_ptr[1]);
            }

            if (names_ptr != nullptr)
            {
                const OwnCallScope own_call_scope;
                const uint32_t*    name_ptr = static_cast<const uint32_t*>(names_ptr);

                for (int32_t n_name = 0;
                             n_name < count;
                           ++n_name)
                {
                    const auto twin_it = m_twin_map.find(name_ptr[n_name]);

                    if (twin_it != m_twin_map.end() )
                    {
                        const GLuint twin = twin_it->second;

                        GlDriverFunctions::get().delete_textures_func_ptr(1,
                                                                          &twin);

                        m_twin_map.erase(twin_it);
                    }

                    for (auto& bound_texture : m_bound_texture_array)
                    {
                        if (bound_texture == name_ptr[n_name])
                        {
                            bound_texture = 0;
                        }
                    }
                }
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLEND:
        {
            m_block_class     = in_draw_class;
            m_block_has_draws = in_has_draws;

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLNEWLIST:
        {
            uint32_t mode = 0;

            m_is_in_app_list = true;

            if (in_n_arguments > 1                                       &&
                APIInterceptor::read_number(in_arguments_ptr[1],
                                           &mode)                        &&
                mode == COMPILE_AND_EXECUTE)
            {
                note_once("compile_and_execute",
                          "A display list that is compiled and made at once is made for the left eye only");
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE2D:
        {
            if (m_twin_map.find(m_bound_texture_array.at(m_active_unit) ) != m_twin_map.end() )
            {
                m_statistics.n_unmirrored_calls++;

                note_once("texture_upload",
                          "An upload of pixels to a texture that has a twin for the right eye was made to the texture only");
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERF:
        case APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI:
        {
            if (m_targets.is_ready() )
            {
                mirror_texture_parameter(in_function,
                                         in_n_arguments,
                                         in_arguments_ptr);
            }

            break;
        }

        default:
        {
            break;
        }
    }

end:
    return result;
}

bool Stereo::ConverterDLL::GlEyeRenderer::on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                                                      const uint32_t&                            in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                      const Analysis::Recipe::DrawClass&         in_draw_class)
{
    std::function<void()> draw_func;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLCALLLIST:
        {
            uint32_t list = 0;

            if (in_n_arguments > 0                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &list) )
            {
                draw_func = [list]()
                {
                    GlDriverFunctions::get().call_list_func_ptr(list);
                };

                m_statistics.n_list_draws++;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS:
        {
            int32_t  count = 0;
            int32_t  first = 0;
            uint32_t mode  = 0;

            if (in_n_arguments > 2                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &mode)                        &&
                APIInterceptor::read_number(in_arguments_ptr[1],
                                           &first)                       &&
                APIInterceptor::read_number(in_arguments_ptr[2],
                                           &count) )
            {
                draw_func = [count, first, mode]()
                {
                    GlDriverFunctions::get().draw_arrays_func_ptr(mode,
                                                                  first,
                                                                  count);
                };

                m_statistics.n_array_draws++;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS:
        {
            int32_t  count = 0;
            uint32_t mode  = 0;
            uint32_t type  = 0;

            if (in_n_arguments > 3                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &mode)                        &&
                APIInterceptor::read_number(in_arguments_ptr[1],
                                           &count)                       &&
                APIInterceptor::read_number(in_arguments_ptr[2],
                                           &type) )
            {
                /* The indices are in the memory of the application (a chunk of them), or the offset in a buffer, which is the pointer itself. */
                const void* const indices_ptr = (in_arguments_ptr[3].type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID) ? resolve_pointer(in_arguments_ptr[3])
                                                                                                                    : in_arguments_ptr[3].get_ptr();

                draw_func = [count, indices_ptr, mode, type]()
                {
                    GlDriverFunctions::get().draw_elements_func_ptr(mode,
                                                                    count,
                                                                    type,
                                                                    indices_ptr);
                };

                m_statistics.n_array_draws++;
            }

            break;
        }

        default:
        {
            break;
        }
    }

    if (draw_func)
    {
        if (in_draw_class != Analysis::Recipe::DrawClass::SKIPPED)
        {
            play_eyes(draw_func,
                      in_draw_class);
        }
        else
        {
            m_statistics.n_skipped_draws++;
        }
    }

    /* Made here, for both eyes; what could not be read is left to the application. */
    return (draw_func == nullptr);
}

void Stereo::ConverterDLL::GlEyeRenderer::play_eyes(const std::function<void()>&       in_draw_func,
                                                    const Analysis::Recipe::DrawClass& in_draw_class)
{
    const OwnCallScope    own_call_scope;
    const bool            is_affected = (in_draw_class == Analysis::Recipe::DrawClass::AFFECTED);
    GLint                 matrix_mode = MODELVIEW;
    std::array<float, 16> saved_matrix;

    if (is_affected)
    {
        GlDriverFunctions::get().get_integerv_func_ptr(GL_MATRIX_MODE,
                                                       &matrix_mode);

        if (matrix_mode != MODELVIEW)
        {
            GlDriverFunctions::get().matrix_mode_func_ptr(MODELVIEW);
        }

        GlDriverFunctions::get().get_floatv_func_ptr(GL_MODELVIEW_MATRIX,
                                                     saved_matrix.data() );
    }

    for (const auto eye : {Analysis::Recipe::Eye::RIGHT, Analysis::Recipe::Eye::LEFT})
    {
        m_targets.bind_eye(eye);

        if (is_affected)
        {
            const auto eye_matrix = m_core_ptr->make_eye_shift_matrix(eye);

            GlDriverFunctions::get().load_matrixf_func_ptr(eye_matrix.data() );
            GlDriverFunctions::get().mult_matrixf_func_ptr(saved_matrix.data() );
        }

        select_textures(eye == Analysis::Recipe::Eye::RIGHT);

        in_draw_func();
    }

    select_textures(false);

    if (is_affected)
    {
        GlDriverFunctions::get().load_matrixf_func_ptr(saved_matrix.data() );

        if (matrix_mode != MODELVIEW)
        {
            GlDriverFunctions::get().matrix_mode_func_ptr(matrix_mode);
        }
    }
}

const void* Stereo::ConverterDLL::GlEyeRenderer::resolve_pointer(const APIInterceptor::APIFunctionArgument& in_argument) const
{
    const void* result = nullptr;

    if (in_argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        uint32_t n_bytes = 0;

        /* The chunk is the converter's by now (the feeder took it out of the interceptor when it analysed the call). */
        if (m_get_chunk_func                                          == nullptr ||
            m_get_chunk_func(in_argument.get_data_chunk_id(),
                            &result,
                            &n_bytes) == false)
        {
            result = nullptr;
        }
    }
    else
    if (in_argument.type == APIInterceptor::ARGTYPE_VOID_PTR)
    {
        result = in_argument.get_ptr();
    }

    return result;
}

void Stereo::ConverterDLL::GlEyeRenderer::set_get_chunk_func(const GetChunkFunc& in_get_chunk_func)
{
    m_get_chunk_func = in_get_chunk_func;
}

void Stereo::ConverterDLL::GlEyeRenderer::select_textures(const bool& in_are_twins)
{
    if (m_twin_map.empty() == false)
    {
        const OwnCallScope own_call_scope;

        for (uint32_t n_unit = 0;
                      n_unit < N_TEXTURE_UNITS;
                    ++n_unit)
        {
            const uint32_t texture = m_bound_texture_array.at(n_unit);
            const auto     twin_it = m_twin_map.find(texture);

            if (texture == 0                  ||
                twin_it == m_twin_map.end() )
            {
                continue;
            }

            /* Only the unit that is active can be bound without the function that changes it. */
            if (n_unit == m_active_unit)
            {
                GlDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                               (in_are_twins) ? twin_it->second
                                                               : texture);
            }
            else
            {
                note_once("units",
                          "A texture that has a twin is bound to a unit other than the one that is active: it is not bound to its twin for the right eye");
            }
        }
    }
}

std::vector<std::string> Stereo::ConverterDLL::GlEyeRenderer::take_notes()
{
    std::vector<std::string> result;

    result.swap(m_note_vec);

    return result;
}

void Stereo::ConverterDLL::GlEyeRenderer::try_to_prepare()
{
    const OwnCallScope own_call_scope;
    constexpr uint32_t MAX_FAILED_ATTEMPTS = 20;

    const HGLRC        context = ::wglGetCurrentContext();
    const HDC          device  = ::wglGetCurrentDC();

    if (m_has_failed == false &&
        context      != nullptr &&
        device       != nullptr)
    {
        const HWND window = ::WindowFromDC(device);
        RECT       client = {};

        if (window != nullptr)
        {
            ::GetClientRect(window,
                           &client);
        }

        const bool was_ready = m_targets.is_ready();
        const bool is_ready  = m_targets.ensure(context,
                                                static_cast<uint32_t>(client.right  - client.left),
                                                static_cast<uint32_t>(client.bottom - client.top) );

        if (is_ready && was_ready == false)
        {
            note_once("ready",
                      "The framebuffers of the eyes are made, " + std::to_string(m_targets.get_width() ) + " x " + std::to_string(m_targets.get_height() ) );

            if (m_is_shown_elsewhere)
            {
                m_invisible_windows.add(window);
            }
        }
        else
        if (is_ready == false && was_ready == false)
        {
            m_n_failed_attempts++;
            m_has_failed = (m_n_failed_attempts >= MAX_FAILED_ATTEMPTS);

            note_once("failed",
                      "The framebuffers of the eyes cannot be made (a context of OpenGL without them, or a window with no size): the application is not converted");
        }
    }
}
