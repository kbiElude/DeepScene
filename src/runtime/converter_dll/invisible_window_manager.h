/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_INVISIBLE_WINDOW_MANAGER_H
#define STEREO_CONVERTERDLL_INVISIBLE_WINDOW_MANAGER_H

#include <map>
#include <mutex>
#include <set>
#include <Windows.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Makes the windows of an application invisible, so that its images appear only where the converter shows them and not on the application's own
         * screen. A window is made fully transparent and click-through. It is not hidden, because an application may pause while its window is hidden
         * and may show or restyle its window after it creates its device.
         *
         * An invisible window is never the foreground window, and an application may pause while its window is not active (some do, and wait
         * forever for the activation if the window of the viewer comes to the front first). So the application is always told that its window is
         * active (see handle_message()).
         */
        class InvisibleWindowManager
        {
        public:
            /* Makes the window invisible and keeps it invisible (see keep_invisible()). The application is told that its window is active, and it is
             * never told that the window became inactive.
             */
            void add(const HWND& in_window);

            /* Makes the windows invisible again if the application has changed their styles. */
            void keep_invisible();

        private:
            /* The windows that are kept active: the window procedures that the application set for them. */
            struct Subclasses
            {
                std::mutex              mutex;
                std::map<HWND, WNDPROC> procedure_map;
            };

            static Subclasses& get_subclasses();

            /* The window procedure that runs before the application's procedure. Messages that say the window or the application became inactive are
             * changed into messages that say it became active.
             */
            static LRESULT CALLBACK handle_message(HWND   in_window,
                                                   UINT   in_message,
                                                   WPARAM in_w_param,
                                                   LPARAM in_l_param);

            static void make_invisible(const HWND& in_window);

            /* Installs handle_message() in front of the window procedure of the window, once, and tells the application that the window is active. */
            static void keep_active(const HWND& in_window);

            std::set<HWND> m_window_set;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_INVISIBLE_WINDOW_MANAGER_H */
