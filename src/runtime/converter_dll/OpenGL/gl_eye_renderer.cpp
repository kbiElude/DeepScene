/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "common/argument_reader.h"
#include "common/callbacks.h"
#include "gl_eye_renderer.h"
#include "gl_driver_functions.h"
#include "analysis/api_dump_analysis/draw_measurer.h"
#include "own_call_scope.h"
#include "stereo/core/push_shift_planner.h"
#include <algorithm>
#include <cstring>

Stereo::ConverterDLL::GLEyeRenderer::GLEyeRenderer(Stereo::Core::RecipeStereoPlanner* in_core_ptr,
                                                   const bool&                        in_is_shown_elsewhere,
                                                   const uint32_t&                    in_target_width,
                                                   const uint32_t&                    in_target_height)
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
     m_depth_generation   (0),
     m_depth_lag          (0),
     m_fast_depth_range_measurer     (true),
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

void Stereo::ConverterDLL::GLEyeRenderer::collect_depth_reads(const bool& in_only_older_frames,
                                                              const bool& in_only_the_oldest)
{
    Stereo::Core::PendingDepthRead read;

    bool is_popped = m_depth_ledger.pop_pending(in_only_older_frames,
                                               &read);

    while (is_popped)
    {
        /* The depth is measured in place, in the buffer of the graphics card. The pass it belongs to is identified by the z scale and shift saved when the read started. */
        const auto depth_func = [this, &read](const float* in_depth_ptr, const size_t& in_n_depths)
        {
            const auto range = measure_depths(read.z_scale,
                                              read.z_shift,
                                              in_depth_ptr,
                                              in_n_depths);

            if (range.has_depth)
            {
                m_depth_ledger.add_result(read,
                                          range);
            }
        };

        m_targets.finish_depth_read(read.slot,
                                    depth_func);

        if (in_only_the_oldest)
        {
            is_popped = false;
        }
        else
        {
            is_popped = m_depth_ledger.pop_pending(in_only_older_frames,
                                                  &read);
        }
    }
}

void Stereo::ConverterDLL::GLEyeRenderer::forget_lost_depth_reads()
{
    const uint32_t generation = m_targets.get_generation();

    if (generation != m_depth_generation)
    {
        m_depth_ledger.forget_pending();

        m_depth_generation = generation;
    }
}

uint32_t Stereo::ConverterDLL::GLEyeRenderer::get_depth_lag() const
{
    return m_depth_lag;
}

const Stereo::ConverterDLL::GLEyeRenderer::Statistics& Stereo::ConverterDLL::GLEyeRenderer::get_statistics() const
{
    return m_statistics;
}

bool Stereo::ConverterDLL::GLEyeRenderer::is_ready() const
{
    return m_targets.is_ready();
}

