/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_resolution_hazards.h"
#include "common/argument_reader.h"
#include "common/callbacks.h"
#include "gl_driver_functions.h"
#include "gl_resolution_scaler.h"
#include "own_call_scope.h"
#include "render_scale_planner.h"
#include "resolution_hazard_gate.h"
#include "eye_image_limits.h"

Stereo::ConverterDLL::GlResolutionScaler::GlResolutionScaler(const uint32_t&              in_target_width,
                                                             const uint32_t&              in_target_height,
                                                             Stereo::Core::StereoCore*    in_core_ptr,
                                                             const Stereo::Core::LogFunc& in_log_func)
    :m_asked_height(0),
     m_asked_width (0),
     m_core_ptr    (in_core_ptr),
     m_log_func    (in_log_func),
     m_max_extent  (0),
     m_policy      (Stereo::Core::ResolutionPolicy::get_min_scale() ),
     m_real_height (0),
     m_real_width  (0),
     m_target_height(in_target_height),
     m_target_width (in_target_width)
{
    /* Stub */
}

void Stereo::ConverterDLL::GlResolutionScaler::forget_texture(const uint32_t& in_texture)
{
    m_scaled_texture_set.erase(in_texture);
}

uint32_t Stereo::ConverterDLL::GlResolutionScaler::get_asked_height() const
{
    return m_asked_height;
}

uint32_t Stereo::ConverterDLL::GlResolutionScaler::get_asked_width() const
{
    return m_asked_width;
}

const Stereo::Core::ResolutionPolicy& Stereo::ConverterDLL::GlResolutionScaler::get_policy() const
{
    return m_policy;
}

bool Stereo::ConverterDLL::GlResolutionScaler::is_identity() const
{
    return m_policy.is_identity();
}

bool Stereo::ConverterDLL::GlResolutionScaler::is_null_pointer(const APIInterceptor::APIFunctionArgument& in_argument)
{
    bool result = false;

    if (in_argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        const void* data_ptr = nullptr;
        uint32_t    n_bytes  = 0;

        /* A pointer that is null has no chunk (or one of no bytes): the data of an upload is none. Data that is there is a chunk of its bytes. */
        const bool has_chunk = APIInterceptor::get_data_chunk(in_argument.get_data_chunk_id(),
                                                             &data_ptr,
                                                             &n_bytes);

        result = (has_chunk == false ||
                  n_bytes   == 0);
    }
    else
    if (in_argument.type == APIInterceptor::ARGTYPE_VOID_PTR)
    {
        result = (in_argument.get_ptr() == nullptr);
    }

    return result;
}

bool Stereo::ConverterDLL::GlResolutionScaler::is_scaled_texture(const uint32_t& in_texture) const
{
    return (m_scaled_texture_set.find(in_texture) != m_scaled_texture_set.end() );
}

bool Stereo::ConverterDLL::GlResolutionScaler::make_scaled_read(const uint32_t&                            in_n_arguments,
                                                                const APIInterceptor::APIFunctionArgument* in_arguments_ptr) const
{
    constexpr GLsizei READ_EXTENT = 1;

    SinglePixelRead read;
    const bool      is_identity = m_policy.is_identity();
    const bool      is_read     = parse_single_pixel_read(in_n_arguments,
                                                          in_arguments_ptr,
                                                         &read);
    bool            result      = false;

    if (is_read                  &&
        is_identity    == false  &&
        m_asked_height != 0      &&
        m_asked_width  != 0      &&
        m_real_height  != 0      &&
        m_real_width   != 0)
    {
        /* The pixel under the centre of the one that the application names, which is the nearest one that it has if it names one that it does not. */
        const uint32_t x_real = m_policy.scale_pixel_coordinate(read.x,
                                                                m_asked_width,
                                                                m_real_width);
        const uint32_t y_real = m_policy.scale_pixel_coordinate(read.y,
                                                                m_asked_height,
                                                                m_real_height);

        const OwnCallScope own_call_scope;

        /* The framebuffer that is read is the one that the application renders to, and the pixel is given to it in its own buffer, with the way of packing that it has set. */
        GlDriverFunctions::get().read_pixels_func_ptr(static_cast<GLint>(x_real),
                                                      static_cast<GLint>(y_real),
                                                      READ_EXTENT,
                                                      READ_EXTENT,
                                                      static_cast<GLenum>(read.format),
                                                      static_cast<GLenum>(read.type),
                                                      read.pixels_ptr);

        result = true;
    }

    return result;
}

