/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the session of the VR player: the command line of the launcher, and the reasons for which an application is not
 * started.
 */
#include <gtest/gtest.h>
#include "fixtures.h"
#include "vr_player/app_session.h"
#include <Windows.h>

TEST(AppSessionTest, EveryValueOfTheLauncherCommandLineIsQuoted)
{
    VR::VRPlayer::SessionRequest request;

    request.executable_path = "C:\\Program Files\\Demo\\demo.exe";
    request.launcher_path   = "C:\\VR Player\\StereoLauncher_x86.exe";
    request.recipe_path     = "C:\\recipes\\fr08.json";
    request.session_id      = "12-34";

    EXPECT_EQ(VR::VRPlayer::AppSession::get_command_line(request),
              "\"C:\\VR Player\\StereoLauncher_x86.exe\" --recipe \"C:\\recipes\\fr08.json\" --session \"12-34\" --wait "
              "\"C:\\Program Files\\Demo\\demo.exe\"");
}

TEST(AppSessionTest, ArgumentsAndTheWorkingDirectoryAreHandedOverWithTheirQuotesAndBackslashesKept)
{
    VR::VRPlayer::SessionRequest request;

    request.arguments         = "-title \"My demo\"";
    request.executable_path   = "demo.exe";
    request.launcher_path     = "launcher.exe";
    request.recipe_path       = "r.json";
    request.session_id        = "s";
    request.working_directory = "C:\\work dir\\";

    /* A quote inside is escaped; backslashes in front of the closing quote are doubled, or they would escape it. */
    EXPECT_EQ(VR::VRPlayer::AppSession::get_command_line(request),
              "\"launcher.exe\" --recipe \"r.json\" --session \"s\" --arguments \"-title \\\"My demo\\\"\" "
              "--working-directory \"C:\\work dir\\\\\" --wait \"demo.exe\"");
}

TEST(AppSessionTest, TheWindowIsStartedWithTheSessionAndTheTitle)
{
    VR::VRPlayer::SessionRequest request;

    request.session_id  = "12-34";
    request.presenter_path = "C:\\VR Player\\StereoPresenter.exe";

    EXPECT_EQ(VR::VRPlayer::AppSession::get_presenter_command_line(request),
              "\"C:\\VR Player\\StereoPresenter.exe\" --session \"12-34\"");

    request.title = "fr08 \"in\" VR";

    EXPECT_EQ(VR::VRPlayer::AppSession::get_presenter_command_line(request),
              "\"C:\\VR Player\\StereoPresenter.exe\" --session \"12-34\" --title \"fr08 \\\"in\\\" VR\"");
}

TEST(AppSessionTest, TheWindowIsToShowTheImagesInTheHeadsetOnlyIfAskedTo)
{
    VR::VRPlayer::SessionRequest request;

    request.session_id  = "s";
    request.presenter_path = "presenter.exe";

    EXPECT_EQ(VR::VRPlayer::AppSession::get_presenter_command_line(request),
              "\"presenter.exe\" --session \"s\"");

    request.use_headset = true;

    EXPECT_EQ(VR::VRPlayer::AppSession::get_presenter_command_line(request),
              "\"presenter.exe\" --session \"s\" --headset");

    request.pace_to_headset = false;

    EXPECT_EQ(VR::VRPlayer::AppSession::get_presenter_command_line(request),
              "\"presenter.exe\" --session \"s\" --headset --no-pacing");

    request.is_logging = true;

    EXPECT_EQ(VR::VRPlayer::AppSession::get_presenter_command_line(request),
              "\"presenter.exe\" --session \"s\" --headset --no-pacing --log");
    EXPECT_NE(VR::VRPlayer::AppSession::get_command_line(request).find(" --log"),
              std::string::npos);

    request.is_logging = false;

    EXPECT_EQ(VR::VRPlayer::AppSession::get_command_line(request).find(" --log"),
              std::string::npos);

    /* Nothing is paced to a headset that is not there. */
    request.use_headset = false;

    EXPECT_EQ(VR::VRPlayer::AppSession::get_presenter_command_line(request),
              "\"presenter.exe\" --session \"s\"");
}

TEST(AppSessionTest, AWindowWhichIsNotInstalledIsReportedWithItsPath)
{
    VR::AppCatalog::AppEntry app;
    char                     directory_i8vec260[MAX_PATH] = {};
    std::string              error;
    VR::VRPlayer::AppSession session;

    /* A directory with the launcher of this architecture, which is all that is looked for, and without the window. */
    ::GetTempPathA(MAX_PATH,
                   directory_i8vec260);

    const std::string directory = std::string(directory_i8vec260) + "apistereo_test_" + std::to_string(::GetCurrentProcessId() );
    const std::string launcher  = directory + "\\StereoLauncher_x64.exe";
    const std::string launcher_x86 = directory + "\\StereoLauncher_x86.exe";

    ::CreateDirectoryA(directory.c_str(),
                       nullptr);
    ::CloseHandle(::CreateFileA(launcher.c_str(),
                                GENERIC_WRITE,
                                0,
                                nullptr,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr) );
    ::CloseHandle(::CreateFileA(launcher_x86.c_str(),
                                GENERIC_WRITE,
                                0,
                                nullptr,
                                CREATE_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr) );

    app.executable_path = Tests::Fixtures::get_test_executable_path();

    EXPECT_FALSE(session.start(app,
                               directory,
                               0,     /* in_target_eye_width */
                               0,     /* in_target_eye_height */
                              false, /* in_use_headset */
                              true,  /* in_pace_to_headset */
                              false, /* in_write_log */
                              &error) );
    EXPECT_NE   (error.find("StereoPresenter.exe"),
                 std::string::npos) << error;
    EXPECT_EQ   (session.get_state(),
                 VR::VRPlayer::SessionState::IDLE);

    ::DeleteFileA (launcher.c_str() );
    ::DeleteFileA (launcher_x86.c_str() );
    ::RemoveDirectoryA(directory.c_str() );
}

TEST(AppSessionTest, AnApplicationWhichIsNotThereOrNotAnExecutableIsNotStarted)
{
    VR::AppCatalog::AppEntry app;
    std::string              error;
    VR::VRPlayer::AppSession session;

    app.executable_path = "Z:\\no\\such\\demo.exe";

    EXPECT_FALSE(session.start(app,
                               ".",
                               0,     /* in_target_eye_width */
                               0,     /* in_target_eye_height */
                              false, /* in_use_headset */
                              true,  /* in_pace_to_headset */
                              false, /* in_write_log */
                              &error) );
    EXPECT_FALSE(error.empty() );
    EXPECT_EQ   (session.get_state(),
                 VR::VRPlayer::SessionState::IDLE);
}

TEST(AppSessionTest, ALauncherWhichIsNotInstalledIsReportedWithItsPath)
{
    VR::AppCatalog::AppEntry app;
    std::string              error;
    VR::VRPlayer::AppSession session;

    app.executable_path = Tests::Fixtures::get_test_executable_path();

    EXPECT_FALSE(session.start(app,
                               "Z:\\no\\such\\directory",
                               0,     /* in_target_eye_width */
                               0,     /* in_target_eye_height */
                              false, /* in_use_headset */
                              true,  /* in_pace_to_headset */
                              false, /* in_write_log */
                              &error) );
    EXPECT_NE   (error.find("is not installed"),
                 std::string::npos);
    EXPECT_NE   (error.find("Z:\\no\\such\\directory"),
                 std::string::npos);
    EXPECT_EQ   (session.get_state(),
                 VR::VRPlayer::SessionState::IDLE);
}
