/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_window_extents.h"

bool Analysis::APIDumpAnalysis::Frontends::GLWindowExtents::find_client_size(const int32_t& in_width,
                                                                             const int32_t& in_height,
                                                                             ClientSize*    out_client_size_ptr)
{
    constexpr int32_t USE_DEFAULT_SIZE = static_cast<int32_t>(0x80000000);

    /* The size is the one in the call, as it is: the frame of a window depends on the system and on the program that asks, so the keys of a recipe would too. */
    const bool result = (in_width  != USE_DEFAULT_SIZE &&
                         in_height != USE_DEFAULT_SIZE &&
                         in_width  >  0                &&
                         in_height >  0);

    if (result)
    {
        out_client_size_ptr->height = static_cast<uint32_t>(in_height);
        out_client_size_ptr->width  = static_cast<uint32_t>(in_width);
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::GLWindowExtents::follows_function(const APIInterceptor::APIFunction& in_function)
{
    return (in_function == APIInterceptor::APIFUNCTION_USER32_CREATEWINDOWEXA ||
            in_function == APIInterceptor::APIFUNCTION_USER32_CREATEWINDOWEXW ||
            in_function == APIInterceptor::APIFUNCTION_USER32_GETDC);
}

bool Analysis::APIDumpAnalysis::Frontends::GLWindowExtents::get_client_size(const uint64_t& in_device_context_address,
                                                                            uint32_t*       out_width_ptr,
                                                                            uint32_t*       out_height_ptr) const
{
    bool result = false;

    const auto window_iterator = m_window_by_dc_map.find(in_device_context_address);

    if (window_iterator != m_window_by_dc_map.end() )
    {
        const auto size_iterator = m_client_size_by_window_map.find(window_iterator->second);

        if (size_iterator != m_client_size_by_window_map.end() )
        {
            *out_height_ptr = size_iterator->second.height;
            *out_width_ptr  = size_iterator->second.width;
            result          = true;
        }
    }

    return result;
}

void Analysis::APIDumpAnalysis::Frontends::GLWindowExtents::on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    if (in_call.func == APIInterceptor::APIFUNCTION_USER32_GETDC)
    {
        on_get_dc(in_call);
    }
    else
    {
        on_create_window(in_call);
    }
}

void Analysis::APIDumpAnalysis::Frontends::GLWindowExtents::on_create_window(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    constexpr size_t N_ARGUMENTS       = 12;
    constexpr size_t N_ARGUMENT_HEIGHT = 7;
    constexpr size_t N_ARGUMENT_WIDTH  = 6;

    ClientSize client_size;
    uint64_t   window_address = 0;

    if (in_call.arg_in_vec.size() < N_ARGUMENTS ||
        in_call.returned_value_ptr == nullptr)
    {
        goto end;
    }

    window_address = in_call.returned_value_ptr->get_ptr_as_u64();

    /* The call failed, or made a window which has no size to tell. */
    if (window_address == 0)
    {
        goto end;
    }

    if (find_client_size(static_cast<int32_t>(in_call.arg_in_vec.at(N_ARGUMENT_WIDTH).get_u32() ),
                         static_cast<int32_t>(in_call.arg_in_vec.at(N_ARGUMENT_HEIGHT).get_u32() ),
                        &client_size) )
    {
        m_client_size_by_window_map[window_address] = client_size;
    }

end:
    return;
}

void Analysis::APIDumpAnalysis::Frontends::GLWindowExtents::on_get_dc(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    uint64_t dc_address     = 0;
    uint64_t window_address = 0;

    if (in_call.arg_in_vec.empty() ||
        in_call.returned_value_ptr == nullptr)
    {
        goto end;
    }

    dc_address     = in_call.returned_value_ptr->get_ptr_as_u64();
    window_address = in_call.arg_in_vec.at(0).get_ptr_as_u64();

    /* GetDC of no window is the screen. */
    if (dc_address     == 0 ||
        window_address == 0)
    {
        goto end;
    }

    m_window_by_dc_map[dc_address] = window_address;

end:
    return;
}