bool Stereo::ConverterDLL::GlResolutionScaler::on_pre(const APIInterceptor::APIFunction&         in_function,
                                                      const uint32_t&                            in_n_arguments,
                                                      const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                      const uint32_t&                            in_bound_texture)
{
    constexpr GLenum TEXTURE_2D = 0x0DE1;

    Analysis::APIDumpAnalysis::ResolutionHazard hazard = Analysis::APIDumpAnalysis::ResolutionHazard::SIZE_QUERIES;
    bool                                        result = false;

    /* A read of bytes from the framebuffer is made here, for the larger one, and the application gets what it would have. */
    if (in_function == APIInterceptor::APIFUNCTION_GL_GLREADPIXELS)
    {
        result = make_scaled_read(in_n_arguments,
                                  in_arguments_ptr);
    }

    /* What cannot be made to match ends the application. */
    if (result == false)
    {
        const bool is_hazard = Analysis::APIDumpAnalysis::Frontends::GlResolutionHazards::classify(in_function,
                                                                                                   in_n_arguments,
                                                                                                   in_arguments_ptr,
                                                                                                  &hazard);

        if (is_hazard)
        {
            ResolutionHazardGate::require_unaffected_by_scale(m_core_ptr,
                                                              m_policy,
                                                              hazard);
        }
    }

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_GL_GLLINEWIDTH:
        case APIInterceptor::APIFUNCTION_GL_GLPOINTSIZE:
        {
            float size = 0.0f;

            if (in_n_arguments > 0                                       &&
                APIInterceptor::read_number(in_arguments_ptr[0],
                                           &size) )
            {
                const OwnCallScope own_call_scope;
                const float        scaled_size = size * m_policy.get_scale();

                if (in_function == APIInterceptor::APIFUNCTION_GL_GLLINEWIDTH)
                {
                    GlDriverFunctions::get().line_width_func_ptr(scaled_size);
                }
                else
                {
                    GlDriverFunctions::get().point_size_func_ptr(scaled_size);
                }

                result = true;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLSCISSOR:
        case APIInterceptor::APIFUNCTION_GL_GLVIEWPORT:
        {
            std::array<int32_t, 4> value_array = {};
            bool                   is_read     = (in_n_arguments > 3);

            for (uint32_t n_argument = 0;
                          n_argument < 4 && is_read;
                        ++n_argument)
            {
                is_read = APIInterceptor::read_number(in_arguments_ptr[n_argument],
                                                     &value_array.at(n_argument) );
            }

            /* A box with a corner or a size below 0 is for the driver to say what it is. */
            if (is_read                    &&
                value_array.at(0) >= 0     &&
                value_array.at(1) >= 0     &&
                value_array.at(2) >= 0     &&
                value_array.at(3) >= 0)
            {
                const OwnCallScope      own_call_scope;
                Stereo::Core::PixelRect rect;

                rect.height = static_cast<uint32_t>(value_array.at(3) );
                rect.width  = static_cast<uint32_t>(value_array.at(2) );
                rect.x      = static_cast<uint32_t>(value_array.at(0) );
                rect.y      = static_cast<uint32_t>(value_array.at(1) );

                /* A viewport may be larger than the window, and is then the application's to be wrong about; the scissor box is kept in the framebuffer. */
                rect = (in_function == APIInterceptor::APIFUNCTION_GL_GLSCISSOR) ? m_policy.scale_rect(rect,
                                                                                                       m_real_width,
                                                                                                       m_real_height)
                                                                                 : m_policy.scale_rect(rect);

                if (in_function == APIInterceptor::APIFUNCTION_GL_GLSCISSOR)
                {
                    GlDriverFunctions::get().scissor_func_ptr(static_cast<GLint>(rect.x),
                                                              static_cast<GLint>(rect.y),
                                                              static_cast<GLsizei>(rect.width),
                                                              static_cast<GLsizei>(rect.height) );
                }
                else
                {
                    GlDriverFunctions::get().viewport_func_ptr(static_cast<GLint>(rect.x),
                                                               static_cast<GLint>(rect.y),
                                                               static_cast<GLsizei>(rect.width),
                                                               static_cast<GLsizei>(rect.height) );
                }

                result = true;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_GL_GLTEXIMAGE2D:
        case APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE2D:
        {
            constexpr uint32_t N_TEX_IMAGE_ARGUMENTS = 9;
            constexpr uint32_t PIXELS_ARGUMENT       = 8;

            const bool is_scaled_destination = is_scaled_texture(in_bound_texture);

            /* Pixels given to a texture that has room for more of them than the application knows of cannot be made to match. */
            if (in_function == APIInterceptor::APIFUNCTION_GL_GLTEXSUBIMAGE2D)
            {
                if (is_scaled_destination)
                {
                    ResolutionHazardGate::require_unaffected_by_scale(m_core_ptr,
                                                                      m_policy,
                                                                      Analysis::APIDumpAnalysis::ResolutionHazard::TEXTURE_UPLOADS_TO_RENDER_TARGETS);
                }
            }
            else
            if (in_n_arguments == N_TEX_IMAGE_ARGUMENTS)
            {
                uint32_t target = 0;

                if (APIInterceptor::read_number(in_arguments_ptr[0],
                                               &target)              &&
                    target == TEXTURE_2D)
                {
                    const bool is_storage_only = is_null_pointer(in_arguments_ptr[PIXELS_ARGUMENT]);

                    if (is_storage_only)
                    {
                        /* Room for what the application will copy from the framebuffer: as much more of it as the framebuffer has. */
                        std::array<int32_t, 9> value_array = {};
                        bool                   is_read     = true;

                        for (uint32_t n_argument = 1;
                                      n_argument < PIXELS_ARGUMENT;
                                    ++n_argument)
                        {
                            is_read &= APIInterceptor::read_number(in_arguments_ptr[n_argument],
                                                                  &value_array.at(n_argument) );
                        }

                        if (is_read                     &&
                            value_array.at(3) >= 0      &&
                            value_array.at(4) >= 0)
                        {
                            const OwnCallScope own_call_scope;
                            const uint32_t     width  = m_policy.scale_extent(static_cast<uint32_t>(value_array.at(3) ) );
                            const uint32_t     height = m_policy.scale_extent(static_cast<uint32_t>(value_array.at(4) ) );

                            m_core_ptr->require_supported(m_max_extent == 0 ||
                                                          (width  <= m_max_extent &&
                                                           height <= m_max_extent),
                                                          "A texture that the application renders to would be " + std::to_string(width) + " x " + std::to_string(height) +
                                                          ", larger than the driver makes; lower the render scale");

                            GlDriverFunctions::get().tex_image_2d_func_ptr(target,
                                                                           value_array.at(1),
                                                                           value_array.at(2),
                                                                           static_cast<GLsizei>(width),
                                                                           static_cast<GLsizei>(height),
                                                                           value_array.at(5),
                                                                           static_cast<GLenum>(value_array.at(6) ),
                                                                           static_cast<GLenum>(value_array.at(7) ),
                                                                           nullptr);

                            if (in_bound_texture != 0)
                            {
                                m_scaled_texture_set.insert(in_bound_texture);
                            }

                            if (m_scaled_texture_set.size() <= 12)
                            {
                                m_log_func("The texture " + std::to_string(in_bound_texture) + " (storage without pixels) is made " + std::to_string(width) + " x " + std::to_string(height) + " instead of " + std::to_string(value_array.at(3) ) + " x " + std::to_string(value_array.at(4) ) );
                            }

                            result = true;
                        }
                    }
                    else
                    if (is_scaled_destination)
                    {
                        ResolutionHazardGate::require_unaffected_by_scale(m_core_ptr,
                                                                          m_policy,
                                                                          Analysis::APIDumpAnalysis::ResolutionHazard::TEXTURE_UPLOADS_TO_RENDER_TARGETS);
                    }
                }
            }

            break;
        }

        default:
        {
            break;
        }
    }

    return result;
}

bool Stereo::ConverterDLL::GlResolutionScaler::parse_single_pixel_read(const uint32_t&                            in_n_arguments,
                                                                       const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                                       SinglePixelRead*                           out_read_ptr)
{
    constexpr size_t   ARGUMENT_FORMAT    = 4;
    constexpr size_t   ARGUMENT_HEIGHT    = 3;
    constexpr size_t   ARGUMENT_TYPE      = 5;
    constexpr size_t   ARGUMENT_WIDTH     = 2;
    constexpr size_t   ARGUMENT_X         = 0;
    constexpr size_t   ARGUMENT_Y         = 1;
    constexpr uint32_t FORMAT_RGB         = 0x1907;
    constexpr size_t   N_NUMBER_ARGUMENTS = 6; /* x, y, width, height, format, type */
    constexpr uint32_t N_READ_ARGUMENTS   = 7;
    constexpr uint32_t PIXELS_ARGUMENT    = 6;
    constexpr int64_t  READ_EXTENT        = 1;
    constexpr uint32_t TYPE_UNSIGNED_BYTE = 0x1401;

    bool                                    result      = false;
    std::array<int64_t, N_NUMBER_ARGUMENTS> value_array = {};

    if (in_n_arguments < N_READ_ARGUMENTS)
    {
        goto end;
    }

    /* The buffer of the application: a pointer (not an offset in a buffer of the driver, nor none). */
    if (in_arguments_ptr[PIXELS_ARGUMENT].type != APIInterceptor::ARGTYPE_VOID_PTR)
    {
        goto end;
    }

    if (is_null_pointer(in_arguments_ptr[PIXELS_ARGUMENT]) )
    {
        goto end;
    }

    for (uint32_t n_argument = 0;
                  n_argument < value_array.size();
                ++n_argument)
    {
        if (APIInterceptor::read_number(in_arguments_ptr[n_argument],
                                       &value_array.at(n_argument) ) == false)
        {
            goto end;
        }
    }

    {
        const uint32_t format = static_cast<uint32_t>(value_array.at(ARGUMENT_FORMAT) );
        const int64_t  height = value_array.at(ARGUMENT_HEIGHT);
        const uint32_t type   = static_cast<uint32_t>(value_array.at(ARGUMENT_TYPE) );
        const int64_t  width  = value_array.at(ARGUMENT_WIDTH);

        /* Only the read that is known to be made, of one pixel of bytes of the colors: any other cannot be made to match. */
        if (width  != READ_EXTENT        ||
            height != READ_EXTENT        ||
            format != FORMAT_RGB         ||
            type   != TYPE_UNSIGNED_BYTE)
        {
            goto end;
        }

        out_read_ptr->format     = format;
        out_read_ptr->pixels_ptr = const_cast<void*>(in_arguments_ptr[PIXELS_ARGUMENT].get_ptr() );
        out_read_ptr->type       = type;
        out_read_ptr->x          = value_array.at(ARGUMENT_X);
        out_read_ptr->y          = value_array.at(ARGUMENT_Y);

        result = true;
    }

end:
    return result;
}

void Stereo::ConverterDLL::GlResolutionScaler::plan(const uint32_t& in_asked_width,
                                                    const uint32_t& in_asked_height,
                                                    const uint32_t& in_max_extent,
                                                    uint32_t*       out_real_width_ptr,
                                                    uint32_t*       out_real_height_ptr)
{
    const Stereo::Core::ResolutionPolicy policy(RenderScalePlanner::choose_scale(m_target_width,
                                                                                 m_target_height,
                                                                                 in_asked_width,
                                                                                 in_asked_height,
                                                                                 in_max_extent) );

    /* The scale of a window that changes size is the one for its new size; what the application stated for the old one it states again for the new. */
    if (policy.get_scale() != m_policy.get_scale() ||
        in_asked_height    != m_asked_height       ||
        in_asked_width     != m_asked_width)
    {
        m_log_func(RenderScalePlanner::describe_resize("framebuffer",
                                                       in_asked_width,
                                                       in_asked_height,
                                                       policy.scale_extent(in_asked_width),
                                                       policy.scale_extent(in_asked_height),
                                                       policy.get_scale() ) );
    }

    m_asked_height = in_asked_height;
    m_asked_width  = in_asked_width;
    m_max_extent   = in_max_extent;
    m_policy      = policy;
    m_real_height = policy.scale_extent(in_asked_height);
    m_real_width  = policy.scale_extent(in_asked_width);

    m_core_ptr->require_supported(EyeImageLimits::fits(m_real_width,
                                                       m_real_height),
                                  RenderScalePlanner::describe_too_large(m_real_width,
                                                                         m_real_height,
                                                                         m_policy.get_scale() ) );

    *out_real_height_ptr = m_real_height;
    *out_real_width_ptr  = m_real_width;
}

void Stereo::ConverterDLL::GlResolutionScaler::scale_copy(const bool&             in_is_sub_image,
                                                          const uint32_t&         in_bound_texture,
                                                          std::array<int32_t, 8>* inout_value_array_ptr)
{
    /* The arguments are the target, the level and then: for a sub image the offset in the texture (2 and 3), the corner of what is copied (4, 5) and its size (6, 7); for the
     * other the format (2), the corner (3, 4), the size (5, 6) and the border (7).
     */
    const size_t corner_x_index = (in_is_sub_image) ? 4 : 3;
    const size_t size_width_index = corner_x_index + 2;

    auto& value_array = *inout_value_array_ptr;

    if (in_is_sub_image)
    {
        m_core_ptr->require_fast_path(is_scaled_texture(in_bound_texture),
                                      "The application copies part of the framebuffer to a texture (the number " + std::to_string(in_bound_texture) + ") that it gave pixels, which is not larger than it knows of (" + std::to_string(m_scaled_texture_set.size() ) + " textures are), while it is rendered larger than it asks");
    }
    else
    if (in_bound_texture != 0)
    {
        m_scaled_texture_set.insert(in_bound_texture);
    }

    if (value_array.at(corner_x_index)     >= 0 &&
        value_array.at(corner_x_index + 1) >= 0 &&
        value_array.at(size_width_index)   >= 0 &&
        value_array.at(size_width_index + 1) >= 0)
    {
        Stereo::Core::PixelRect rect;

        rect.height = static_cast<uint32_t>(value_array.at(size_width_index + 1) );
        rect.width  = static_cast<uint32_t>(value_array.at(size_width_index) );
        rect.x      = static_cast<uint32_t>(value_array.at(corner_x_index) );
        rect.y      = static_cast<uint32_t>(value_array.at(corner_x_index + 1) );

        rect = m_policy.scale_rect(rect,
                                   m_real_width,
                                   m_real_height);

        value_array.at(corner_x_index)       = static_cast<int32_t>(rect.x);
        value_array.at(corner_x_index + 1)   = static_cast<int32_t>(rect.y);
        value_array.at(size_width_index)     = static_cast<int32_t>(rect.width);
        value_array.at(size_width_index + 1) = static_cast<int32_t>(rect.height);

        if (in_is_sub_image)
        {
            value_array.at(2) = static_cast<int32_t>(m_policy.scale_extent(static_cast<uint32_t>(value_array.at(2) ) ) );
            value_array.at(3) = static_cast<int32_t>(m_policy.scale_extent(static_cast<uint32_t>(value_array.at(3) ) ) );
        }
    }
}
