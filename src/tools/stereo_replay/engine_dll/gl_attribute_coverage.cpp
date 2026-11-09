/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "capture/api_dump_names/function_names.h"
#include "gl_attribute_coverage.h"
#include <string.h>
#include <vector>

namespace
{
    /* The groups of glPushAttrib. */
    constexpr uint32_t COLOR_BUFFER_BIT   = 0x00004000;
    constexpr uint32_t CURRENT_BIT        = 0x00000001;
    constexpr uint32_t DEPTH_BUFFER_BIT   = 0x00000100;
    constexpr uint32_t ENABLE_BIT         = 0x00002000;
    constexpr uint32_t FOG_BIT            = 0x00000080;
    constexpr uint32_t HINT_BIT           = 0x00008000;
    constexpr uint32_t LIGHTING_BIT       = 0x00000040;
    constexpr uint32_t LINE_BIT           = 0x00000004;
    constexpr uint32_t POINT_BIT          = 0x00000002;
    constexpr uint32_t POLYGON_BIT        = 0x00000008;
    constexpr uint32_t SCISSOR_BIT        = 0x00080000;
    constexpr uint32_t STENCIL_BUFFER_BIT = 0x00000400;
    constexpr uint32_t TEXTURE_BIT        = 0x00040000;
    constexpr uint32_t VIEWPORT_BIT       = 0x00000800;

    bool has_prefix(const char* in_name_ptr,
                    const char* in_prefix_ptr)
    {
        return (::strncmp(in_name_ptr,
                          in_prefix_ptr,
                          ::strlen(in_prefix_ptr) ) == 0);
    }

    bool is_name(const char* in_name_ptr,
                 const char* in_other_ptr)
    {
        return (::strcmp(in_name_ptr,
                         in_other_ptr) == 0);
    }

    bool is_digit_after(const char* in_name_ptr,
                        const char* in_prefix_ptr)
    {
        const size_t n_prefix_characters = ::strlen(in_prefix_ptr);

        return (has_prefix(in_name_ptr,
                           in_prefix_ptr)                   &&
                in_name_ptr[n_prefix_characters] >= '0'     &&
                in_name_ptr[n_prefix_characters] <= '9');
    }

    /* The groups that a function of that name is saved in, from the beginning of its name; no more than what a call only gives a value to. */
    uint32_t get_bits_of_name(const char* in_name_ptr)
    {
        uint32_t result = 0;

        if (is_name(in_name_ptr, "glEnable")  ||
            is_name(in_name_ptr, "glDisable"))
        {
            result = ENABLE_BIT;
        }
        else
        if (is_digit_after(in_name_ptr, "glColor")     ||
            is_digit_after(in_name_ptr, "glNormal")    ||
            is_digit_after(in_name_ptr, "glTexCoord")  ||
            has_prefix    (in_name_ptr, "glMultiTexCoord") ||
            (has_prefix(in_name_ptr, "glFogCoord") && strstr(in_name_ptr, "Pointer") == nullptr) )
        {
            result = CURRENT_BIT;
        }
        else
        if (has_prefix(in_name_ptr, "glMaterial")   ||
            has_prefix(in_name_ptr, "glLightModel") ||
            is_name   (in_name_ptr, "glLightf")     ||
            is_name   (in_name_ptr, "glLightfv")    ||
            is_name   (in_name_ptr, "glLighti")     ||
            is_name   (in_name_ptr, "glLightiv")    ||
            is_name   (in_name_ptr, "glShadeModel") ||
            is_name   (in_name_ptr, "glColorMaterial") )
        {
            result = LIGHTING_BIT;
        }
        else
        if (has_prefix(in_name_ptr, "glTexEnv") ||
            has_prefix(in_name_ptr, "glTexGen") )
        {
            result = TEXTURE_BIT;
        }
        else
        if (has_prefix(in_name_ptr, "glBlendFunc")     ||
            has_prefix(in_name_ptr, "glBlendEquation") ||
            is_name   (in_name_ptr, "glBlendColor")    ||
            is_name   (in_name_ptr, "glAlphaFunc")     ||
            is_name   (in_name_ptr, "glColorMask")     ||
            is_name   (in_name_ptr, "glClearColor")    ||
            is_name   (in_name_ptr, "glLogicOp") )
        {
            result = COLOR_BUFFER_BIT;
        }
        else
        if (is_name(in_name_ptr, "glDepthFunc")  ||
            is_name(in_name_ptr, "glDepthMask")  ||
            is_name(in_name_ptr, "glClearDepth") )
        {
            result = DEPTH_BUFFER_BIT;
        }
        else
        if (is_name(in_name_ptr, "glCullFace")      ||
            is_name(in_name_ptr, "glFrontFace")     ||
            is_name(in_name_ptr, "glPolygonMode")   ||
            is_name(in_name_ptr, "glPolygonOffset") )
        {
            result = POLYGON_BIT;
        }
        else
        if (is_name(in_name_ptr, "glLineWidth")   ||
            is_name(in_name_ptr, "glLineStipple") )
        {
            result = LINE_BIT;
        }
        else
        if (is_name   (in_name_ptr, "glPointSize") ||
            has_prefix(in_name_ptr, "glPointParameter") )
        {
            result = POINT_BIT;
        }
        else
        if (is_name(in_name_ptr, "glFogf")  ||
            is_name(in_name_ptr, "glFogfv") ||
            is_name(in_name_ptr, "glFogi")  ||
            is_name(in_name_ptr, "glFogiv") )
        {
            result = FOG_BIT;
        }
        else
        if (is_name(in_name_ptr, "glHint") )
        {
            result = HINT_BIT;
        }
        else
        if (is_name(in_name_ptr, "glViewport")   ||
            is_name(in_name_ptr, "glDepthRange") )
        {
            result = VIEWPORT_BIT;
        }
        else
        if (is_name(in_name_ptr, "glScissor") )
        {
            result = SCISSOR_BIT;
        }
        else
        if (has_prefix(in_name_ptr, "glStencil")       ||
            is_name   (in_name_ptr, "glClearStencil") )
        {
            result = STENCIL_BUFFER_BIT;
        }

        return result;
    }
}

uint32_t Stereo::Replay::EngineDLL::GlAttributeCoverage::get_attribute_bits(const APIInterceptor::APIFunction& in_function)
{
    /* Made once, from the names, when the first call is asked about; the reader threads ask at the same time. */
    static const std::vector<uint32_t> bits_vec = []()
    {
        std::vector<uint32_t> result_vec(static_cast<size_t>(APIInterceptor::APIFUNCTION_COUNT),
                                         0);

        for (size_t n_function = 0;
                    n_function < result_vec.size();
                  ++n_function)
        {
            const char* name_ptr = Capture::APIDumpNames::get_function_name(static_cast<APIInterceptor::APIFunction>(n_function) );

            if (name_ptr != nullptr)
            {
                result_vec.at(n_function) = get_bits_of_name(name_ptr);
            }
        }

        return result_vec;
    }();

    const size_t index  = static_cast<size_t>(in_function);
    uint32_t     result = 0;

    if (index < bits_vec.size() )
    {
        result = bits_vec.at(index);
    }

    return result;
}

bool Stereo::Replay::EngineDLL::GlAttributeCoverage::is_undone_by_pop(const uint32_t&                    in_mask,
                                                                      const APIInterceptor::APIFunction& in_function)
{
    const uint32_t bits = get_attribute_bits(in_function);

    return (bits != 0 &&
            (in_mask & bits) == bits);
}
