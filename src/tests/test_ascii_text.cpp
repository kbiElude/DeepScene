/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include <gtest/gtest.h>
#include "utils/strings/ascii_text.h"

TEST(AsciiText, TheLettersOfTheAsciiRangeAreMadeLowerCaseAndEverythingElseIsLeftAlone)
{
    EXPECT_EQ(Utils::Strings::AsciiText::to_lower("Kasparov FR08_Final.EXE"),
              "kasparov fr08_final.exe");
    EXPECT_EQ(Utils::Strings::AsciiText::to_lower(""),
              "");
    EXPECT_EQ(Utils::Strings::AsciiText::to_lower("0123@[`{ \xC4"),
              "0123@[`{ \xC4");
}
