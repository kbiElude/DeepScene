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
        keep_active   (in_window);
    }
}

Stereo::ConverterDLL::InvisibleWindows::Subclasses& Stereo::ConverterDLL::InvisibleWindows::get_subclasses()
{
    static Subclasses result;

    return result;
}

LRESULT CALLBACK Stereo::ConverterDLL::InvisibleWindows::handle_message(HWND   in_window,
                                                                        UINT   in_message,
                                                                        WPARAM in_w_param,
                                                                        LPARAM in_l_param)
{
    LRESULT result    = 0;
    WNDPROC procedure = nullptr;
    WPARAM  w_param   = in_w_param;

    {
        auto&                       subclasses             = get_subclasses();
        std::lock_guard<std::mutex> lock(subclasses.mutex);
        const auto                  iterator               = subclasses.procedure_map.find(in_window);

        if (iterator != subclasses.procedure_map.end() )
        {
            procedure = iterator->second;
        }
    }

    if (procedure == nullptr)
    {
        result = ::DefWindowProcW(in_window,
                                  in_message,
                                  in_w_param,
                                  in_l_param);

        goto end;
    }

    switch (in_message)
    {
        case WM_ACTIVATE:
        {
            /* The low word says how, and the high one if the window is minimized: the window is not. */
            w_param = WA_ACTIVE;

            break;
        }

        case WM_ACTIVATEAPP:
        case WM_NCACTIVATE:
        {
            w_param = TRUE;

            break;
        }

        case WM_KILLFOCUS:
        {
            /* An application which pauses when it loses the focus is not to know that it does. */
            goto end;
        }

        case WM_NCDESTROY:
        {
            auto&                       subclasses = get_subclasses();
            std::lock_guard<std::mutex> lock(subclasses.mutex);

            subclasses.procedure_map.erase(in_window);

            break;
        }

        default:
        {
            break;
        }
    }

    result = ::CallWindowProcW(procedure,
                               in_window,
                               in_message,
                               w_param,
                               in_l_param);

end:
    return result;
}

void Stereo::ConverterDLL::InvisibleWindows::keep_active(const HWND& in_window)
{
    auto& subclasses = get_subclasses();
    bool  is_new     = false;

    {
        std::lock_guard<std::mutex> lock(subclasses.mutex);

        if (subclasses.procedure_map.count(in_window) == 0)
        {
            const WNDPROC procedure = reinterpret_cast<WNDPROC>(::GetWindowLongPtrW(in_window,
                                                                                    GWLP_WNDPROC) );

            if (procedure != nullptr)
            {
                subclasses.procedure_map[in_window] = procedure;

                is_new = true;
            }
        }
    }

    if (is_new)
    {
        ::SetWindowLongPtrW(in_window,
                            GWLP_WNDPROC,
                            reinterpret_cast<LONG_PTR>(&handle_message) );

        /* What the application would have been told if its window had come to the front: it may be waiting for that. */
        ::PostMessageW(in_window,
                       WM_ACTIVATEAPP,
                       TRUE,
                       0);
        ::PostMessageW(in_window,
                       WM_ACTIVATE,
                       WA_ACTIVE,
                       0);
        ::PostMessageW(in_window,
                       WM_SETFOCUS,
                       0,
                       0);
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
