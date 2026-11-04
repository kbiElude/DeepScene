/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_INVISIBLE_WINDOWS_H
#define STEREO_CONVERTERDLL_INVISIBLE_WINDOWS_H

#include <set>
#include <Windows.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The windows of an application which are made invisible, so that its images are seen where the converter shows them and not on the
         * screen of the application. A window is made fully transparent and click-through, and kept so, and not hidden: an application may
         * pause while its window is hidden (fr-08 does), and may show or style its window after it makes its device.
         */
        class InvisibleWindows
        {
        public:
            /* Makes the window invisible, and keeps it so (see keep_invisible()). */
            void add(const HWND& in_window);

            /* Makes the windows invisible again if the application has changed their styles. */
            void keep_invisible();

        private:
            static void make_invisible(const HWND& in_window);

            std::set<HWND> m_window_set;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_INVISIBLE_WINDOWS_H */
