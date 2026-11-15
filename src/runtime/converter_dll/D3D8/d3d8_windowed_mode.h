/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D8_WINDOWED_MODE_H
#define STEREO_CONVERTERDLL_D3D8_WINDOWED_MODE_H

#include "interception/hooks/D3D8/argument_modifier.h"
#include "invisible_windows.h"
#include <Windows.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Makes a Direct3D 8 application render to a window, and makes that window invisible. The images of the application then appear where
         * the converter shows them, not on the screen of the application. Without this, an application that asks for the whole screen would
         * take it, in the display mode it asks for.
         *
         * The device parameters that the application passes when it creates the device are changed: a window, the desktop format for the back
         * buffer, one back buffer, and a swap effect that a window supports. The back buffer sizes stay the application's. The windows the
         * application names are made fully transparent and click-through. They are not hidden, because an application may pause while its
         * window is hidden (fr-08 does), and may show or restyle its window after it creates the device.
         */
        class D3D8WindowedMode : public Interception::Hooks::D3D8::IArgumentModifier
        {
        public:
            D3D8WindowedMode();
            ~D3D8WindowedMode() override;

            D3D8WindowedMode(const D3D8WindowedMode&)            = delete;
            D3D8WindowedMode& operator=(const D3D8WindowedMode&) = delete;

            /* Makes the windows invisible again if the application has changed their styles. */
            void keep_windows_invisible();

            void on_create_device(Interception::Hooks::D3D8::IDirect3D8*            in_direct3d_ptr,
                                  UINT                                              in_adapter,
                                  HWND                                              in_focus_window,
                                  Interception::Hooks::D3D8::D3DPRESENT_PARAMETERS* inout_parameters_ptr) override;

        private:
            InvisibleWindows m_invisible_windows;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D8_WINDOWED_MODE_H */