bool Stereo::ConverterDLL::GLEyeRenderer::make_twin(const uint32_t& in_texture,
                                                    const bool&     in_has_level_size,
                                                    const int32_t&  in_level)
{
    constexpr GLenum TEXTURE_HEIGHT          = 0x1001;
    constexpr GLenum TEXTURE_INTERNAL_FORMAT = 0x1003;
    constexpr GLenum TEXTURE_MAG_FILTER      = 0x2800;
    constexpr GLenum TEXTURE_MIN_FILTER      = 0x2801;
    constexpr GLenum TEXTURE_WIDTH           = 0x1000;
    constexpr GLenum TEXTURE_WRAP_S          = 0x2802;
    constexpr GLenum TEXTURE_WRAP_T          = 0x2803;

    const OwnCallScope own_call_scope;
    GLuint             twin    = 0;
    bool               result  = false;
    GLint              height  = 0;
    GLint              format  = 0;
    GLint              width   = 0;
    std::array<GLint, 4> parameter_array = {};

    if (in_has_level_size)
    {
        GLDriverFunctions::get().get_tex_level_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                                    in_level,
                                                                    TEXTURE_WIDTH,
                                                                    &width);
        GLDriverFunctions::get().get_tex_level_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                                    in_level,
                                                                    TEXTURE_HEIGHT,
                                                                    &height);
        GLDriverFunctions::get().get_tex_level_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                                    in_level,
                                                                    TEXTURE_INTERNAL_FORMAT,
                                                                    &format);

        if (width  <= 0 ||
            height <= 0)
        {
            goto end;
        }
    }

    GLDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_MIN_FILTER,
                                                          &parameter_array.at(0) );
    GLDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_MAG_FILTER,
                                                          &parameter_array.at(1) );
    GLDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_WRAP_S,
                                                          &parameter_array.at(2) );
    GLDriverFunctions::get().get_tex_parameteriv_func_ptr(GL_TEXTURE_2D,
                                                          TEXTURE_WRAP_T,
                                                          &parameter_array.at(3) );

    GLDriverFunctions::get().gen_textures_func_ptr(1,
                                                   &twin);
    GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                   twin);

    GLDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_MIN_FILTER,
                                                     parameter_array.at(0) );
    GLDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_MAG_FILTER,
                                                     parameter_array.at(1) );
    GLDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_WRAP_S,
                                                     parameter_array.at(2) );
    GLDriverFunctions::get().tex_parameteri_func_ptr(GL_TEXTURE_2D,
                                                     TEXTURE_WRAP_T,
                                                     parameter_array.at(3) );

    if (in_has_level_size)
    {
        GLDriverFunctions::get().tex_image_2d_func_ptr(GL_TEXTURE_2D,
                                                       in_level,
                                                       format,
                                                       width,
                                                       height,
                                                       0, /* border */
                                                       GL_RGBA,
                                                       GL_UNSIGNED_BYTE,
                                                       nullptr);
    }

    GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                   in_texture);

    m_twin_map[in_texture] = twin;
    m_statistics.n_twin_textures++;

    result = true;

end:
    return result;
}

Stereo::Core::DepthRange Stereo::ConverterDLL::GLEyeRenderer::measure_depths(const float&  in_z_scale,
                                                                             const float&  in_z_shift,
                                                                             const float*  in_depth_ptr,
                                                                             const size_t& in_n_depths)
{
    /* A depth array larger than N_DEPTHS_LOOKED_AT_WHOLE is a whole depth buffer (the small copy could not be made), so every fourth pixel is measured. The small copy is measured at every pixel. */
    constexpr size_t N_DEPTHS_LOOKED_AT_WHOLE = 100000;
    constexpr size_t N_PIXELS_BETWEEN_SAMPLES = 4;

    const float  near_fraction = m_core_ptr->get_near_fraction();
    const size_t step          = (in_n_depths > N_DEPTHS_LOOKED_AT_WHOLE) ? N_PIXELS_BETWEEN_SAMPLES
                                                                          : 1;

    return m_fast_depth_range_measurer.measure(in_depth_ptr,
                                               in_n_depths,
                                               step,
                                               in_z_scale,
                                               in_z_shift,
                                               near_fraction);
}

void Stereo::ConverterDLL::GLEyeRenderer::measure_pass_depth()
{
    const bool is_ready = m_targets.is_ready();

    if (m_measures_depth          &&
        m_pass_has_affected_draws &&
        is_ready)
    {
        forget_lost_depth_reads();

        /* With a buffer of the graphics card, the depth read is not waited for. Its result is taken at the end of the next frame, and it belongs to the frame the read started in. */
        if (m_targets.has_depth_read_buffers() )
        {
            start_depth_read();
        }
        else
        {
            read_depth_at_once();
        }
    }

    m_pass_has_affected_draws = false;
}

