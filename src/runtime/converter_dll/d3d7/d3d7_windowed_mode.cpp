/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Common/argument_reader.h"
#include "Common/callbacks.h"
#include "Common/vtable.h"
#include "Interception/Core/D3D7Utils/constants.h"
#include "Interception/Hooks/D3D7/types.h"
#include "d3d7_windowed_mode.h"
#include "eye_image_limits.h"
#include "own_call_scope.h"
#include <Windows.h>

Stereo::ConverterDLL::D3D7WindowedMode::D3D7WindowedMode()
    :m_back_buffer_ptr(nullptr),
     m_display_height (0),
     m_display_width  (0),
     m_is_enabled     (false),
     m_policy         (1.0f),
     m_primary_ptr    (nullptr),
     m_target_height  (0),
     m_target_width   (0)
{
    /* Stub */
}

void* Stereo::ConverterDLL::D3D7WindowedMode::get_back_buffer_ptr() const
{
    return m_back_buffer_ptr;
}

void Stereo::ConverterDLL::D3D7WindowedMode::get_display_size(uint32_t* out_width_ptr,
                                                              uint32_t* out_height_ptr) const
{
    *out_height_ptr = m_display_height;
    *out_width_ptr  = m_display_width;
}

float Stereo::ConverterDLL::D3D7WindowedMode::get_render_scale() const
{
    return m_policy.get_scale();
}

uint32_t Stereo::ConverterDLL::D3D7WindowedMode::get_scaled_extent(const uint32_t& in_extent) const
{
    return m_policy.scale_extent(in_extent);
}

void Stereo::ConverterDLL::D3D7WindowedMode::handle_create_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                                   bool*                                      out_should_pass_through_ptr)
{
    constexpr size_t DESCRIPTION_ARGUMENT = 1;
    constexpr size_t DIRECT_DRAW_ARGUMENT = 0;
    constexpr size_t SURFACE_ARGUMENT     = 2;

    Interception::Hooks::D3D7::DDSURFACEDESC2       back_buffer_description = {};
    const void*                                     chunk_ptr               = nullptr;
    void* const                                     direct_draw_ptr         = const_cast<void*>(in_arguments_ptr[DIRECT_DRAW_ARGUMENT].get_ptr() );
    uint32_t                                        n_chunk_bytes           = 0;
    Interception::Hooks::D3D7::DDSURFACEDESC2       primary_description     = {};
    Interception::Hooks::D3D7::IDirectDrawSurface7* back_buffer_ptr         = nullptr;
    Interception::Hooks::D3D7::IDirectDrawSurface7* primary_ptr             = nullptr;

    if (in_arguments_ptr[DESCRIPTION_ARGUMENT].type != APIInterceptor::ARGTYPE_DATA_CHUNK_ID                                      ||
        in_arguments_ptr[SURFACE_ARGUMENT].type     != APIInterceptor::ARGTYPE_VOID_PTR                                           ||
        APIInterceptor::get_data_chunk(in_arguments_ptr[DESCRIPTION_ARGUMENT].get_data_chunk_id(),
                                      &chunk_ptr,
                                      &n_chunk_bytes) == false                                                                    ||
        n_chunk_bytes != sizeof(Interception::Hooks::D3D7::DDSURFACEDESC2) )
    {
        goto end;
    }

    {
        const auto* description_ptr = static_cast<const Interception::Hooks::D3D7::DDSURFACEDESC2*>(chunk_ptr);

        /* Only the surface that is shown, with the chain of buffers that it is flipped with, is not one a window can have. */
        if ( (description_ptr->ddsCaps.dwCaps & Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE) == 0 ||
             (description_ptr->ddsCaps.dwCaps & Interception::Core::D3D7Utils::DDSCAPS_FLIP)           == 0)
        {
            /* A surface that is rendered to, of the size of the images of the application (a depth buffer for the back buffer), is made as large as the
             * back buffer is.
             */
            handle_sized_surface(*description_ptr,
                                 direct_draw_ptr,
                                 static_cast<void**>(const_cast<void*>(in_arguments_ptr[SURFACE_ARGUMENT].get_ptr() )),
                                 out_should_pass_through_ptr);

            goto end;
        }
    }

    {
        const OwnCallScope own_call_scope;
        void** const       vtable = APIInterceptor::get_vtable_ptr(direct_draw_ptr);

        const auto create_surface_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAW7_CREATESURFACE>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAW7_CREATESURFACE);

        /* The surface that is shown is the one of the window. */
        primary_description.dwSize      = static_cast<DWORD>(sizeof(primary_description) );
        primary_description.dwFlags     = Interception::Core::D3D7Utils::DDSD_CAPS;
        primary_description.ddsCaps.dwCaps = Interception::Core::D3D7Utils::DDSCAPS_PRIMARYSURFACE;

        /* The back buffer is a surface of its own that is rendered to: of the size of the display mode that was asked for, and of the format
         * of the desktop, which is the one of the primary surface.
         */
        back_buffer_description.dwSize         = static_cast<DWORD>(sizeof(back_buffer_description) );
        back_buffer_description.dwFlags        = Interception::Core::D3D7Utils::DDSD_CAPS   |
                                                 Interception::Core::D3D7Utils::DDSD_HEIGHT |
                                                 Interception::Core::D3D7Utils::DDSD_WIDTH;
        back_buffer_description.dwHeight       = (m_display_height != 0) ? get_scaled_extent(m_display_height)
                                                                         : static_cast<uint32_t>(::GetSystemMetrics(SM_CYSCREEN) );
        back_buffer_description.dwWidth        = (m_display_width  != 0) ? get_scaled_extent(m_display_width)
                                                                         : static_cast<uint32_t>(::GetSystemMetrics(SM_CXSCREEN) );
        back_buffer_description.ddsCaps.dwCaps = Interception::Core::D3D7Utils::DDSCAPS_3DDEVICE |
                                                 Interception::Core::D3D7Utils::DDSCAPS_OFFSCREENPLAIN;

        if (SUCCEEDED(create_surface_func(static_cast<Interception::Hooks::D3D7::IDirectDraw7*>(direct_draw_ptr),
                                          &primary_description,
                                          &primary_ptr,
                                           nullptr) )                                              &&
            SUCCEEDED(create_surface_func(static_cast<Interception::Hooks::D3D7::IDirectDraw7*>(direct_draw_ptr),
                                          &back_buffer_description,
                                          &back_buffer_ptr,
                                           nullptr) ))
        {
            m_back_buffer_ptr = back_buffer_ptr;
            m_primary_ptr     = primary_ptr;

            /* What the application is to get is where it asked for it. */
            *static_cast<void**>(const_cast<void*>(in_arguments_ptr[SURFACE_ARGUMENT].get_ptr() )) = primary_ptr;

            *out_should_pass_through_ptr = false;
        }
    }

