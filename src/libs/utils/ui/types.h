/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_UI_TYPES_H
#define UTILS_UI_TYPES_H

#include <string>

namespace Utils
{
    namespace UI
    {
        /* The bottom right corner of everything a window has drawn, in the pixels of the window. */
        struct ContentExtent
        {
            float bottom = 0.0f;
            float right  = 0.0f;
        };

        struct WindowOptions
        {
            std::wstring class_name;                    /* window class name: one for each kind of window in a process         */
            int          client_height = 0;             /* at 100% scale; the monitor scale is applied on top                  */
            int          client_width  = 0;
            bool         fits_content  = false;         /* the window is sized to fit what it draws                            */
            bool         has_custom_title_bar = false;  /* no title bar of the system: the window draws its own and says where it is (see set_title_bar_layout) */
            bool         is_resizable  = false;
            bool         is_topmost    = false;
            std::string  title;
            bool         uses_system_font = false;      /* use Segoe UI, if the machine has it, instead of the ImGui font      */
        };
    }
}

#endif /* UTILS_UI_TYPES_H */
