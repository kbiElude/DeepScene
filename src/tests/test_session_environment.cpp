/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for how the launcher hands a playback over to the converter in the application: through the environment.
 */
#include <gtest/gtest.h>
#include "stereo/session/session_environment.h"
#include <Windows.h>

TEST(SessionEnvironmentTest, ThePlaybackIsHandedOverThroughTheEnvironment)
{
    Stereo::Session::SessionInfo read_info;
    Stereo::Session::SessionInfo written_info;

    written_info.recipe_path = "C:\\recipes\\fr08.json";
    written_info.session_id  = "1234-5678";

    ASSERT_TRUE(Stereo::Session::SessionEnvironment::write(written_info) );
    ASSERT_TRUE(Stereo::Session::SessionEnvironment::read(&read_info) );

    EXPECT_EQ(read_info.recipe_path,
              written_info.recipe_path);
    EXPECT_EQ(read_info.session_id,
              written_info.session_id);

    /* A session logs only if it is asked to, and a session that is not does not take it from the one before. */
    EXPECT_FALSE(read_info.is_logging);

    written_info.is_logging = true;

    ASSERT_TRUE(Stereo::Session::SessionEnvironment::write(written_info) );
    ASSERT_TRUE(Stereo::Session::SessionEnvironment::read(&read_info) );

    EXPECT_TRUE(read_info.is_logging);

    written_info.is_logging = false;

    ASSERT_TRUE(Stereo::Session::SessionEnvironment::write(written_info) );
    ASSERT_TRUE(Stereo::Session::SessionEnvironment::read(&read_info) );

    EXPECT_FALSE(read_info.is_logging);

    /* A process which was not started by the launcher has none. */
    ::SetEnvironmentVariableA("APISTEREO_SESSION",
                              nullptr);

    EXPECT_FALSE(Stereo::Session::SessionEnvironment::read(&read_info) );

    ::SetEnvironmentVariableA("APISTEREO_RECIPE",
                              nullptr);
}

TEST(SessionEnvironmentTest, TheLogOfASessionIsInTheTemporaryDirectoryUnderItsNameAndTheExitCodeOfAStoppedApplicationIsNotZero)
{
    const std::string path = Stereo::Session::SessionEnvironment::get_log_path("12-34");

    EXPECT_NE(path.find("apistereo_12-34.log"),
              std::string::npos);
    EXPECT_NE(Stereo::Session::SessionEnvironment::get_converter_failed_exit_code(),
              0u);
}