bool Stereo::ConverterDLL::GLEyeRenderer::mirror_copy(const APIInterceptor::APIFunction&         in_function,
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

            /* The framebuffer is larger than the application knows, so the sizes of the copy are scaled up by the same factor. */
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

                GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                               twin);

                if (is_sub)
                {
                    GLDriverFunctions::get().copy_tex_sub_image_2d_func_ptr(target,
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
                    GLDriverFunctions::get().copy_tex_image_2d_func_ptr(target,
                                                                        value_array.at(1),
                                                                        value_array.at(2),
                                                                        value_array.at(3),
                                                                        value_array.at(4),
                                                                        value_array.at(5),
                                                                        value_array.at(6),
                                                                        value_array.at(7) );
                }

                GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
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

            /* The application's copy call uses the sizes of the smaller framebuffer, so it cannot be made. The left-eye copy is made with the scaled-up sizes instead. */
            if (is_scaled)
            {
                if (is_sub)
                {
                    GLDriverFunctions::get().copy_tex_sub_image_2d_func_ptr(target,
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
                    GLDriverFunctions::get().copy_tex_image_2d_func_ptr(target,
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

void Stereo::ConverterDLL::GLEyeRenderer::mirror_texture_parameter(const APIInterceptor::APIFunction&         in_function,
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

        GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                       twin_it->second);

        if (in_function == APIInterceptor::APIFUNCTION_GL_GLTEXPARAMETERI)
        {
            GLDriverFunctions::get().tex_parameteri_func_ptr(target,
                                                             name,
                                                             static_cast<GLint>(value) );
        }
        else
        {
            GLDriverFunctions::get().tex_parameterf_func_ptr(target,
                                                             name,
                                                             value);
        }

        GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
                                                       texture);
    }
}

void Stereo::ConverterDLL::GLEyeRenderer::note_once(const std::string& in_key,
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

bool Stereo::ConverterDLL::GLEyeRenderer::on_frame_ending(GLFrameImages* out_images_ptr)
{
    bool result = false;

    /* A frame ends here. The picture that was read is out of date in the next frame, whatever calls come in between. */
    m_readback_cache.invalidate();

    if (m_targets.is_ready() == false)
    {
        goto end;
    }

    /* Take the results of the depth reads started in earlier frames. finish_depth_read() waits for any read that is still running. Each result goes to the frame it belongs to. */
    forget_lost_depth_reads();

    collect_depth_reads(true,   /* in_only_older_frames */
                        false); /* in_only_the_oldest */

    measure_pass_depth();

    /* The depth of the previous frame is complete, because all of its reads were taken above. With buffers of the graphics card, the depth of this frame arrives one frame later (lag 1). Without them it was read at once (lag 0). */
    m_depth_lag = (m_targets.has_depth_read_buffers() ) ? 1
                                                        : 0;

    m_depth_ledger.end_frame(m_depth_lag);

    {
        /* The images returned belong to the previous frame. They were read while this frame was being made. */
        result = m_frame_reader.read(&m_targets,
                                     out_images_ptr);

        out_images_ptr->picture = m_targets.get_picture();

        /* A program that shows the images elsewhere does not need the window. Otherwise the window shows the left eye. */
        if (m_is_shown_elsewhere == false)
        {
            m_targets.blit_left_to_window();
        }

        m_invisible_window_manager.keep_invisible();
    }

end:
    return result;
}

void Stereo::ConverterDLL::GLEyeRenderer::on_post(const APIInterceptor::APIFunction&         in_function,
                                                  const uint32_t&                            in_n_arguments_out,
                                                  const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr)
{
    (void) in_n_arguments_out;
    (void) in_arguments_out_ptr;

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS:
        {
            /* The window may have a new size, so the framebuffers are made again for the frame that begins. */
            try_to_prepare();

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLEND:
        {
            if (m_is_in_block)
            {
                const OwnCallScope own_call_scope;

                GLDriverFunctions::get().end_list_func_ptr();

                m_is_in_block = false;

                if (m_block_has_draws == false ||
                    m_block_class     != Analysis::Recipe::DrawClass::SKIPPED)
                {
                    const GLuint list = m_block_list;

                    const auto call_list_func = [list]()
                    {
                        GLDriverFunctions::get().call_list_func_ptr(list);
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

bool Stereo::ConverterDLL::GLEyeRenderer::on_pre(const APIInterceptor::APIFunction&         in_function,
                                                 const uint32_t&                            in_n_arguments,
                                                 const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                 const bool&                                in_has_draws,
                                                 const Analysis::Recipe::DrawClass&         in_draw_class,
                                                 const bool&                                in_is_pushed)
{
    constexpr GLenum COMPILE_AND_EXECUTE = 0x1301;
    constexpr GLenum TEXTURE0            = 0x84C0;

    bool result            = true;
    bool is_made_scaled    = false;
    bool is_read_served    = false;

    /* Any call except a pixel read may change the picture, so the picture that was read is discarded. This applies only if the recipe serves pixel reads from the picture. */
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

    /* State that the application gives in pixels is adapted to the size of the framebuffers. This applies inside and outside a display list that the application compiles; inside one, the call made here is compiled into the list. */
    if (m_targets.is_ready()            &&
        m_scaler.is_identity() == false &&
        is_read_served         == false)
    {
        is_made_scaled = m_scaler.on_pre(in_function,
                                         in_n_arguments,
                                         in_arguments_ptr,
                                         m_bound_texture_array.at(m_active_unit) );
    }

    /* A display list that the application compiles executes nothing. Its contents run when the list is called. */
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
            /* The block is compiled, not executed. It is executed once for each eye when the block ends, when the class that the recipe gives it is known. */
            if (m_targets.is_ready() )
            {
                const OwnCallScope own_call_scope;

                if (m_block_list == 0)
                {
                    m_block_list = GLDriverFunctions::get().gen_lists_func_ptr(1);
                }

                GLDriverFunctions::get().new_list_func_ptr(m_block_list,
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

                /* What the pass drew stays in the depth buffer until the buffer is cleared. */
                if (mask & DEPTH_BUFFER_BIT)
                {
                    measure_pass_depth();
                }

                const OwnCallScope own_call_scope;

                m_targets.bind_eye(Analysis::Recipe::Eye::RIGHT);

                GLDriverFunctions::get().clear_func_ptr(mask);

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

                        GLDriverFunctions::get().delete_textures_func_ptr(1,
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

bool Stereo::ConverterDLL::GLEyeRenderer::on_pre_draw(const APIInterceptor::APIFunction&         in_function,
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
                    GLDriverFunctions::get().call_list_func_ptr(list);
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
                    GLDriverFunctions::get().draw_arrays_func_ptr(mode,
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
                /* The indices are either in the memory of the application (a chunk) or an offset into a bound buffer. In the second case the pointer value is the offset itself. */
                const void* const indices_ptr = (in_arguments_ptr[3].type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID) ? resolve_pointer(in_arguments_ptr[3])
                                                                                                                    : in_arguments_ptr[3].get_ptr();

                draw_func = [count, indices_ptr, mode, type]()
                {
                    GLDriverFunctions::get().draw_elements_func_ptr(mode,
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

    /* The converter makes this call for both eyes. A call whose arguments cannot be read is left to the application. */
    return (draw_func == nullptr);
}

void Stereo::ConverterDLL::GLEyeRenderer::play_eyes(const std::function<void()>&       in_draw_func,
                                                    const Analysis::Recipe::DrawClass& in_draw_class,
                                                    const bool&                        in_is_pushed)
{
    /* Positions of the values this function needs within a matrix (stored column by column) and within a viewport. */
    constexpr size_t MATRIX_X_SCALE              = 0;
    constexpr size_t MATRIX_Z_TRANSLATION        = 14;
    constexpr GLenum MODELVIEW                   = 0x1700;
    constexpr GLenum PROJECTION_MATRIX           = 0x0BA7;
    constexpr GLenum VIEWPORT                    = 0x0BA2;
    constexpr size_t VIEWPORT_HEIGHT             = 3;
    constexpr size_t VIEWPORT_WIDTH              = 2;
    constexpr size_t VIEWPORT_X                  = 0;
    constexpr size_t VIEWPORT_Y                  = 1;

    const OwnCallScope    own_call_scope;
    bool                  has_read_matrix    = false;
    bool                  is_affected        = (in_draw_class == Analysis::Recipe::DrawClass::AFFECTED);
    const bool            is_last_draw_part  = m_core_ptr->get_is_last_draw_part();
    bool                  is_part            = false;
    const bool            is_side_reaching   = m_core_ptr->get_is_last_draw_side_reaching();
    GLint                 matrix_mode        = MODELVIEW;
    std::array<float, 16> part_matrix;
    std::array<float, 16> part_projection;
    std::array<float, 16> saved_matrix;
    std::array<GLint, 4>  saved_viewport     = {};

    /* The viewport is the one the draw is made with. A pushed draw is moved by shifting the origin of the viewport, which works with any projection. */
    if (in_is_pushed)
    {
        GLDriverFunctions::get().get_integerv_func_ptr(VIEWPORT,
                                                       saved_viewport.data() );
    }

    /* A part that protrudes from an object is drawn once for both eyes. It must appear where the eyes would have put its origin, so the camera of the draw is read here. */
    if (in_is_pushed &&
        is_last_draw_part)
    {
        GLDriverFunctions::get().get_floatv_func_ptr(GL_MODELVIEW_MATRIX,
                                                     part_matrix.data() );
        GLDriverFunctions::get().get_floatv_func_ptr(PROJECTION_MATRIX,
                                                     part_projection.data() );

        is_part = true;
    }

    if (is_affected                         &&
        m_measures_depth                    &&
        m_pass_has_affected_draws == false)
    {
        GLDriverFunctions::get().get_floatv_func_ptr(PROJECTION_MATRIX,
                                                     m_pass_projection_f32mat.data() );

        /* A projection that is not perspective carries no depth information. */
        m_pass_has_affected_draws = Analysis::APIDumpAnalysis::DrawMeasurer::is_perspective_projection(m_pass_projection_f32mat);
    }

    if (is_affected)
    {
        GLDriverFunctions::get().get_integerv_func_ptr(GL_MATRIX_MODE,
                                                       &matrix_mode);

        has_read_matrix = true;

        if (matrix_mode != MODELVIEW)
        {
            GLDriverFunctions::get().matrix_mode_func_ptr(MODELVIEW);
        }

        GLDriverFunctions::get().get_floatv_func_ptr(GL_MODELVIEW_MATRIX,
                                                     saved_matrix.data() );

        /* The core decides whether the draw is attached to the camera from the distance of its origin in front of the camera. */
        const float origin_depth = -saved_matrix.at(MATRIX_Z_TRANSLATION);

        if (m_core_ptr->is_last_draw_attached_to_camera(origin_depth) )
        {
            is_affected = false;

            m_statistics.n_camera_attached_draws++;
        }
    }

    /* Parallax the eyes already apply to the draw, which the push must leave room for; zero for an unmoved draw. */
    float layer_parallax = 0.0f;

    if (in_is_pushed &&
        is_affected)
    {
        std::array<float, 16> projection_f32mat;

        GLDriverFunctions::get().get_floatv_func_ptr(PROJECTION_MATRIX,
                                                     projection_f32mat.data() );

        layer_parallax = m_core_ptr->get_eye_parallax(-saved_matrix.at(MATRIX_Z_TRANSLATION),
                                                      projection_f32mat.at(MATRIX_X_SCALE),
                                                      Analysis::APIDumpAnalysis::DrawMeasurer::is_perspective_projection(projection_f32mat) );
    }

    for (const auto eye : {Analysis::Recipe::Eye::RIGHT, Analysis::Recipe::Eye::LEFT})
    {
        m_targets.bind_eye(eye);

        if (is_affected)
        {
            const auto eye_matrix = m_core_ptr->make_eye_shift_matrix(eye);

            GLDriverFunctions::get().load_matrixf_func_ptr(eye_matrix.data() );
            GLDriverFunctions::get().mult_matrixf_func_ptr(saved_matrix.data() );
        }

        select_textures(eye == Analysis::Recipe::Eye::RIGHT);

        if (in_is_pushed)
        {
            const float picture_width_pixels = static_cast<float>(saved_viewport.at(VIEWPORT_WIDTH) );
            float       shift_pixels         = m_core_ptr->plan_push_shift_with_layer(eye,
                                                                                          picture_width_pixels,
                                                                                          layer_parallax);

            if (is_part)
            {
                /* A perspective projection has a -1 in the third column of its fourth row. */
                shift_pixels = m_core_ptr->plan_part_shift(eye,
                                                           picture_width_pixels,
                                                           part_matrix.at(MATRIX_Z_TRANSLATION),
                                                           part_projection.at(MATRIX_X_SCALE),
                                                           Analysis::APIDumpAnalysis::DrawMeasurer::is_perspective_projection(part_projection) );
            }

            /* The shift is rounded toward zero so that it stays within the plan. An overlay that reaches the sides is widened so that no edge is left bare. */
            const auto pushed_viewport = Stereo::Core::PushShiftPlanner::make_viewport(saved_viewport.at(VIEWPORT_X),
                                                                                       saved_viewport.at(VIEWPORT_WIDTH),
                                                                                       shift_pixels,
                                                                                       (is_part == false && is_side_reaching) );

            GLDriverFunctions::get().viewport_func_ptr(pushed_viewport.x,
                                                       saved_viewport.at(VIEWPORT_Y),
                                                       pushed_viewport.width,
                                                       saved_viewport.at(VIEWPORT_HEIGHT) );
        }

        in_draw_func();
    }

    if (in_is_pushed)
    {
        GLDriverFunctions::get().viewport_func_ptr(saved_viewport.at(VIEWPORT_X),
                                                   saved_viewport.at(VIEWPORT_Y),
                                                   saved_viewport.at(VIEWPORT_WIDTH),
                                                   saved_viewport.at(VIEWPORT_HEIGHT) );
    }

    select_textures(false);

    if (is_affected)
    {
        GLDriverFunctions::get().load_matrixf_func_ptr(saved_matrix.data() );
    }

    if (has_read_matrix                &&
        matrix_mode != MODELVIEW)
    {
        GLDriverFunctions::get().matrix_mode_func_ptr(matrix_mode);
    }
}

void Stereo::ConverterDLL::GLEyeRenderer::read_depth_at_once()
{
    uint32_t depth_height = 0;
    uint32_t depth_width  = 0;

    const bool is_read = m_targets.read_depth(&m_depth_f32_vec,
                                              &depth_width,
                                              &depth_height);

    if (is_read)
    {
        const auto range = measure_depths(m_pass_projection_f32mat.at(10),
                                          m_pass_projection_f32mat.at(14),
                                          m_depth_f32_vec.data(),
                                          m_depth_f32_vec.size() );

        if (range.has_depth)
        {
            m_depth_ledger.add_result_now(range,
                                          m_pass_projection_f32mat.at(0) );
        }
    }
}

const void* Stereo::ConverterDLL::GLEyeRenderer::resolve_pointer(const APIInterceptor::APIFunctionArgument& in_argument) const
{
    const void* result = nullptr;

    if (in_argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        uint32_t n_bytes = 0;

        /* The chunk belongs to the converter now: the call analyzer took it out of the interceptor when it analysed the call. */
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

void Stereo::ConverterDLL::GLEyeRenderer::select_textures(const bool& in_are_twins)
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

            /* Only the active unit can be bound without calling the function that changes the active unit. */
            if (n_unit == m_active_unit)
            {
                GLDriverFunctions::get().bind_texture_func_ptr(GL_TEXTURE_2D,
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

bool Stereo::ConverterDLL::GLEyeRenderer::serve_pixel_read(const uint32_t&                            in_n_arguments,
                                                           const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t N_PIXEL_BYTES = 3; /* R, G and B of a pixel that the application reads as GL_RGB */

    SinglePixelRead read;
    const bool      is_read        = GLResolutionScaler::parse_single_pixel_read(in_n_arguments,
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

        /* This is the picture the application sees: the graphics card scales it down to the size of the application's window. */
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

void Stereo::ConverterDLL::GLEyeRenderer::set_get_chunk_func(const GetChunkFunc& in_get_chunk_func)
{
    m_get_chunk_func = in_get_chunk_func;
}

void Stereo::ConverterDLL::GLEyeRenderer::start_depth_read()
{
    uint32_t slot = 0;

    bool is_started = m_targets.begin_depth_read(&slot);

    /* All buffers are in use. Wait for the oldest read so that one buffer is free. */
    if (is_started == false)
    {
        collect_depth_reads(false, /* in_only_older_frames */
                            true); /* in_only_the_oldest */

        is_started = m_targets.begin_depth_read(&slot);
    }

    if (is_started)
    {
        m_depth_ledger.add_pending(slot,
                                   m_pass_projection_f32mat.at(0),
                                   m_pass_projection_f32mat.at(10),
                                   m_pass_projection_f32mat.at(14) );
    }
}

bool Stereo::ConverterDLL::GLEyeRenderer::take_frame_depth(Stereo::Core::DepthRange* out_depth_range_ptr,
                                                           float*                    out_projection_scale_ptr)
{
    return m_depth_ledger.take_ready(out_depth_range_ptr,
                                     out_projection_scale_ptr);
}

std::vector<std::string> Stereo::ConverterDLL::GLEyeRenderer::take_notes()
{
    std::vector<std::string> result;

    result.swap(m_note_vec);

    return result;
}

void Stereo::ConverterDLL::GLEyeRenderer::try_to_prepare()
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
        Stereo::Core::PixelRect picture;

        picture.height = asked_height;
        picture.width  = asked_width;

        /* The framebuffers are as large as the eye of the headset allows, with the proportions of the window. */
        if (asked_width  > 0 &&
            asked_height > 0)
        {
            constexpr GLenum MAX_RENDERBUFFER_SIZE = 0x84E8;

            GLint max_extent = 0;

            GLDriverFunctions::get().get_integerv_func_ptr(MAX_RENDERBUFFER_SIZE,
                                                           &max_extent);

            m_scaler.plan(asked_width,
                          asked_height,
                          static_cast<uint32_t>(max_extent),
                         &real_width,
                         &real_height,
                         &picture);
        }

        const bool is_ready = m_targets.ensure(context,
                                               real_width,
                                               real_height,
                                               picture);

        /* Until the application sets its own, the viewport and scissor box are the picture. */
        if (is_ready                           &&
            m_scaler.is_identity() == false    &&
            (real_width  != m_applied_width    ||
             real_height != m_applied_height) )
        {
            GLDriverFunctions::get().viewport_func_ptr(static_cast<GLint>(picture.x),
                                                       static_cast<GLint>(picture.y),
                                                       static_cast<GLsizei>(picture.width),
                                                       static_cast<GLsizei>(picture.height) );
            GLDriverFunctions::get().scissor_func_ptr (static_cast<GLint>(picture.x),
                                                       static_cast<GLint>(picture.y),
                                                       static_cast<GLsizei>(picture.width),
                                                       static_cast<GLsizei>(picture.height) );

            m_applied_height = real_height;
            m_applied_width  = real_width;
        }

        if (is_ready && was_ready == false)
        {
            note_once("ready",
                      "The framebuffers of the eyes are made, " + std::to_string(m_targets.get_width() ) + " x " + std::to_string(m_targets.get_height() ) );

            if (m_is_shown_elsewhere)
            {
                m_invisible_window_manager.add(window);
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
