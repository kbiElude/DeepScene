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
#include <cstring>


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
    constexpr GLenum  VIEWPORT            = 0x0BA2;

}

Stereo::ConverterDLL::GlEyeRenderer::GlEyeRenderer(Stereo::Core::StereoCore* in_core_ptr,
                                                   const bool&               in_is_shown_elsewhere,
                                                   const uint32_t&           in_target_width,
                                                   const uint32_t&           in_target_height)
    :m_bound_texture_array(),
     m_active_unit        (0),
     m_applied_height     (0),
     m_applied_width      (0),
     m_block_class        (Analysis::Recipe::DrawClass::UNAFFECTED),
     m_block_has_draws    (false),
     m_block_is_pushed    (false),
     m_block_list         (0),
     m_caches_pixel_reads (in_core_ptr->get_caches_pixel_reads() ),
     m_core_ptr           (in_core_ptr),
     m_frame_depth        (),
     m_frame_projection_scale(0.0f),
     m_has_frame_depth    (false),
     m_get_chunk_func     (),
     m_has_failed         (false),
     m_is_in_app_list     (false),
     m_is_in_block        (false),
     m_is_shown_elsewhere (in_is_shown_elsewhere),
     m_measures_depth     (in_core_ptr->get_measures_depth() ),
     m_n_failed_attempts  (0),
     m_pass_has_affected_draws(false),
     m_scaler             (in_target_width,
                           in_target_height,
                           in_core_ptr,
                           [this](const std::string& in_message)
                           {
                               m_note_vec.push_back(in_message);
                           })
{
    if (m_caches_pixel_reads)
    {
        m_note_vec.push_back("The recipe has the reads of one pixel that the application makes in a run given from a picture of the left eye that is read once (cache_pixel_reads)");
    }
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

void Stereo::ConverterDLL::GlEyeRenderer::measure_pass_depth()
{
    /* A depth buffer that is read whole (the copy of it could not be made) is looked at every few pixels; the copy is small enough to be looked at whole. */
    constexpr size_t N_DEPTHS_LOOKED_AT_WHOLE = 100000;
    constexpr size_t N_PIXELS_BETWEEN_SAMPLES = 4;

    uint32_t depth_height = 0;
    uint32_t depth_width  = 0;

    if (m_measures_depth           &&
        m_pass_has_affected_draws  &&
        m_targets.is_ready()       &&
        m_targets.read_depth(&m_depth_f32_vec,
                             &depth_width,
                             &depth_height) )
    {
        const auto range = Stereo::Core::DepthBufferStatistics::measure(m_depth_f32_vec.data(),
                                                                        m_depth_f32_vec.size(),
                                                                        (m_depth_f32_vec.size() > N_DEPTHS_LOOKED_AT_WHOLE) ? N_PIXELS_BETWEEN_SAMPLES
                                                                                                                            : 1,
                                                                        m_pass_projection_f32mat.at(10),
                                                                        m_pass_projection_f32mat.at(14),
                                                                        m_core_ptr->get_near_fraction() );

        if (range.has_depth)
        {
            if (m_has_frame_depth == false)
            {
                m_frame_depth     = range;
                m_has_frame_depth = true;
            }
            else
            {
                m_frame_depth.far_depth  = (std::max)(m_frame_depth.far_depth,
                                                    range.far_depth);
                m_frame_depth.near_depth = (std::min)(m_frame_depth.near_depth,
                                                    range.near_depth);
            }

            m_frame_median_vec.push_back(range.median_depth);

            m_frame_projection_scale = m_pass_projection_f32mat.at(0);
        }
    }

    m_pass_has_affected_draws = false;
}

bool Stereo::ConverterDLL::GlEyeRenderer::mirror_copy(const APIInterceptor::APIFunction&         in_function,
                                                      const uint32_t&                            in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t N_COPY_IMAGE_ARGUMENTS     = 8;
    constexpr uint32_t N_COPY_SUB_IMAGE_ARGUMENTS = 8;

    const bool     is_sub  = (in_function == APIInterceptor::APIFUNCTION_GL_GLCOPYTEXSUBIMAGE2D);
    const uint32_t texture = m_bound_texture_array.at(m_active_unit);

    bool                   is_made_scaled = false;
    std::array<int32_t, 8> value_array    = {};
    uint32_t               target         = 0;

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
            const bool         is_scaled     = (m_scaler.is_identity() == false);

            /* The framebuffer is larger than the application knows: what it copies of it is as many times larger. */
            if (is_scaled)
            {
                m_scaler.scale_copy(is_sub,
                                    texture,
                                   &value_array);
            }

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

            /* The copy of the left eye is the application's call with the numbers of the larger framebuffer, which it cannot make. */
            if (is_scaled)
            {
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

                is_made_scaled = true;
            }
        }
    }

    return is_made_scaled;
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

    /* A frame ends here: what was read of the picture is not the picture of the next one, whatever the calls in between were. */
    m_readback_cache.invalidate();

    if (m_targets.is_ready() == false)
    {
        goto end;
    }

    measure_pass_depth();

    {
        /* The images that come are those of the frame before, which have been read while this one was made. */
        result = m_frame_reader.read(&m_targets,
                                     out_images_ptr);

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
                                                  : Analysis::Recipe::DrawClass::UNAFFECTED,
                              (m_block_has_draws &&
                               m_block_is_pushed) );

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
                                                 const Analysis::Recipe::DrawClass&         in_draw_class,
                                                 const bool&                                in_is_pushed)
{
    bool result            = true;
    bool is_made_scaled    = false;
    bool is_read_served    = false;

    /* Anything that the application does but read a pixel may change the picture: what was read of it is not used again. Only if the recipe asks for the reads to be given from it. */
    if (m_caches_pixel_reads)
    {
        if (in_function != APIInterceptor::APIFUNCTION_GL_GLREADPIXELS)
        {
            m_readback_cache.invalidate();
        }
        else
        if (m_targets.is_ready() )
        {
            is_read_served = serve_pixel_read(in_n_arguments,
                                              in_arguments_ptr);
        }
    }

    /* What the application states in pixels is made to match the framebuffers, in a list that it compiles as well as outside one (the call made here is in the list). */
    if (m_targets.is_ready()            &&
        m_scaler.is_identity() == false &&
        is_read_served         == false)
    {
        is_made_scaled = m_scaler.on_pre(in_function,
                                         in_n_arguments,
                                         in_arguments_ptr,
                                         m_bound_texture_array.at(m_active_unit) );
    }

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
                                     in_draw_class,
                                     in_is_pushed);
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
                constexpr uint32_t DEPTH_BUFFER_BIT = 0x00000100;

                /* What the pass drew is in the depth buffer until it is cleared. */
                if (mask & DEPTH_BUFFER_BIT)
                {
                    measure_pass_depth();
                }

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
                if (mirror_copy(in_function,
                                in_n_arguments,
                                in_arguments_ptr) )
                {
                    is_made_scaled = true;
                }
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

                    m_scaler.forget_texture(name_ptr[n_name]);

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
            m_block_is_pushed = in_is_pushed;

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
    return (result                      &&
            is_made_scaled == false     &&
            is_read_served == false);
}

