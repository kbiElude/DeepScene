/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "button_clicker.h"
#include <algorithm>
#include <cctype>

bool Stereo::Launcher::ButtonClicker::click_once(const DWORD&       in_process_id,
                                                 const std::string& in_button_text,
                                                 const std::string& in_checkbox_text,
                                                 CheckboxOutcome*   out_checkbox_outcome_ptr)
{
    bool   result = false;
    Search search;

    search.button_text   = get_normalized_text(in_button_text);
    search.checkbox_text = get_normalized_text(in_checkbox_text);
    search.process_id    = in_process_id;

    ::EnumWindows(on_top_level_window,
                  reinterpret_cast<LPARAM>(&search) );

    *out_checkbox_outcome_ptr = (in_checkbox_text.empty() ) ? CheckboxOutcome::NOT_ASKED
                                                            : CheckboxOutcome::NOT_FOUND;

    if (search.button != nullptr)
    {
        /* The checkbox is unticked before the button is pressed, because the application reads it when the button is pressed. */
        if (search.checkbox != nullptr)
        {
            *out_checkbox_outcome_ptr = untick_checkbox(search.checkbox);
        }

        /* The click is posted, not sent. The application may run the demo from inside its click handler, and a sent message would block until that handler returns. */
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
                                                       const std::string& in_checkbox_text,
                                                       const DWORD&       in_timeout_milliseconds,
                                                       CheckboxOutcome*   out_checkbox_outcome_ptr)
{
    constexpr DWORD POLL_INTERVAL_MILLISECONDS = 250;

    DWORD n_waited_milliseconds = 0;
    bool  result                = false;

    *out_checkbox_outcome_ptr = (in_checkbox_text.empty() ) ? CheckboxOutcome::NOT_ASKED
                                                            : CheckboxOutcome::NOT_FOUND;

    while (n_waited_milliseconds < in_timeout_milliseconds &&
           result                == false)
    {
        const bool is_clicked = click_once(in_process_id,
                                           in_button_text,
                                           in_checkbox_text,
                                           out_checkbox_outcome_ptr);

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

    const std::string class_name = get_normalized_text(class_i8vec64);
    const std::string text       = get_normalized_text(text_i8vec256);

    /* Accepts a standard button or a button class that a toolkit derives from it (such as "TButton"). A label with the same text is not pressed. */
    const bool is_button     = (class_name.find("button")   != std::string::npos);
    const bool is_checkbox   = (class_name.find("button")   != std::string::npos ||
                                class_name.find("checkbox") != std::string::npos);
    const bool is_enabled    = (::IsWindowEnabled(in_window) != FALSE);
    const bool is_shown      = (::IsWindowVisible(in_window) != FALSE);
    const bool is_button_hit = (is_button                    &&
                                is_enabled                   &&
                                is_shown                     &&
                                text == search_ptr->button_text);
    const bool is_box_hit    = (search_ptr->checkbox_text.empty() == false &&
                                is_checkbox                                &&
                                is_enabled                                 &&
                                is_shown                                   &&
                                text == search_ptr->checkbox_text);

    if (is_button_hit)
    {
        search_ptr->button = in_window;
    }
    else
    if (is_box_hit)
    {
        search_ptr->checkbox = in_window;
    }

    /* The search goes on until both are found, if a checkbox is looked for: it can come after the button. */
    const bool is_done = (search_ptr->button   != nullptr &&
                          (search_ptr->checkbox_text.empty() ||
                           search_ptr->checkbox != nullptr) );

    return (is_done) ? FALSE
                     : TRUE;
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

    const bool is_done = (search_ptr->button   != nullptr &&
                          (search_ptr->checkbox_text.empty() ||
                           search_ptr->checkbox != nullptr) );

    return (is_done) ? FALSE
                     : TRUE;
}

Stereo::Launcher::CheckboxOutcome Stereo::Launcher::ButtonClicker::untick_checkbox(const HWND& in_checkbox)
{
    constexpr DWORD N_POLLS                    = 20;
    constexpr DWORD POLL_INTERVAL_MILLISECONDS = 50;
    constexpr DWORD SEND_TIMEOUT_MILLISECONDS  = 1000;

    CheckboxOutcome result = CheckboxOutcome::ALREADY_UNTICKED;

    /* The state is asked from the application's thread. A message that is not answered in time counts as "not ticked". */
    const auto is_ticked_func = [&]()
    {
        DWORD_PTR state = BST_UNCHECKED;

        ::SendMessageTimeoutA(in_checkbox,
                              BM_GETCHECK,
                              0,
                              0,
                              SMTO_ABORTIFHUNG,
                              SEND_TIMEOUT_MILLISECONDS,
                             &state);

        return (state == BST_CHECKED);
    };

    if (is_ticked_func() )
    {
        /* A click, not BM_SETCHECK: the application is told of it the way it is of a click of the user. */
        ::PostMessageA(in_checkbox,
                       BM_CLICK,
                       0,
                       0);

        result = CheckboxOutcome::STILL_TICKED;

        for (DWORD n_poll = 0;
                   n_poll < N_POLLS && result == CheckboxOutcome::STILL_TICKED;
                 ++n_poll)
        {
            ::Sleep(POLL_INTERVAL_MILLISECONDS);

            if (is_ticked_func() == false)
            {
                result = CheckboxOutcome::UNTICKED;
            }
        }
    }

    return result;
}
