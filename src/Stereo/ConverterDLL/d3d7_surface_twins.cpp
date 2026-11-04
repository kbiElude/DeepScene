/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Interception/Core/D3D7Utils/constants.h"
#include "d3d7_surface_twins.h"

namespace
{
    /* Whether the call that gave the argument succeeded; a call with no returned value is taken to have. */
    bool has_succeeded(const APIInterceptor::APIFunctionArgument* in_returned_value_ptr)
    {
        bool result = true;

        if (in_returned_value_ptr != nullptr)
        {
            switch (in_returned_value_ptr->type)
            {
                case APIInterceptor::ARGTYPE_I32:
                {
                    result = (in_returned_value_ptr->get_i32() >= 0);

                    break;
                }

                case APIInterceptor::ARGTYPE_U32:
                {
                    result = (static_cast<int32_t>(in_returned_value_ptr->get_u32() ) >= 0);

                    break;
                }

                default:
                {
                    break;
                }
            }
        }

        return result;
    }
}

Stereo::ConverterDLL::D3D7SurfaceTwins::D3D7SurfaceTwins()
    :m_back_buffer_ptr(nullptr),
     m_direct_draw_ptr(nullptr),
     m_n_failed_twins (0),
     m_primary_ptr    (nullptr)
{
    /* Stub */
}

bool Stereo::ConverterDLL::D3D7SurfaceTwins::get_caps(void*     in_surface_ptr,
                                                       uint32_t* out_caps_ptr)
{
    Interception::Hooks::D3D7::DDSURFACEDESC2 description    = {};
    void** const                              surface_vtable = *reinterpret_cast<void***>(in_surface_ptr);

    description.dwSize = static_cast<DWORD>(sizeof(description) );

    const bool result = SUCCEEDED(reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_GETSURFACEDESC>(surface_vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETSURFACEDESC])(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(in_surface_ptr),
                                                                                                                                                                                                         &description) );

    *out_caps_ptr = description.ddsCaps.dwCaps;

    return result;
}

uint64_t Stereo::ConverterDLL::D3D7SurfaceTwins::get_n_failed_twins() const
{
    return m_n_failed_twins;
}

uint64_t Stereo::ConverterDLL::D3D7SurfaceTwins::get_n_twins() const
{
    return m_twin_map.size();
}

bool Stereo::ConverterDLL::D3D7SurfaceTwins::is_back_buffer(void* in_surface_ptr) const
{
    return (in_surface_ptr != nullptr &&
            in_surface_ptr == m_back_buffer_ptr);
}

void* Stereo::ConverterDLL::D3D7SurfaceTwins::get_twin_ptr(void* in_surface_ptr) const
{
    const auto twin_iterator = m_twin_map.find(in_surface_ptr);

    return (twin_iterator != m_twin_map.end() ) ? twin_iterator->second
                                                : nullptr;
}

void Stereo::ConverterDLL::D3D7SurfaceTwins::handle_add_attached_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t ATTACHED_ARGUMENT = 1;
    constexpr size_t SURFACE_ARGUMENT  = 0;

    void* const attached_twin_ptr = get_twin_ptr(const_cast<void*>(in_arguments_ptr[ATTACHED_ARGUMENT].get_ptr() ) );
    void* const surface_twin_ptr  = get_twin_ptr(const_cast<void*>(in_arguments_ptr[SURFACE_ARGUMENT].get_ptr() ) );

    /* What is attached to a surface is attached to its twin, if both have one. */
    if (attached_twin_ptr != nullptr &&
        surface_twin_ptr  != nullptr)
    {
        void** const vtable = *reinterpret_cast<void***>(surface_twin_ptr);

        reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE])(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(surface_twin_ptr),
                                                                                                                                                                                       static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(attached_twin_ptr) );
    }
}

void Stereo::ConverterDLL::D3D7SurfaceTwins::handle_create_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t DIRECT_DRAW_ARGUMENT = 0;
    constexpr size_t SURFACE_ARGUMENT     = 2;

    void* const surface_ptr = const_cast<void*>(in_arguments_ptr[SURFACE_ARGUMENT].get_ptr() );

    if (surface_ptr != nullptr)
    {
        uint32_t caps = 0;

        m_direct_draw_ptr = const_cast<void*>(in_arguments_ptr[DIRECT_DRAW_ARGUMENT].get_ptr() );

        /* The description the application gave is a structure, which the arguments hold as a chunk: the surface knows what it is. */
        if (get_caps(surface_ptr,
                    &caps) )
        {
            if ( (caps & Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE) != 0)
            {
                /* The surface that is shown: it has no twin, but its back buffer will. */
                m_primary_ptr = surface_ptr;
            }
            else
            if ( (caps & (Interception::Core::D3D7Utils::DDSCAPS_3DDEVICE |
                          Interception::Core::D3D7Utils::DDSCAPS_ZBUFFER) ) != 0)
            {
                make_twin_ptr(surface_ptr,
                              false); /* in_is_back_buffer */
            }
        }
    }
}

