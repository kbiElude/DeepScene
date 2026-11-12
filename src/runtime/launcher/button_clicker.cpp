/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "button_clicker.h"
#include <algorithm>
#include <cctype>

namespace
{
    /* What the enumeration of the windows looks for, and what it found. */
    struct Search
    {
        HWND        button      = nullptr;
        std::string button_text; /* normalized */
        DWORD       process_id  = 0;
    };
}

bool Stereo::Launcher::ButtonClicker::click_once(const DWORD&       in_process_id,
                                                 const std::string& in_button_text)
{
    bool   result = false;
    Search search;

    search.button_text = get_normalized_text(in_button_text);
    search.process_id  = in_process_id;

    ::EnumWindows(on_top_level_window,
                  reinterpret_cast<LPARAM>(&search) );

    if (search.button != nullptr)
    {
        /* Posted, not sent: the application may run the demo from inside the handler of the click, and a send would wait for that. */
        result = (::PostMessageA(search.button,
                                 BM_CLICK,
                                 0,
                                 0) != FALSE);
    }

    return result;
}

bool Stereo::Launcher::ButtonClicker::click_when_shown(const HANDLE&      in_process,
                                                       const DWORD&       in_process_id,
                                                       const std::string& in_button_text,
                                                       const DWORD&       in_timeout_milliseconds)
{
    constexpr DWORD POLL_INTERVAL_MILLISECONDS = 250;

    DWORD n_waited_milliseconds = 0;
    bool  result                = false;

    while (n_waited_milliseconds < in_timeout_milliseconds &&
           result                == false)
    {
        const bool is_clicked = click_once(in_process_id,
                                           in_button_text);

        if (is_clicked)
        {
            result = true;

            continue;
        }

        const DWORD wait_result = ::WaitForSingleObject(in_process,
                                                        POLL_INTERVAL_MILLISECONDS);

        /* The application exited. */
        if (wait_result == WAIT_OBJECT_0)
        {
            break;
        }

        n_waited_milliseconds += POLL_INTERVAL_MILLISECONDS;
    }

    return result;
}

std::string Stereo::Launcher::ButtonClicker::get_normalized_text(const std::string& in_text)
{
    std::string result;

    for (const char character : in_text)
    {
        if (character != '&')
        {
            result.push_back(static_cast<char>(::tolower(static_cast<unsigned char>(character) ) ) );
        }
    }

    return result;
}

BOOL CALLBACK Stereo::Launcher::ButtonClicker::on_child_window(HWND   in_window,
                                                               LPARAM in_search_ptr)
{
    char          class_i8vec64[64]  = {};
    Search* const search_ptr         = reinterpret_cast<Search*>(in_search_ptr);
    char          text_i8vec256[256] = {};

    ::GetClassNameA (in_window,
                     class_i8vec64,
                     static_cast<int>(sizeof(class_i8vec64) ) );
    ::GetWindowTextA(in_window,
                     text_i8vec256,
                     static_cast<int>(sizeof(text_i8vec256) ) );

    /* A button, or one that a toolkit makes of it ("TButton"); a label that says the same is not pressed. */
    const bool is_button  = (get_normalized_text(class_i8vec64).find("button") != std::string::npos);
    const bool is_enabled = (::IsWindowEnabled(in_window)                      != FALSE);
    const bool is_shown   = (::IsWindowVisible(in_window)                      != FALSE);
    const bool is_match   = (get_normalized_text(text_i8vec256)                == search_ptr->button_text);

    if (is_button  &&
        is_enabled &&
        is_shown   &&
        is_match)
    {
        search_ptr->button = in_window;
    }

    return (search_ptr->button == nullptr) ? TRUE
                                           : FALSE;
}

BOOL CALLBACK Stereo::Launcher::ButtonClicker::on_top_level_window(HWND   in_window,
                                                                   LPARAM in_search_ptr)
{
    DWORD         process_id = 0;
    Search* const search_ptr = reinterpret_cast<Search*>(in_search_ptr);

    ::GetWindowThreadProcessId(in_window,
                              &process_id);

    if (process_id == search_ptr->process_id)
    {
        ::EnumChildWindows(in_window,
                           on_child_window,
                           in_search_ptr);
    }

    return (search_ptr->button == nullptr) ? TRUE
                                           : FALSE;
}
