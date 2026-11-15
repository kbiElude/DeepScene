/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for ButtonClicker: a button of a dialog is pressed, and a checkbox of the dialog is unticked before it.
 */
#include <gtest/gtest.h>
#include "runtime/launcher/button_clicker.h"
#include <atomic>
#include <thread>
#include <Windows.h>

namespace
{
    constexpr int  CHECKBOX_ID  = 101;
    constexpr int  PLAY_ID      = 102;
    constexpr DWORD TIMEOUT_MILLISECONDS = 5000;

    /* The state of the checkbox when the button was pressed: the application reads it then. */
    std::atomic<int>  g_checkbox_state_at_play = -1;
    std::atomic<bool> g_is_play_pressed        = false;

    LRESULT CALLBACK dialog_procedure(HWND   in_window,
                                      UINT   in_message,
                                      WPARAM in_w_param,
                                      LPARAM in_l_param)
    {
        if (in_message == WM_COMMAND &&
            LOWORD(in_w_param) == PLAY_ID)
        {
            const HWND checkbox = ::GetDlgItem(in_window,
                                               CHECKBOX_ID);

            g_checkbox_state_at_play = static_cast<int>(::SendMessageA(checkbox,
                                                                       BM_GETCHECK,
                                                                       0,
                                                                       0) );
            g_is_play_pressed        = true;
        }

        return ::DefWindowProcA(in_window,
                                in_message,
                                in_w_param,
                                in_l_param);
    }

    /* A window of its own thread with a checkbox and a button, like the dialog that a demo shows before it starts. */
    class TestDialog
    {
    public:
        TestDialog(const bool&        in_is_checkbox_ticked,
                   const std::string& in_checkbox_text)
        {
            g_checkbox_state_at_play = -1;
            g_is_play_pressed        = false;

            m_thread = std::thread([this, in_is_checkbox_ticked, in_checkbox_text]()
            {
                WNDCLASSA window_class     = {};
                HWND      window           = nullptr;
                MSG       message          = {};

                window_class.hInstance     = ::GetModuleHandleA(nullptr);
                window_class.lpfnWndProc   = dialog_procedure;
                window_class.lpszClassName = "ButtonClickerTestDialog";

                ::RegisterClassA(&window_class);

                window = ::CreateWindowA("ButtonClickerTestDialog",
                                         "Setup",
                                         WS_OVERLAPPED | WS_VISIBLE,
                                         0,
                                         0,
                                         300,
                                         200,
                                         nullptr,
                                         nullptr,
                                         window_class.hInstance,
                                         nullptr);

                const HWND checkbox = ::CreateWindowA("BUTTON",
                                                      in_checkbox_text.c_str(),
                                                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                                      10,
                                                      10,
                                                      200,
                                                      24,
                                                      window,
                                                      reinterpret_cast<HMENU>(static_cast<INT_PTR>(CHECKBOX_ID) ),
                                                      window_class.hInstance,
                                                      nullptr);

                ::CreateWindowA("BUTTON",
                                "&Play >",
                                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                10,
                                50,
                                100,
                                28,
                                window,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(PLAY_ID) ),
                                window_class.hInstance,
                                nullptr);

                ::SendMessageA(checkbox,
                               BM_SETCHECK,
                               (in_is_checkbox_ticked) ? BST_CHECKED
                                                       : BST_UNCHECKED,
                               0);

                m_thread_id = ::GetCurrentThreadId();
                m_window    = window;

                while (::GetMessageA(&message,
                                     nullptr,
                                     0,
                                     0) > 0)
                {
                    ::TranslateMessage(&message);
                    ::DispatchMessageA(&message);
                }

                ::DestroyWindow(window);
                ::UnregisterClassA("ButtonClickerTestDialog",
                                   window_class.hInstance);
            });

            while (m_window == nullptr)
            {
                ::Sleep(5);
            }
        }

        ~TestDialog()
        {
            ::PostThreadMessageA(m_thread_id,
                                 WM_QUIT,
                                 0,
                                 0);

            m_thread.join();
        }