void Stereo::ConverterDLL::D3D7SurfaceTwins::handle_get_attached_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr size_t RESULT_ARGUMENT  = 2;
    constexpr size_t SURFACE_ARGUMENT = 0;

    void* const attached_ptr = const_cast<void*>(in_arguments_ptr[RESULT_ARGUMENT].get_ptr() );
    uint32_t    caps         = 0;
    void* const surface_ptr  = const_cast<void*>(in_arguments_ptr[SURFACE_ARGUMENT].get_ptr() );

    /* What the application asked for is a structure, which the arguments hold as a chunk: the surface that it got knows what it is. */
    if (attached_ptr == nullptr                    ||
        get_twin_ptr(attached_ptr) != nullptr      ||
        get_caps(attached_ptr,
                &caps) == false)
    {
        goto end;
    }

    if (surface_ptr == m_primary_ptr                                                      &&
        (caps & Interception::Core::D3D7Utils::DDSCAPS_BACKBUFFER) != 0)
    {
        /* The back buffer of the chain that the primary surface flips with: what the application renders to. */
        make_twin_ptr(attached_ptr,
                      true); /* in_is_back_buffer */
        m_back_buffer_ptr = attached_ptr;
    }
    else
    if ( (caps & Interception::Core::D3D7Utils::DDSCAPS_ZBUFFER) != 0)
    {
        void* const surface_twin_ptr = get_twin_ptr(surface_ptr);

        /* The depth buffer that is attached to a surface which has a twin is attached to the twin as well. */
        if (surface_twin_ptr != nullptr)
        {
            Interception::Hooks::D3D7::DDSCAPS2             depth_caps       = {};
            void**                                          vtable           = *reinterpret_cast<void***>(surface_twin_ptr);
            Interception::Hooks::D3D7::IDirectDrawSurface7* attached_twin_ptr = nullptr;

            depth_caps.dwCaps = Interception::Core::D3D7Utils::DDSCAPS_ZBUFFER;

            if (SUCCEEDED(reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE])(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(surface_twin_ptr),
                                                                                                                                                                                                             &depth_caps,
                                                                                                                                                                                                             &attached_twin_ptr) ))
            {
                void** const attached_vtable = *reinterpret_cast<void***>(attached_twin_ptr);

                m_twin_map[attached_ptr] = attached_twin_ptr;

                /* The twin holds on to what is attached to it. */
                reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_RELEASE>(attached_vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE])(attached_twin_ptr);
            }
        }
    }

end:
    return;
}

void Stereo::ConverterDLL::D3D7SurfaceTwins::handle_release(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                            const APIInterceptor::APIFunctionArgument* in_returned_value_ptr)
{
    constexpr size_t SURFACE_ARGUMENT = 0;

    if (in_returned_value_ptr != nullptr                                                                &&
        (in_returned_value_ptr->type == APIInterceptor::ARGTYPE_U32 ||
         in_returned_value_ptr->type == APIInterceptor::ARGTYPE_I32) )
    {
        const uint32_t n_references = (in_returned_value_ptr->type == APIInterceptor::ARGTYPE_U32) ? in_returned_value_ptr->get_u32()
                                                                                                    : static_cast<uint32_t>(in_returned_value_ptr->get_i32() );

        if (n_references == 0)
        {
            void* const surface_ptr = const_cast<void*>(in_arguments_ptr[SURFACE_ARGUMENT].get_ptr() );
            const auto  twin_iterator = m_twin_map.find(surface_ptr);

            if (surface_ptr == m_primary_ptr)
            {
                m_primary_ptr = nullptr;
            }

            if (surface_ptr == m_back_buffer_ptr)
            {
                m_back_buffer_ptr = nullptr;
            }

            if (twin_iterator != m_twin_map.end() )
            {
                void* const twin_ptr = twin_iterator->second;
                void** const vtable  = *reinterpret_cast<void***>(twin_ptr);

                m_twin_map.erase(twin_iterator);

                /* The reference of the twin that this made is the one that is let go of: what is attached to a twin goes with it. */
                reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_RELEASE>(vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE])(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(twin_ptr) );
            }
        }
    }
}

