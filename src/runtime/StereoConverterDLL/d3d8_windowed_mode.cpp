/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Common/vtable.h"
#include "d3d8_windowed_mode.h"

Stereo::ConverterDLL::D3D8WindowedMode::D3D8WindowedMode()
{
    Interception::Hooks::D3D8::add_argument_modifier(this);
}

Stereo::ConverterDLL::D3D8WindowedMode::~D3D8WindowedMode()
{
    Interception::Hooks::D3D8::remove_argument_modifier(this);
}

void Stereo::ConverterDLL::D3D8WindowedMode::keep_windows_invisible()
{
    m_invisible_windows.keep_invisible();
}

void Stereo::ConverterDLL::D3D8WindowedMode::on_create_device(Interception::Hooks::D3D8::IDirect3D8*            in_direct3d_ptr,
                                                              UINT                                              in_adapter,
                                                              HWND                                              in_focus_window,
                                                              Interception::Hooks::D3D8::D3DPRESENT_PARAMETERS* inout_parameters_ptr)
{
    Interception::Hooks::D3D8::D3DDISPLAYMODE display_mode = {};
    void** const                              vtable       = APIInterceptor::get_vtable_ptr(in_direct3d_ptr);
    const HWND                                window       = (inout_parameters_ptr->hDeviceWindow != nullptr) ? inout_parameters_ptr->hDeviceWindow
                                                                                        : in_focus_window;

    const auto get_display_mode_func = APIInterceptor::get_method_ptr<Interception::Hooks::D3D8::PFN_IDIRECT3D8_GETADAPTERDISPLAYMODE>(vtable, Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3D8_GETADAPTERDISPLAYMODE);

    /* A window shows the format of the desktop. */
    if (SUCCEEDED(get_display_mode_func(in_direct3d_ptr,
                                        in_adapter,
                                       &display_mode) ))
    {
        inout_parameters_ptr->BackBufferFormat = display_mode.Format;
    }

    /* Only a swap effect that discards can have samples; a window has no flip, and the refresh rate and the interval are for the
     * whole screen.
     */
    inout_parameters_ptr->BackBufferCount                  = 1;
    inout_parameters_ptr->FullScreen_PresentationInterval  = Interception::Hooks::D3D8::D3DPRESENT_INTERVAL_DEFAULT;
    inout_parameters_ptr->FullScreen_RefreshRateInHz       = 0;
    inout_parameters_ptr->SwapEffect                       = (inout_parameters_ptr->MultiSampleType != Interception::Hooks::D3D8::D3DMULTISAMPLE_NONE) ? Interception::Hooks::D3D8::D3DSWAPEFFECT_DISCARD
                                                                                                                                                       : Interception::Hooks::D3D8::D3DSWAPEFFECT_COPY;
    inout_parameters_ptr->Windowed                         = TRUE;

    m_invisible_windows.add(window);
    m_invisible_windows.add(in_focus_window);
}