end:
    return;
}

void Stereo::ConverterDLL::D3D7WindowedMode::handle_get_attached_surface(const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                                         bool*                                      out_should_pass_through_ptr)
{
    constexpr size_t CAPS_ARGUMENT    = 1;
    constexpr size_t RESULT_ARGUMENT  = 2;
    constexpr size_t SURFACE_ARGUMENT = 0;

    const void* chunk_ptr     = nullptr;
    uint32_t    n_chunk_bytes = 0;

    if (m_back_buffer_ptr                                               == nullptr                         ||
        in_arguments_ptr[SURFACE_ARGUMENT].get_ptr()                    != m_primary_ptr                   ||
        in_arguments_ptr[CAPS_ARGUMENT].type                            != APIInterceptor::ARGTYPE_DATA_CHUNK_ID ||
        in_arguments_ptr[RESULT_ARGUMENT].type                          != APIInterceptor::ARGTYPE_VOID_PTR      ||
        APIInterceptor::get_data_chunk(in_arguments_ptr[CAPS_ARGUMENT].get_data_chunk_id(),
                                      &chunk_ptr,
                                      &n_chunk_bytes) == false                                              ||
        n_chunk_bytes != sizeof(Interception::Hooks::D3D7::DDSCAPS2) )
    {
        goto end;
    }

    /* The back buffer of the chain is the surface of the window's own. */
    if ( (static_cast<const Interception::Hooks::D3D7::DDSCAPS2*>(chunk_ptr)->dwCaps & Interception::Core::D3D7Utils::DDSCAPS_BACKBUFFER) != 0)
    {
        const OwnCallScope own_call_scope;
        void** const       vtable = APIInterceptor::get_vtable_ptr(m_back_buffer_ptr);

        /* What a call that gives a surface gives is a reference to it. */
        APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAWSURFACE7_ADDREF>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAWSURFACE7_ADDREF)(static_cast<Interception::Hooks::D3D7::IDirectDrawSurface7*>(m_back_buffer_ptr) );

        *static_cast<void**>(const_cast<void*>(in_arguments_ptr[RESULT_ARGUMENT].get_ptr() )) = m_back_buffer_ptr;

        *out_should_pass_through_ptr = false;
    }

end:
    return;
}

