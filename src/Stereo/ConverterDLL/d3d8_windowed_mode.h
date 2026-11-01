/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_D3D8_WINDOWED_MODE_H
#define STEREO_CONVERTERDLL_D3D8_WINDOWED_MODE_H

#include "Interception/Hooks/D3D8/argument_modifier.h"
#include <set>
#include <Windows.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Has the application render to a window, and makes the window invisible, so that the images of the application are seen where the converter
         * shows them and not on the screen of the application: an application which asks for the whole screen would otherwise have it, in
         * the display mode it asks for.
         *
         * The parameters of the device that the application asks for are changed as it creates it: a window, the format of the desktop
         * for the back buffer, one back buffer and a swap effect which a window has. The sizes of the back buffer are the application's.
         * The windows it names are made fully transparent and click-through, and kept so, and not hidden: an application may pause while its
         * window is hidden (fr-08 does), and may show or style its window after it creates its device.
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
            static void make_invisible(const HWND& in_window);

            std::set<HWND> m_window_set;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_D3D8_WINDOWED_MODE_H */
