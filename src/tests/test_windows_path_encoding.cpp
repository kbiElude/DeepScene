/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for WindowsPathEncoding: what a path becomes when it is handed to Windows.
 */
#include <gtest/gtest.h>
#include "utils/files/windows_path_encoding.h"

TEST(WindowsPathEncoding, ASlashBecomesABackslashInAPath)
{
    EXPECT_EQ(Utils::Files::WindowsPathEncoding::to_wide("C:/folder/file.txt"),
              std::wstring(L"C:\\folder\\file.txt") );
}

TEST(WindowsPathEncoding, ATextWhichIsNotAPathIsNotChanged)
{
    EXPECT_EQ(Utils::Files::WindowsPathEncoding::to_wide_text("a/b"),
              std::wstring(L"a/b") );
}

TEST(WindowsPathEncoding, ALongPathOnADriveGetsThePrefixThatLiftsTheLengthLimit)
{
    const std::string long_path = "C:\\" + std::string(300,
                                                       'a');

    EXPECT_EQ(Utils::Files::WindowsPathEncoding::to_wide(long_path).compare(0,
                                                                            4,
                                                                            L"\\\\?\\"),
              0);
}

TEST(WindowsPathEncoding, AShortPathHasNoPrefix)
{
    EXPECT_EQ(Utils::Files::WindowsPathEncoding::to_wide("C:\\short"),
              std::wstring(L"C:\\short") );
}

TEST(WindowsPathEncoding, NonAsciiTextSurvivesTheRoundTrip)
{
    const std::string text = "d\xC5\x82ugi katalog";

    EXPECT_EQ(Utils::Files::WindowsPathEncoding::to_utf8(Utils::Files::WindowsPathEncoding::to_wide_text(text) ),
              text);
}
