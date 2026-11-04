/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for InvisibleWindows: a window is told to its application as active, whatever the system says.
 */
#include <gtest/gtest.h>
#include "invisible_windows.h"
#include <Windows.h>

namespace
{
    /* What the procedure of the application was told last. */
    struct Seen
    {
        WPARAM activate_app_w_param = 99;
        WPARAM activate_w_param     = 99;
        WPARAM nc_activate_w_param  = 99;
        bool   was_kill_focus_seen  = false;
    };

    Seen g_seen;

    LRESULT CALLBACK application_procedure(HWND   in_window,
                                           UINT   in_message,
                                           WPARAM in_w_param,
                                           LPARAM in_l_param)
    {
        switch (in_message)
        {
            case WM_ACTIVATE:
            {
                g_seen.activate_w_param = in_w_param;

                break;
            }

            case WM_ACTIVATEAPP:
            {
                g_seen.activate_app_w_param = in_w_param;

                break;
            }

            case WM_KILLFOCUS:
            {
                g_seen.was_kill_focus_seen = true;

                break;
            }

            case WM_NCACTIVATE:
            {
                g_seen.nc_activate_w_param = in_w_param;

                break;
            }

            default:
            {
                break;
            }
        }

        return ::DefWindowProcW(in_window,
                                in_message,
                                in_w_param,
                                in_l_param);
    }

    HWND make_window()
    {
        WNDCLASSW window_class = {};

        window_class.hInstance     = ::GetModuleHandleW(nullptr);
        window_class.lpfnWndProc   = application_procedure;
        window_class.lpszClassName = L"InvisibleWindowsTestWindow";

        ::RegisterClassW(&window_class);

        return ::CreateWindowExW(0,
                                 window_class.lpszClassName,
                                 L"test",
                                 WS_POPUP,
                                 0,
                                 0,
                                 16,
                                 16,
                                 nullptr,
                                 nullptr,
                                 window_class.hInstance,
                                 nullptr);
    }

    void pump_messages()
    {
        MSG message;

        while (::PeekMessageW(&message,
                              nullptr,
                              0,
                              0,
                              PM_REMOVE) )
        {
            ::DispatchMessageW(&message);
        }
    }
}

TEST(InvisibleWindows, TheApplicationIsToldItsWindowIsActiveWhenTheWindowIsAdded)
{
    Stereo::ConverterDLL::InvisibleWindows windows;
    const HWND                             window = make_window();

    ASSERT_NE(window,
              nullptr);

    g_seen = Seen();

    windows.add(window);

    pump_messages();

    EXPECT_EQ(g_seen.activate_app_w_param,
              static_cast<WPARAM>(TRUE) );
    EXPECT_EQ(LOWORD(g_seen.activate_w_param),
              static_cast<WORD>(WA_ACTIVE) );

    ::DestroyWindow(window);
}

TEST(InvisibleWindows, AMessageThatSaysTheWindowIsNotActiveIsPassedOnAsOneThatSaysItIs)
{
    Stereo::ConverterDLL::InvisibleWindows windows;
    const HWND                             window = make_window();

    ASSERT_NE(window,
              nullptr);

    windows.add(window);

    pump_messages();

    g_seen = Seen();

    ::SendMessageW(window,
                   WM_ACTIVATEAPP,
                   FALSE,
                   0);
    ::SendMessageW(window,
                   WM_ACTIVATE,
                   WA_INACTIVE,
                   0);
    ::SendMessageW(window,
                   WM_NCACTIVATE,
                   FALSE,
                   0);
    ::SendMessageW(window,
                   WM_KILLFOCUS,
                   0,
                   0);

    EXPECT_EQ(g_seen.activate_app_w_param,
              static_cast<WPARAM>(TRUE) );
    EXPECT_EQ(LOWORD(g_seen.activate_w_param),
              static_cast<WORD>(WA_ACTIVE) );
    EXPECT_EQ(g_seen.nc_activate_w_param,
              static_cast<WPARAM>(TRUE) );
    EXPECT_FALSE(g_seen.was_kill_focus_seen);

    ::DestroyWindow(window);
}

TEST(InvisibleWindows, AWindowIsMadeTransparentAndClickThroughAndIsNotHidden)
{
    Stereo::ConverterDLL::InvisibleWindows windows;
    const HWND                             window = make_window();

    ASSERT_NE(window,
              nullptr);

    windows.add(window);

    const LONG_PTR style = ::GetWindowLongPtrW(window,
                                               GWL_EXSTYLE);

    EXPECT_NE(style & WS_EX_LAYERED,
              0);
    EXPECT_NE(style & WS_EX_TRANSPARENT,
              0);
    EXPECT_NE(style & WS_EX_TOOLWINDOW,
              0);

    ::DestroyWindow(window);
}