void Stereo::ConverterDLL::D3D7WindowedMode::handle_set_cooperative_level(const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t COOPERATIVE_LEVEL_NORMAL = 0x00000008; /* DDSCL_NORMAL */
    constexpr size_t   DIRECT_DRAW_ARGUMENT     = 0;
    constexpr size_t   WINDOW_ARGUMENT          = 1;

    const OwnCallScope own_call_scope;
    void* const        direct_draw_ptr = const_cast<void*>(in_arguments_ptr[DIRECT_DRAW_ARGUMENT].get_ptr() );
    void** const       vtable          = APIInterceptor::get_vtable_ptr(direct_draw_ptr);
    HWND               window          = nullptr;

    /* A handle is a pointer, or the number of one. */
    if (in_arguments_ptr[WINDOW_ARGUMENT].type == APIInterceptor::ARGTYPE_VOID_PTR)
    {
        window = reinterpret_cast<HWND>(const_cast<void*>(in_arguments_ptr[WINDOW_ARGUMENT].get_ptr() ));
    }
    else
    if (in_arguments_ptr[WINDOW_ARGUMENT].type == APIInterceptor::ARGTYPE_U64)
    {
        window = reinterpret_cast<HWND>(static_cast<uintptr_t>(in_arguments_ptr[WINDOW_ARGUMENT].get_u64() ));
    }

    m_invisible_windows.add(window);

    APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAW7_SETCOOPERATIVELEVEL>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAW7_SETCOOPERATIVELEVEL)(static_cast<Interception::Hooks::D3D7::IDirectDraw7*>(direct_draw_ptr),
                                                                                                                                                                         window,
                                                                                                                                                                         COOPERATIVE_LEVEL_NORMAL);
}

void Stereo::ConverterDLL::D3D7WindowedMode::handle_set_display_mode(const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    /* The largest side of what Direct3D 7 renders to: a surface wider or higher than this is not made on the hardware that was tried (a size that is
     * a pixel more than 2048 fails, and the application stops), whatever the headset could show.
     */
    constexpr uint32_t MAX_RENDER_TARGET_EXTENT = 2048;
    constexpr size_t   HEIGHT_ARGUMENT          = 2;
    constexpr size_t   WIDTH_ARGUMENT           = 1;

    uint32_t height = 0;
    uint32_t width  = 0;

    /* The size is the one of the images of the application, which is what the back buffer is made as large as. */
    if (APIInterceptor::read_number(in_arguments_ptr[WIDTH_ARGUMENT],
                &width)  &&
        APIInterceptor::read_number(in_arguments_ptr[HEIGHT_ARGUMENT],
                &height))
    {
        m_display_height = height;
        m_display_width  = width;

        /* The back buffer is made as large as the images that are shown allow, with the proportions of the display mode; not larger than the channel
         * holds, for the two eyes side by side.
         */
        if (m_target_height != 0 &&
            m_target_width  != 0)
        {
            const float channel_scale = EyeImageLimits::get_largest_scale(width,
                                                                          height);
            const float fit_scale     = Stereo::Core::ResolutionPolicy::get_fit_scale(m_target_width,
                                                                                      m_target_height,
                                                                                      width,
                                                                                      height);
            const float surface_scale = Stereo::Core::ResolutionPolicy::get_fit_scale(MAX_RENDER_TARGET_EXTENT,
                                                                                      MAX_RENDER_TARGET_EXTENT,
                                                                                      width,
                                                                                      height);
            const float smaller_scale = (fit_scale < channel_scale) ? fit_scale
                                                                    : channel_scale;

            m_policy = Stereo::Core::ResolutionPolicy((surface_scale < smaller_scale) ? surface_scale
                                                                                      : smaller_scale);
        }
    }
}

