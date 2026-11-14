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
        /* Presses the button of a window that an application shows before it starts (the "Launch" of a demo's setup dialog), so that
         * nobody has to.
         */
        class ButtonClicker
        {
        public:
            /* Looks, until the process exits or the time runs out, for a button of a window of the process whose text is the one given
             * (without its '&', not looking at the case), and presses it. Returns false if it was not found.
             */
            static bool click_when_shown(const HANDLE&      in_process,
                                         const DWORD&       in_process_id,
                                         const std::string& in_button_text,
                                         const DWORD&       in_timeout_milliseconds);

            /* The text without its '&' (the mark of the key of a button), in lower case. */
            static std::string get_normalized_text(const std::string& in_text);

        private:
            /* What the enumeration of the windows looks for, and what it found. */
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