    private:
        std::atomic<HWND>  m_window    = nullptr;
        std::thread        m_thread;
        std::atomic<DWORD> m_thread_id = 0;
    };

    bool wait_for_play()
    {
        for (uint32_t n_wait = 0;
                      n_wait < 200 && g_is_play_pressed == false;
                    ++n_wait)
        {
            ::Sleep(10);
        }

        return g_is_play_pressed;
    }
}

TEST(ButtonClicker, TheButtonOfTheDialogIsPressed)
{
    Stereo::Launcher::CheckboxOutcome outcome = Stereo::Launcher::CheckboxOutcome::UNTICKED;
    const TestDialog                  dialog(true,
                                             "Fullscreen");

    const bool is_clicked = Stereo::Launcher::ButtonClicker::click_when_shown(::GetCurrentProcess(),
                                                                              ::GetCurrentProcessId(),
                                                                              "play >",
                                                                              std::string(),
                                                                             TIMEOUT_MILLISECONDS,
                                                                             &outcome);

    EXPECT_TRUE(is_clicked);
    EXPECT_TRUE(wait_for_play() );
    EXPECT_EQ  (outcome,
                Stereo::Launcher::CheckboxOutcome::NOT_ASKED);
    EXPECT_EQ  (g_checkbox_state_at_play.load(),
                static_cast<int>(BST_CHECKED) );
}

TEST(ButtonClicker, ATickedCheckboxIsUntickedBeforeTheButtonIsPressed)
{
    Stereo::Launcher::CheckboxOutcome outcome = Stereo::Launcher::CheckboxOutcome::NOT_ASKED;
    const TestDialog                  dialog(true,
                                             "&Fullscreen");

    const bool is_clicked = Stereo::Launcher::ButtonClicker::click_when_shown(::GetCurrentProcess(),
                                                                              ::GetCurrentProcessId(),
                                                                              "Play >",
                                                                              "fullscreen",
                                                                             TIMEOUT_MILLISECONDS,
                                                                             &outcome);

    EXPECT_TRUE(is_clicked);
    EXPECT_TRUE(wait_for_play() );
    EXPECT_EQ  (outcome,
                Stereo::Launcher::CheckboxOutcome::UNTICKED);

    /* The application read the checkbox when the button was pressed: it was not ticked then. */
    EXPECT_EQ  (g_checkbox_state_at_play.load(),
                static_cast<int>(BST_UNCHECKED) );
}

TEST(ButtonClicker, ACheckboxWhichIsNotTickedIsLeftAlone)
{
    Stereo::Launcher::CheckboxOutcome outcome = Stereo::Launcher::CheckboxOutcome::NOT_ASKED;
    const TestDialog                  dialog(false,
                                             "Fullscreen");

    const bool is_clicked = Stereo::Launcher::ButtonClicker::click_when_shown(::GetCurrentProcess(),
                                                                              ::GetCurrentProcessId(),
                                                                              "Play >",
                                                                              "Fullscreen",
                                                                             TIMEOUT_MILLISECONDS,
                                                                             &outcome);

    EXPECT_TRUE(is_clicked);
    EXPECT_TRUE(wait_for_play() );
    EXPECT_EQ  (outcome,
                Stereo::Launcher::CheckboxOutcome::ALREADY_UNTICKED);
    EXPECT_EQ  (g_checkbox_state_at_play.load(),
                static_cast<int>(BST_UNCHECKED) );
}

TEST(ButtonClicker, ACheckboxThatTheDialogDoesNotHaveIsReportedAndTheButtonIsPressedAllTheSame)
{
    Stereo::Launcher::CheckboxOutcome outcome = Stereo::Launcher::CheckboxOutcome::NOT_ASKED;
    const TestDialog                  dialog(true,
                                             "Fullscreen");

    const bool is_clicked = Stereo::Launcher::ButtonClicker::click_when_shown(::GetCurrentProcess(),
                                                                              ::GetCurrentProcessId(),
                                                                              "Play >",
                                                                              "Windowed",
                                                                             TIMEOUT_MILLISECONDS,
                                                                             &outcome);

    EXPECT_TRUE(is_clicked);
    EXPECT_TRUE(wait_for_play() );
    EXPECT_EQ  (outcome,
                Stereo::Launcher::CheckboxOutcome::NOT_FOUND);
}