void Stereo::ConverterDLL::D3D7WindowedMode::handle_sized_surface(const Interception::Hooks::D3D7::DDSURFACEDESC2& in_description,
                                                                  void*                                            in_direct_draw_ptr,
                                                                  void**                                           out_surface_ptr_ptr,
                                                                  bool*                                            out_should_pass_through_ptr)
{
    constexpr uint32_t CAPS_OF_SURFACES_RENDERED_TO = Interception::Core::D3D7Utils::DDSCAPS_3DDEVICE |
                                                      Interception::Core::D3D7Utils::DDSCAPS_ZBUFFER;
    constexpr uint32_t FLAGS_OF_SIZE                = Interception::Core::D3D7Utils::DDSD_HEIGHT |
                                                      Interception::Core::D3D7Utils::DDSD_WIDTH;

    Interception::Hooks::D3D7::DDSURFACEDESC2       scaled_description = in_description;
    Interception::Hooks::D3D7::IDirectDrawSurface7* surface_ptr        = nullptr;

    /* Only a surface that is as large as the images of the application is, and only if they are made larger. */
    if (m_policy.is_identity()                                                                     ||
        m_display_width == 0                                                                       ||
        (in_description.ddsCaps.dwCaps & CAPS_OF_SURFACES_RENDERED_TO) == 0                        ||
        (in_description.dwFlags & FLAGS_OF_SIZE)                       != FLAGS_OF_SIZE            ||
        in_description.dwHeight != m_display_height                                                ||
        in_description.dwWidth  != m_display_width)
    {
        goto end;
    }

    {
        const OwnCallScope own_call_scope;
        void** const       vtable = APIInterceptor::get_vtable_ptr(in_direct_draw_ptr);

        scaled_description.dwHeight = get_scaled_extent(in_description.dwHeight);
        scaled_description.dwWidth  = get_scaled_extent(in_description.dwWidth);

        if (SUCCEEDED(APIInterceptor::get_method_ptr<Interception::Hooks::D3D7::PFN_IDIRECTDRAW7_CREATESURFACE>(vtable, Interception::Hooks::D3D7::VTABLE_INDEX_IDIRECTDRAW7_CREATESURFACE)(static_cast<Interception::Hooks::D3D7::IDirectDraw7*>(in_direct_draw_ptr),
                                                                                                                                                                              &scaled_description,
                                                                                                                                                                              &surface_ptr,
                                                                                                                                                                               nullptr) ))
        {
            *out_surface_ptr_ptr         = surface_ptr;
            *out_should_pass_through_ptr = false;
        }
    }

end:
    return;
}

void Stereo::ConverterDLL::D3D7WindowedMode::keep_windows_invisible()
{
    m_invisible_windows.keep_invisible();
}

bool Stereo::ConverterDLL::D3D7WindowedMode::on_pre(const APIInterceptor::APIFunction&         in_function,
                                                    const uint32_t&                            in_n_arguments,
                                                    const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    constexpr uint32_t N_ARGUMENTS_CREATE_SURFACE        = 3;
    constexpr uint32_t N_ARGUMENTS_FLIP                  = 1;
    constexpr uint32_t N_ARGUMENTS_GET_ATTACHED_SURFACE  = 3;
    constexpr uint32_t N_ARGUMENTS_SET_COOPERATIVE_LEVEL = 3;
    constexpr uint32_t N_ARGUMENTS_SET_DISPLAY_MODE      = 3;

    bool result = true;

    if (m_is_enabled == false)
    {
        goto end;
    }

    switch (in_function)
    {
        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_CREATESURFACE:
        {
            if (in_n_arguments >= N_ARGUMENTS_CREATE_SURFACE)
            {
                handle_create_surface(in_arguments_ptr,
                                     &result);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETCOOPERATIVELEVEL:
        {
            if (in_n_arguments >= N_ARGUMENTS_SET_COOPERATIVE_LEVEL)
            {
                handle_set_cooperative_level(in_arguments_ptr);

                result = false;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAW7_SETDISPLAYMODE:
        {
            /* The desktop keeps its mode. */
            if (in_n_arguments >= N_ARGUMENTS_SET_DISPLAY_MODE)
            {
                handle_set_display_mode(in_arguments_ptr);

                result = false;
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_GETATTACHEDSURFACE:
        {
            if (in_n_arguments >= N_ARGUMENTS_GET_ATTACHED_SURFACE)
            {
                handle_get_attached_surface(in_arguments_ptr,
                                           &result);
            }

            break;
        }

        case APIInterceptor::APIFUNCTION_D3D7_IDIRECTDRAWSURFACE7_FLIP:
        {
            /* A window has no chain to flip: the images are taken from the back buffer by the converter, and nothing is shown. */
            if (in_n_arguments >= N_ARGUMENTS_FLIP                                &&
                in_arguments_ptr[0].get_ptr() == m_primary_ptr                    &&
                m_primary_ptr                 != nullptr)
            {
                result = false;
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

void Stereo::ConverterDLL::D3D7WindowedMode::set_is_enabled(const bool& in_is_enabled)
{
    m_is_enabled = in_is_enabled;
}

void Stereo::ConverterDLL::D3D7WindowedMode::set_target_eye_size(const uint32_t& in_width,
                                                                 const uint32_t& in_height)
{
    m_target_height = in_height;
    m_target_width  = in_width;
}

