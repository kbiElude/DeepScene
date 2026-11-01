/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d8_windowed_mode.h"

Stereo::StereoConverterDLL::D3D8WindowedMode::D3D8WindowedMode()
{
    Interception::Hooks::D3D8::add_argument_modifier(this);
}

Stereo::StereoConverterDLL::D3D8WindowedMode::~D3D8WindowedMode()
{
    Interception::Hooks::D3D8::remove_argument_modifier(this);
}

void Stereo::StereoConverterDLL::D3D8WindowedMode::keep_windows_invisible()
{
    for (const auto& window : m_window_set)
    {
        make_invisible(window);
    }
}

void Stereo::StereoConverterDLL::D3D8WindowedMode::on_create_device(Interception::Hooks::D3D8::IDirect3D8*            in_direct3d_ptr,
                                                                    UINT                                              in_adapter,
                                                                    HWND                                              in_focus_window,
                                                                    Interception::Hooks::D3D8::D3DPRESENT_PARAMETERS* inout_parameters_ptr)
{
    Interception::Hooks::D3D8::D3DDISPLAYMODE display_mode = {};
    void** const                              vtable       = *reinterpret_cast<void***>(in_direct3d_ptr);
    const HWND                                window       = (inout_parameters_ptr->hDeviceWindow != nullptr) ? inout_parameters_ptr->hDeviceWindow
                                                                                        : in_focus_window;

    const auto get_display_mode_func = reinterpret_cast<Interception::Hooks::D3D8::PFN_IDIRECT3D8_GETADAPTERDISPLAYMODE>(vtable[Interception::Hooks::D3D8::VTABLE_INDEX_IDIRECT3D8_GETADAPTERDISPLAYMODE]);

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

    for (const HWND& named_window : {window, in_focus_window})
    {
        if (named_window != nullptr)
        {
            m_window_set.insert(named_window);

            make_invisible(named_window);
        }
    }
}

void Stereo::StereoConverterDLL::D3D8WindowedMode::make_invisible(const HWND& in_window)
{
    const LONG_PTR style = ::GetWindowLongPtrW(in_window,
                                               GWL_EXSTYLE);
    const LONG_PTR wanted = style | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT;

    /* Said again only when the application has taken it off. */
    if (style != wanted)
    {
        ::SetWindowLongPtrW         (in_window,
                                     GWL_EXSTYLE,
                                     wanted);
        ::SetLayeredWindowAttributes(in_window,
                                     0, /* crKey */
                                     0, /* bAlpha */
                                     LWA_ALPHA);
    }
}
