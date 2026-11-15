/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_LAUNCHER_BUTTON_CLICKER_H
#define STEREO_LAUNCHER_BUTTON_CLICKER_H

#include <string>
#include <Windows.h>

namespace Stereo
{
    namespace Launcher
    {
        /* Presses a button in a window that an application shows before it starts, such as the "Launch" button of a demo's setup dialog, so that
         * the user does not have to.
         */
        class ButtonClicker
        {
        public:
            /* Searches, until the process exits or the timeout expires, for a button in a window of the process whose text matches in_button_text
             * (ignoring '&' and case), and presses it. Returns false if no such button was found.
             */
            static bool click_when_shown(const HANDLE&      in_process,
                                         const DWORD&       in_process_id,
                                         const std::string& in_button_text,
                                         const DWORD&       in_timeout_milliseconds);

            /* Returns the text in lower case without '&' (the mark of the keyboard shortcut of a button). */
            static std::string get_normalized_text(const std::string& in_text);

        private:
            /* What the window enumeration searches for, and what it found. */
            struct Search
            {
                HWND        button      = nullptr;
                std::string button_text; /* normalized */
                DWORD       process_id  = 0;
            };

            static bool click_once(const DWORD&       in_process_id,
                                   const std::string& in_button_text);

            static BOOL CALLBACK on_child_window    (HWND   in_window,
                                                     LPARAM in_search_ptr);
            static BOOL CALLBACK on_top_level_window(HWND   in_window,
                                                     LPARAM in_search_ptr);
        };
    }
}

#endif /* STEREO_LAUNCHER_BUTTON_CLICKER_H */
