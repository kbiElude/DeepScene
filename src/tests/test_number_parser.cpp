/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for NumberParser: only a plain number is a number, and nothing wraps around.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/number_parser.h"
#include <limits>

TEST(NumberParser, DecimalDigitsAreAnUnsignedNumber)
{
    uint64_t value = 99;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::NumberParser::parse_unsigned("0042",
                                                                        1000,
                                                                       &value) );
    EXPECT_EQ(value,
              42u);
}

TEST(NumberParser, TextWhichIsNotOnlyDigitsIsNotAnUnsignedNumber)
{
    for (const char* text : {"", " 1", "1 ", "-1", "+1", "1.5", "0x10", "1e3", "abc", "1-2"})
    {
        uint64_t value = 99;

        EXPECT_FALSE(Analysis::APIDumpAnalysis::NumberParser::parse_unsigned(text,
                                                                             1000,
                                                                            &value) ) << text;
        EXPECT_EQ   (value,
                     99u) << text;
    }
}

TEST(NumberParser, ANumberAboveTheMaximumIsRefusedWhateverTheWidth)
{
    uint64_t value = 0;

    EXPECT_TRUE (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned("4294967295",
                                                                         std::numeric_limits<uint32_t>::max(),
                                                                        &value) );
    EXPECT_FALSE(Analysis::APIDumpAnalysis::NumberParser::parse_unsigned("4294967296",
                                                                         std::numeric_limits<uint32_t>::max(),
                                                                        &value) );
    EXPECT_FALSE(Analysis::APIDumpAnalysis::NumberParser::parse_unsigned("99999999999999999999999",
                                                                         std::numeric_limits<uint64_t>::max(),
                                                                        &value) );
    EXPECT_TRUE (Analysis::APIDumpAnalysis::NumberParser::parse_unsigned("18446744073709551615",
                                                                         std::numeric_limits<uint64_t>::max(),
                                                                        &value) );
    EXPECT_EQ   (value,
                 std::numeric_limits<uint64_t>::max() );
}

TEST(NumberParser, APlainDecimalNumberIsADouble)
{
    double value = -1.0;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::NumberParser::parse_double("2.5",
                                                                     &value) );
    EXPECT_EQ  (value,
                2.5);
    ASSERT_TRUE(Analysis::APIDumpAnalysis::NumberParser::parse_double("0",
                                                                     &value) );
    EXPECT_EQ  (value,
                0.0);
}

TEST(NumberParser, TextWhichIsNotAPlainDecimalNumberIsNotADouble)
{
    for (const char* text : {"", " 1", "-1", "+1", "nan", "inf", "0x10", "1e3", "1.2.3", "abc", "."})
    {
        double value = -1.0;

        EXPECT_FALSE(Analysis::APIDumpAnalysis::NumberParser::parse_double(text,
                                                                          &value) ) << text;
        EXPECT_EQ   (value,
                     -1.0) << text;
    }
}