void* Stereo::ConverterDLL::D3D7SurfaceTwins::make_twin_ptr(void*       in_surface_ptr,
                                                            const bool& in_is_back_buffer)
{
    /* What makes a surface a part of a chain, or the surface that is shown, is not for a copy to have. */
    constexpr uint32_t CAPS_NOT_FOR_TWIN = Interception::Core::D3D7Utils::DDSCAPS_BACKBUFFER     |
                                           Interception::Core::D3D7Utils::DDSCAPS_COMPLEX        |
                                           Interception::Core::D3D7Utils::DDSCAPS_FLIP           |
                                           Interception::Core::D3D7Utils::DDSCAPS_FRONTBUFFER    |
                                           Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE;
    constexpr uint32_t FLAGS_FOR_TWIN    = Interception::Core::D3D7Utils::DDSD_CAPS        |
                                           Interception::Core::D3D7Utils::DDSD_HEIGHT      |
                                           Interception::Core::D3D7Utils::DDSD_MIPMAPCOUNT |
                                           Interception::Core::D3D7Utils::DDSD_PIXELFORMAT |
                                           Interception::Core::D3D7Utils::DDSD_WIDTH;

    Interception::Hooks::D3D7::DDSURFACEDESC2       description        = {};
    void** const                                    direct_draw_vtable = (m_direct_draw_ptr != nullptr) ? *reinterpret_cast<void***>(m_direct_draw_ptr)
                                                                                                        : nullptr;
    void*                                           result_ptr         = nullptr;
    void** const                                    surface_vtable     = *reinterpret_cast<void***>(in_surface_ptr);
    Interception::Hooks::D3D7::IDirectDrawSurface7* twin_ptr           = nullptr;

    description.dwSize = static_cast<DWORD>(sizeof(description) );

    if (m_direct_draw_ptr == nullptr                                                                                                                                                                  ||
        FAILED(reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_GETSURFACEDESC>(surface_vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETSURFACEDESC])(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(in_surface_ptr),
                                                                                                                                                                                              &description) ))
    {
        goto end;
    }

    description.dwBackBufferCount = 0;
    description.dwFlags          &= FLAGS_FOR_TWIN;
    description.ddsCaps.dwCaps   &= ~CAPS_NOT_FOR_TWIN;

    /* A back buffer is a plain surface that is rendered to, which is what a copy of it is. */
    if (in_is_back_buffer)
    {
        description.ddsCaps.dwCaps |= Interception::Core::D3D7Utils::DDSCAPS_3DDEVICE |
                                      Interception::Core::D3D7Utils::DDSCAPS_OFFSCREENPLAIN;
    }

    if (FAILED(reinterpret_cast<Interception::Hooks::D3D7::PFN_IDIRECTDRAW7_CREATESURFACE>(direct_draw_vtable[Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAW7_CREATESURFACE])(static_cast<Interception::Hooks::D3D7::IDirectDraw7*>(m_direct_draw_ptr),
                                                                                                                                                                                  &description,
                                                                                                                                                                                  &twin_ptr,
                                                                                                                                                                                   nullptr) ))
    {
        goto end;
    }

    m_twin_map[in_surface_ptr] = twin_ptr;
    result_ptr                 = twin_ptr;

end:
    if (result_ptr == nullptr)
    {
        m_n_failed_twins++;
    }

    return result_ptr;
}

void Stereo::ConverterDLL::D3D7SurfaceTwins::on_post(const APIInterceptor::APIFunction&         in_function,
                                                     const uint32_t&                            in_n_arguments,
                                                     const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                     const APIInterceptor::APIFunctionArgument* in_returned_value_ptr)
{
    constexpr uint32_t N_ARGUMENTS_ADD_ATTACHED = 2;
    constexpr uint32_t N_ARGUMENTS_CREATE       = 4;
    constexpr uint32_t N_ARGUMENTS_GET_ATTACHED = 3;
    constexpr uint32_t N_ARGUMENTS_RELEASE      = 1;

    /* A call that failed made nothing; a release which does not is told by what it returns. */
    const bool is_to_handle = (has_succeeded(in_returned_value_ptr) ||
                               in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_RELEASE);

    switch ((is_to_handle) ? in_function
                           : APIInterceptor::APIFUNCTION_UNKNOWN)
    {
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE:
        {
            if (in_n_arguments >= N_ARGUMENTS_CREATE)
            {
                handle_create_surface(in_arguments_ptr);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE:
        {
            if (in_n_arguments >= N_ARGUMENTS_ADD_ATTACHED)
            {
                handle_add_attached_surface(in_arguments_ptr);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE:
        {
            if (in_n_arguments >= N_ARGUMENTS_GET_ATTACHED)
            {
                handle_get_attached_surface(in_arguments_ptr);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_RELEASE:
        {
            if (in_n_arguments >= N_ARGUMENTS_RELEASE)
            {
                handle_release(in_arguments_ptr,
                               in_returned_value_ptr);
            }

            break;
        }

        default:
        {
            break;
        }
    }
}