bool Stereo::ConverterDLL::GlEyeRenderer::on_pre_draw(const APIInterceptor::APIFunction&         in_function,
                                                      const uint32_t&                            in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                      const Analysis::Recipe::DrawClass&         in_draw_class,
                                                      const bool&                                in_is_pushed)
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
                      in_draw_class,
                      in_is_pushed);
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
                                                    const Analysis::Recipe::DrawClass& in_draw_class,
                                                    const bool&                        in_is_pushed)
{
    /* Where the numbers that are looked for are in a matrix (the columns one after the other) and in a viewport. */
    constexpr size_t MATRIX_PERSPECTIVE_W_FROM_Z = 11;
    constexpr size_t MATRIX_W_TRANSLATION        = 15;
    constexpr size_t MATRIX_X_SCALE              = 0;
    constexpr size_t MATRIX_Z_TRANSLATION        = 14;
    constexpr float  PERSPECTIVE_W_FROM_Z        = -1.0f; /* what a perspective projection has there: w is the distance in front of the camera */
    constexpr GLenum PROJECTION_MATRIX           = 0x0BA7;
    constexpr size_t VIEWPORT_HEIGHT             = 3;
    constexpr size_t VIEWPORT_WIDTH              = 2;
    constexpr size_t VIEWPORT_X                  = 0;
    constexpr size_t VIEWPORT_Y                  = 1;

    const OwnCallScope    own_call_scope;
    bool                  has_read_matrix    = false;
    bool                  is_affected        = (in_draw_class == Analysis::Recipe::DrawClass::AFFECTED);
    const bool            is_last_draw_part  = m_core_ptr->get_is_last_draw_part();
    bool                  is_part            = false;
    GLint                 matrix_mode        = MODELVIEW;
    std::array<float, 16> part_matrix;
    std::array<float, 16> part_projection;
    std::array<float, 16> saved_matrix;
    std::array<GLint, 4>  saved_viewport     = {};

    /* The viewport is the one the draw is made with: a pushed draw is moved by moving its origin, which works for any projection. */
    if (in_is_pushed)
    {
        GlDriverFunctions::get().get_integerv_func_ptr(VIEWPORT,
                                                       saved_viewport.data() );
    }

    /* A part that protrudes from an object, drawn once for both eyes, goes where the eyes would have put its origin: the camera it is made with is read here. */
    if (in_is_pushed &&
        is_last_draw_part)
    {
        GlDriverFunctions::get().get_floatv_func_ptr(GL_MODELVIEW_MATRIX,
                                                     part_matrix.data() );
        GlDriverFunctions::get().get_floatv_func_ptr(PROJECTION_MATRIX,
                                                     part_projection.data() );

        is_part = true;
    }

    if (is_affected                         &&
        m_measures_depth                    &&
        m_pass_has_affected_draws == false)
    {
        GlDriverFunctions::get().get_floatv_func_ptr(PROJECTION_MATRIX,
                                                     m_pass_projection_f32mat.data() );

        /* A projection that is not a perspective one (the last element is 0 in it, and the one before it is -1) says nothing of depth. */
        m_pass_has_affected_draws = (m_pass_projection_f32mat.at(MATRIX_W_TRANSLATION)        == 0.0f &&
                                     m_pass_projection_f32mat.at(MATRIX_PERSPECTIVE_W_FROM_Z) == PERSPECTIVE_W_FROM_Z);
    }

    if (is_affected)
    {
        GlDriverFunctions::get().get_integerv_func_ptr(GL_MATRIX_MODE,
                                                       &matrix_mode);

        has_read_matrix = true;

        if (matrix_mode != MODELVIEW)
        {
            GlDriverFunctions::get().matrix_mode_func_ptr(MODELVIEW);
        }

        GlDriverFunctions::get().get_floatv_func_ptr(GL_MODELVIEW_MATRIX,
                                                     saved_matrix.data() );

        /* A draw that is attached to the camera is made as it is: its origin is in front of the camera, and nearer than the part of the distance of convergence that the recipe says. */
        const float attached_depth = m_core_ptr->get_camera_attached_depth() * m_core_ptr->get_effective_convergence_distance();
        const float origin_depth   = -saved_matrix.at(MATRIX_Z_TRANSLATION);

        if (attached_depth > 0.0f         &&
            origin_depth   < attached_depth)
        {
            is_affected = false;

            m_statistics.n_camera_attached_draws++;
        }
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

        if (in_is_pushed)
        {
            const float picture_width_pixels = static_cast<float>(saved_viewport.at(VIEWPORT_WIDTH) );
            float       shift_pixels         = m_core_ptr->plan_push_shift(eye,
                                                                           picture_width_pixels);

            if (is_part)
            {
                /* A projection with a -1 in its fourth row, third column is a perspective one. */
                shift_pixels = m_core_ptr->plan_part_shift(eye,
                                                           picture_width_pixels,
                                                           part_matrix.at(MATRIX_Z_TRANSLATION),
                                                           part_projection.at(MATRIX_X_SCALE),
                                                           (part_projection.at(MATRIX_PERSPECTIVE_W_FROM_Z) == PERSPECTIVE_W_FROM_Z) );
            }

            /* Toward zero, so that the shift never goes past what the planner allowed. */
            GlDriverFunctions::get().viewport_func_ptr(saved_viewport.at(VIEWPORT_X) + static_cast<GLint>(shift_pixels),
                                                       saved_viewport.at(VIEWPORT_Y),
                                                       saved_viewport.at(VIEWPORT_WIDTH),
                                                       saved_viewport.at(VIEWPORT_HEIGHT) );
        }

        in_draw_func();
    }

    if (in_is_pushed)
    {
        GlDriverFunctions::get().viewport_func_ptr(saved_viewport.at(VIEWPORT_X),
                                                   saved_viewport.at(VIEWPORT_Y),
                                                   saved_viewport.at(VIEWPORT_WIDTH),
                                                   saved_viewport.at(VIEWPORT_HEIGHT) );
    }

    select_textures(false);

    if (is_affected)
    {
        GlDriverFunctions::get().load_matrixf_func_ptr(saved_matrix.data() );
    }

    if (has_read_matrix                &&
        matrix_mode != MODELVIEW)
    {
        GlDriverFunctions::get().matrix_mode_func_ptr(matrix_mode);
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

bool Stereo::ConverterDLL::GlEyeRenderer::serve_pixel_read(const uint32_t&                            in_n_arguments,
                                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t N_PIXEL_BYTES = 3; /* R, G and B of a pixel that the application reads as GL_RGB */

    SinglePixelRead read;
    const bool      is_read        = GlResolutionScaler::parse_single_pixel_read(in_n_arguments,
                                                                                 in_arguments_ptr,
                                                                                &read);
    const uint32_t  picture_height = m_scaler.get_asked_height();
    const uint32_t  picture_width  = m_scaler.get_asked_width ();
    bool            result         = false;

    if (is_read                 &&
        picture_height != 0     &&
        picture_width  != 0)
    {
        std::array<uint8_t, 4> rgba = {};

        /* The picture is the one that the application sees, brought down to the size of its window by the graphics card. */
        const auto fill_func = [this, picture_height, picture_width](std::vector<uint8_t>* out_rgba_u8_vec_ptr)
        {
            return m_targets.read_picture(picture_width,
                                          picture_height,
                                          out_rgba_u8_vec_ptr);
        };

        const bool is_served = m_readback_cache.read_pixel(fill_func,
                                                           picture_width,
                                                           picture_height,
                                                           read.x,
                                                           read.y,
                                                          &rgba);

        if (is_served)
        {
            ::memcpy(read.pixels_ptr,
                     rgba.data(),
                     N_PIXEL_BYTES);

            result = true;
        }
    }

    return result;
}

void Stereo::ConverterDLL::GlEyeRenderer::set_get_chunk_func(const GetChunkFunc& in_get_chunk_func)
{
    m_get_chunk_func = in_get_chunk_func;
}

bool Stereo::ConverterDLL::GlEyeRenderer::take_frame_depth(Stereo::Core::DepthRange* out_depth_range_ptr,
                                                           float*                    out_projection_scale_ptr)
{
    const bool result = m_has_frame_depth;

    if (result)
    {
        *out_depth_range_ptr = m_frame_depth;

        /* Of the passes, the one in the middle. */
        std::sort(m_frame_median_vec.begin(),
                  m_frame_median_vec.end  () );

        out_depth_range_ptr->median_depth = m_frame_median_vec.at(m_frame_median_vec.size() / 2);
        *out_projection_scale_ptr         = m_frame_projection_scale;
    }

    m_frame_median_vec.clear();

    m_has_frame_depth = false;

    return result;
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

        const bool     was_ready     = m_targets.is_ready();
        const uint32_t asked_width   = static_cast<uint32_t>(client.right  - client.left);
        const uint32_t asked_height  = static_cast<uint32_t>(client.bottom - client.top);
        uint32_t       real_height   = asked_height;
        uint32_t       real_width    = asked_width;

        /* The framebuffers are as large as the eye of the headset has room for, with the proportions of the window. */
        if (asked_width  > 0 &&
            asked_height > 0)
        {
            constexpr GLenum MAX_RENDERBUFFER_SIZE = 0x84E8;

            GLint max_extent = 0;

            GlDriverFunctions::get().get_integerv_func_ptr(MAX_RENDERBUFFER_SIZE,
                                                           &max_extent);

            m_scaler.plan(asked_width,
                          asked_height,
                          static_cast<uint32_t>(max_extent),
                         &real_width,
                         &real_height);
        }

        const bool is_ready = m_targets.ensure(context,
                                               real_width,
                                               real_height);

        /* Until the application says otherwise, the viewport and the scissor box are those of its window, which are the size of the framebuffers. */
        if (is_ready                           &&
            m_scaler.is_identity() == false    &&
            (real_width  != m_applied_width    ||
             real_height != m_applied_height) )
        {
            GlDriverFunctions::get().viewport_func_ptr(0,
                                                       0,
                                                       static_cast<GLsizei>(real_width),
                                                       static_cast<GLsizei>(real_height) );
            GlDriverFunctions::get().scissor_func_ptr (0,
                                                       0,
                                                       static_cast<GLsizei>(real_width),
                                                       static_cast<GLsizei>(real_height) );

            m_applied_height = real_height;
            m_applied_width  = real_width;
        }

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
