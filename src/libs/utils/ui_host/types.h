/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_UIHOST_TYPES_H
#define UTILS_UIHOST_TYPES_H

#include <string>

namespace Utils
{
    namespace UIHost
    {
        /* The lower right corner of what a window has drawn, in the pixels of the window. */
        struct ContentExtent
        {
            float bottom = 0.0f;
            float right  = 0.0f;
        };

        struct WindowOptions
        {
            std::wstring class_name;                    /* of the window class: one for each kind of window of a process       */
            int          client_height = 0;             /* at 100% scale; the scale of the monitor is applied                  */
            int          client_width  = 0;
            bool         fits_content  = false;         /* the window takes the size of what it draws                          */
            bool         is_resizable  = false;
            bool         is_topmost    = false;
            std::string  title;
            bool         uses_system_font = false;      /* Segoe UI, if the machine has it, in place of the font of ImGui      */
        };
    }
}

#endif /* UTILS_UIHOST_TYPES_H */
