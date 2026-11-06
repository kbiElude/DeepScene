/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_INVISIBLE_WINDOWS_H
#define STEREO_CONVERTERDLL_INVISIBLE_WINDOWS_H

#include <map>
#include <mutex>
#include <set>
#include <Windows.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The windows of an application which are made invisible, so that its images are seen where the converter shows them and not on the
         * screen of the application. A window is made fully transparent and click-through, and kept so, and not hidden: an application may
         * pause while its window is hidden (fr-08 does), and may show or style its window after it makes its device.
         *
         * An invisible window is never the one in front, and an application may pause while its window is not the active one (Kasparov does, and
         * waits for the activation for good if the window of the program which shows its images came to the front first): the window is told to the
         * application as active, always (see handle_message()).
         */
        class InvisibleWindows
        {
        public:
            /* Makes the window invisible, and keeps it so (see keep_invisible()); the application is told that its window is active, and that it
             * is not made inactive.
             */
            void add(const HWND& in_window);

            /* Makes the windows invisible again if the application has changed their styles. */
            void keep_invisible();

        private:
            /* What is known of the windows that are not to be made inactive: their procedures, as the application set them. */
            struct Subclasses
            {
                std::mutex                mutex;
                std::map<HWND, WNDPROC>   procedure_map;
            };

            static Subclasses& get_subclasses();

            /* The procedure of the window, in front of the one of the application: a message that says the window is no longer the active one (or the
             * application) is passed on as one that says it is.
             */
            static LRESULT CALLBACK handle_message(HWND   in_window,
                                                   UINT   in_message,
                                                   WPARAM in_w_param,
                                                   LPARAM in_l_param);

            static void make_invisible(const HWND& in_window);

            /* Puts handle_message() in front of the procedure of the window, once, and says the application that the window is active. */
            static void keep_active(const HWND& in_window);

            std::set<HWND> m_window_set;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_INVISIBLE_WINDOWS_H */
