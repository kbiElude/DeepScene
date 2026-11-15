/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "common/vtable.h"
#include "interception/core/D3D7Utils/constants.h"
#include "d3d7_surface_twins.h"

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
    void** const                              surface_vtable = APIInterceptor::get_vtable_ptr(in_surface_ptr);

    description.dwSize = static_cast<DWORD>(sizeof(description) );

    const bool result = SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_GETSURFACEDESC>(surface_vtable,
                                                                                                                                    Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETSURFACEDESC)(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(in_surface_ptr),
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
    void* const surface_twin_ptr  = get_twin_ptr(const_cast<void*>(in_arguments_ptr[SURFACE_ARGUMENT].get_ptr () ) );

    /* Whatever is attached to a surface is also attached to its twin, if both have twins. */
    if (attached_twin_ptr != nullptr &&
        surface_twin_ptr  != nullptr)
    {
        void** const vtable = APIInterceptor::get_vtable_ptr(surface_twin_ptr);

        APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE>(vtable,
                                                                                                              Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_ADDATTACHEDSURFACE)(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(surface_twin_ptr),
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

        /* The description that the application passed is a structure; the arguments hold it as a chunk. It tells which kind of surface is created. */
        if (get_caps(surface_ptr,
                    &caps) )
        {
            if ( (caps & Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE) != 0)
            {
                /* The shown surface has no twin, but its back buffer will. */
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

    /* The description that the application passed is a structure; the arguments hold it as a chunk. It tells which kind of surface the application got. */
    if (attached_ptr == nullptr)
    {
        goto end;
    }

    {
        void* const known_twin_ptr = get_twin_ptr(attached_ptr);

        if (known_twin_ptr != nullptr)
        {
            goto end;
        }
    }

    {
        const bool has_caps = get_caps(attached_ptr,
                                      &caps);

        if (has_caps == false)
        {
            goto end;
        }
    }

    /* The back buffer that the primary surface returns is either the application's own chain or, in windowed mode, the converter's surface. */
    if (surface_ptr == m_primary_ptr)
    {
        /* The back buffer of the chain that the primary surface flips with. It is the surface the application renders to. */
        make_twin_ptr(attached_ptr,
                      true); /* in_is_back_buffer */

        m_back_buffer_ptr = attached_ptr;
    }
    else
    if ( (caps & Interception::Core::D3D7Utils::DDSCAPS_ZBUFFER) != 0)
    {
        void* const surface_twin_ptr = get_twin_ptr(surface_ptr);

        /* A depth buffer attached to a surface that has a twin is also attached to the twin. */
        if (surface_twin_ptr != nullptr)
        {
            Interception::Hooks::D3D7::IDirectDrawSurface7* attached_twin_ptr = nullptr;
            Interception::Hooks::D3D7::DDSCAPS2             depth_caps        = {};
            void**                                          vtable            = APIInterceptor::get_vtable_ptr(surface_twin_ptr);

            depth_caps.dwCaps = Interception::Core::D3D7Utils::DDSCAPS_ZBUFFER;

            if (SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE>(vtable,
                                                                                                                                Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE)(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(surface_twin_ptr),
                                                                                                                                                                                                               &depth_caps,
                                                                                                                                                                                                               &attached_twin_ptr) ))
            {
                void** const attached_vtable = APIInterceptor::get_vtable_ptr(attached_twin_ptr);

                m_twin_map[attached_ptr] = attached_twin_ptr;

                /* GetAttachedSurface returned a new reference. Release it, because the twin keeps the attachment itself. */
                APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_RELEASE>(attached_vtable,
                                                                                                           Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE)(attached_twin_ptr);
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

    if (in_returned_value_ptr        != nullptr                      &&
        (in_returned_value_ptr->type == APIInterceptor::ARGTYPE_U32  ||
         in_returned_value_ptr->type == APIInterceptor::ARGTYPE_I32) )
    {
        const uint32_t n_references = (in_returned_value_ptr->type == APIInterceptor::ARGTYPE_U32) ? in_returned_value_ptr->get_u32()
                                                                                                   : static_cast<uint32_t>         (in_returned_value_ptr->get_i32() );

        if (n_references == 0)
        {
            void* const surface_ptr   = const_cast<void*>(in_arguments_ptr[SURFACE_ARGUMENT].get_ptr() );
            const auto  twin_iterator = m_twin_map.find  (surface_ptr);

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
                void* const  twin_ptr = twin_iterator->second;
                void** const vtable   = APIInterceptor::get_vtable_ptr(twin_ptr);

                m_twin_map.erase(twin_iterator);

                /* Releases the reference that this class holds on the twin. Surfaces attached to the twin are released along with it. */
                APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_RELEASE>(vtable,
                                                                                                           Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_RELEASE)(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(twin_ptr) );
            }
        }
    }
}

bool Stereo::ConverterDLL::D3D7SurfaceTwins::has_succeeded(const APIInterceptor::APIFunctionArgument* in_returned_value_ptr)
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

bool Stereo::ConverterDLL::D3D7SurfaceTwins::is_back_buffer(void* in_surface_ptr) const
{
    return (in_surface_ptr != nullptr           &&
            in_surface_ptr == m_back_buffer_ptr);
}

void* Stereo::ConverterDLL::D3D7SurfaceTwins::make_twin_ptr(void*       in_surface_ptr,
                                                            const bool& in_is_back_buffer)
{
    /* Capabilities that make a surface part of a flip chain or the shown surface. The twin must not have them. */
    constexpr uint32_t CAPS_NOT_FOR_TWIN = Interception::Core::D3D7Utils::DDSCAPS_BACKBUFFER     |
                                           Interception::Core::D3D7Utils::DDSCAPS_COMPLEX        |
                                           Interception::Core::D3D7Utils::DDSCAPS_FLIP           |
                                           Interception::Core::D3D7Utils::DDSCAPS_FRONTBUFFER    |
                                           Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE;
    constexpr uint32_t FLAGS_FOR_TWIN    = Interception::Core::D3D7Utils::DDSD_CAPS              |
                                           Interception::Core::D3D7Utils::DDSD_HEIGHT            |
                                           Interception::Core::D3D7Utils::DDSD_MIPMAPCOUNT       |
                                           Interception::Core::D3D7Utils::DDSD_PIXELFORMAT       |
                                           Interception::Core::D3D7Utils::DDSD_WIDTH;

    Interception::Hooks::D3D7::DDSURFACEDESC2       description        = {};
    void** const                                    direct_draw_vtable = (m_direct_draw_ptr != nullptr) ? APIInterceptor::get_vtable_ptr(m_direct_draw_ptr)
                                                                                                        : nullptr;
    void*                                           result_ptr         = nullptr;
    void** const                                    surface_vtable     = APIInterceptor::get_vtable_ptr(in_surface_ptr);
    Interception::Hooks::D3D7::IDirectDrawSurface7* twin_ptr           = nullptr;

    description.dwSize = static_cast<DWORD>(sizeof(description) );

    if (m_direct_draw_ptr == nullptr)
    {
        goto end;
    }

    {
        const auto    get_description_func_ptr = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_GETSURFACEDESC>(surface_vtable,
                                                                                                                                                   Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_GETSURFACEDESC);
        const HRESULT get_description_result   = get_description_func_ptr                                                                         (static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(in_surface_ptr),
                                                                                                                                                   &description);

        if (FAILED(get_description_result) )
        {
            goto end;
        }
    }

    description.dwBackBufferCount = 0;
    description.dwFlags          &= FLAGS_FOR_TWIN;
    description.ddsCaps.dwCaps   &= ~CAPS_NOT_FOR_TWIN;

    /* A back buffer is an ordinary render-target surface, so its twin is created as one. */
    if (in_is_back_buffer)
    {
        description.ddsCaps.dwCaps |= Interception::Core::D3D7Utils::DDSCAPS_3DDEVICE |
                                      Interception::Core::D3D7Utils::DDSCAPS_OFFSCREENPLAIN;
    }

    if (FAILED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAW7_CREATESURFACE>(direct_draw_vtable,
                                                                                                         Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAW7_CREATESURFACE)(static_cast<Interception::Hooks::D3D7::IDirectDraw7*>(m_direct_draw_ptr),
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

    /* A failed call changed nothing, so it is skipped. Release is always handled, because its return value is a reference count, not a success code. */
    const bool did_succeed  = has_succeeded(in_returned_value_ptr);
    const bool is_to_handle = (did_succeed                                                                  ||
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
