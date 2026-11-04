/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "invisible_windows.h"

void Stereo::ConverterDLL::InvisibleWindows::add(const HWND& in_window)
{
    if (in_window != nullptr)
    {
        m_window_set.insert(in_window);

        make_invisible(in_window);
    }
}

void Stereo::ConverterDLL::InvisibleWindows::keep_invisible()
{
    for (const auto& window : m_window_set)
    {
        make_invisible(window);
    }
}

void Stereo::ConverterDLL::InvisibleWindows::make_invisible(const HWND& in_window)
{
    const LONG_PTR style  = ::GetWindowLongPtrW(in_window,
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
