/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for JsonReader: what it accepts, what it refuses, and its error messages.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/json_reader.h"

static bool read_json(const std::string&                    in_text,
                      Analysis::APIDumpAnalysis::JsonValue* out_value_ptr,
                      std::string*                          out_error_ptr)
{
    return Analysis::APIDumpAnalysis::JsonReader::parse(in_text,
                                                        out_value_ptr,
                                                        out_error_ptr);
}

TEST(JsonReader, ScalarsAreRead)
{
    std::string                          error;
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_TRUE(read_json("true",
                         &value,
                         &error) ) << error;
    EXPECT_EQ  (value.type,
                Analysis::APIDumpAnalysis::JsonValue::Type::BOOL);
    EXPECT_TRUE(value.bool_value);

    ASSERT_TRUE(read_json("false",
                         &value,
                         &error) );
    EXPECT_FALSE(value.bool_value);

    ASSERT_TRUE(read_json("null",
                         &value,
                         &error) );
    EXPECT_EQ  (value.type,
                Analysis::APIDumpAnalysis::JsonValue::Type::NULL_VALUE);

    ASSERT_TRUE(read_json("  \"text\"\n",
                         &value,
                         &error) );
    EXPECT_EQ  (value.string_value,
                "text");
}

TEST(JsonReader, NumbersAreRead)
{
    struct Case
    {
        const char* text_ptr;
        double      expected;
    };

    const Case case_array[] =
    {
        {"0",       0.0},
        {"-0",      0.0},
        {"12",      12.0},
        {"-1.5",    -1.5},
        {"0.25",    0.25},
        {"1e3",     1000.0},
        {"2.5E-1",  0.25},
        {"1E+2",    100.0}
    };

    for (const auto& number_case : case_array)
    {
        Analysis::APIDumpAnalysis::JsonValue value;

        ASSERT_TRUE(read_json(number_case.text_ptr,
                             &value,
                             nullptr) ) << number_case.text_ptr;
        EXPECT_EQ  (value.type,
                    Analysis::APIDumpAnalysis::JsonValue::Type::NUMBER);
        EXPECT_DOUBLE_EQ(value.number_value,
                         number_case.expected) << number_case.text_ptr;
    }
}

TEST(JsonReader, ContainersAreRead)
{
    std::string                          error;
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_TRUE(read_json("{\"a\": [1, 2, {\"b\": null}], \"c\": {}, \"d\": []}",
                         &value,
                         &error) ) << error;

    ASSERT_EQ(value.at("a").item_vec.size(),
              3u);
    EXPECT_DOUBLE_EQ(value.at("a").item_vec.at(1).number_value,
                     2.0);
    EXPECT_EQ(value.at("a").item_vec.at(2).at("b").type,
              Analysis::APIDumpAnalysis::JsonValue::Type::NULL_VALUE);
    EXPECT_EQ(value.at("c").type,
              Analysis::APIDumpAnalysis::JsonValue::Type::OBJECT);
    EXPECT_TRUE(value.at("c").item_vec.empty() );
    EXPECT_EQ(value.at("d").type,
              Analysis::APIDumpAnalysis::JsonValue::Type::ARRAY);
    EXPECT_NE(value.find_ptr("a"),
              nullptr);
    EXPECT_EQ(value.find_ptr("missing"),
              nullptr);
    EXPECT_EQ(value.at("missing").type,
              Analysis::APIDumpAnalysis::JsonValue::Type::NULL_VALUE);
}

TEST(JsonReader, MembersKeepTheirOrder)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_TRUE(read_json("{\"z\":1,\"a\":2,\"m\":3}",
                         &value,
                         nullptr) );

    ASSERT_EQ(value.key_vec.size(),
              3u);
    EXPECT_EQ(value.key_vec.at(0),
              "z");
    EXPECT_EQ(value.key_vec.at(1),
              "a");
    EXPECT_EQ(value.key_vec.at(2),
              "m");
}

TEST(JsonReader, EscapesAreRead)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_TRUE(read_json("\"q\\\" b\\\\ s\\/ \\b\\f\\n\\r\\t\"",
                         &value,
                         nullptr) );
    EXPECT_EQ  (value.string_value,
                "q\" b\\ s/ \b\f\n\r\t");
}

TEST(JsonReader, UnicodeEscapesBecomeUtf8)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_TRUE(read_json("\"\\u0041\\u00e9\\u20ac\\ud83d\\ude00\"",
                         &value,
                         nullptr) );
    EXPECT_EQ  (value.string_value,
                "A\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80");
}

TEST(JsonReader, Utf8InTheTextGoesThrough)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_TRUE(read_json("\"\xC3\xA9\"",
                         &value,
                         nullptr) );
    EXPECT_EQ  (value.string_value,
                "\xC3\xA9");
}

TEST(JsonReader, TextThatIsNotJsonIsRefusedWithAReason)
{
    struct Case
    {
        const char* text_ptr;
        const char* error_part_ptr;
    };

    const Case case_array[] =
    {
        {"",                  "expected a value"},
        {"[1,]",              "expected a value"},
        {"{\"a\":1,}",        "expected a key"},
        {"{a:1}",             "expected a key"},
        {"[01]",              "expected ',' or ']'"},
        {"[1 2]",             "expected ',' or ']'"},
        {"\"abc",             "not closed"},
        {"[\"a\nb\"]",        "control character"},
        {"tru",               "expected true"},
        {"[1] x",             "after the value"},
        {"{\"a\"}",           "expected ':'"},
        {"-",                 "expected a number"},
        {"1.",                "after '.'"},
        {"1e",                "exponent"},
        {"[\"\\q\"]",         "unknown escape"},
        {"\"\\u12\"",         "4 hex digits"},
        {"\"\\udc00\"",       "low surrogate"},
        {"\"\\ud800x\"",      "high surrogate"},
        {"{\"a\":1,\"a\":2}", "repeated"},
        {"1e999",             "does not fit"},
        {"// c\n1",           "expected a value"}
    };

    for (const auto& error_case : case_array)
    {
        std::string                          error;
        Analysis::APIDumpAnalysis::JsonValue value;

        EXPECT_FALSE(read_json(error_case.text_ptr,
                              &value,
                              &error) ) << error_case.text_ptr;
        EXPECT_NE   (error.find(error_case.error_part_ptr),
                     std::string::npos) << error_case.text_ptr << " -> " << error;
    }
}

TEST(JsonReader, TheErrorSaysWhereTheProblemIs)
{
    std::string                          error;
    Analysis::APIDumpAnalysis::JsonValue value;

    EXPECT_FALSE(read_json("{\n  \"a\": 1,\n  \"b\": ?\n}",
                          &value,
                          &error) );
    EXPECT_EQ   (error.find("line 3, column 8"),
                 0u) << error;
}

TEST(JsonReader, TextThatIsTooDeepIsRefused)
{
    constexpr size_t MAX_DEPTH = 64; /* deepest nesting accepted        */

    std::string                          error;
    std::string                          text;
    Analysis::APIDumpAnalysis::JsonValue value;

    text.assign(MAX_DEPTH + 1,
                '[');

    EXPECT_FALSE(read_json(text,
                          &value,
                          &error) );
    EXPECT_NE   (error.find("too deeply"),
                 std::string::npos);

    /* The deepest accepted text is read. */
    text = std::string(MAX_DEPTH,
                       '[') +
           std::string(MAX_DEPTH,
                       ']');

    EXPECT_TRUE(read_json(text,
                         &value,
                         &error) ) << error;
}

TEST(JsonReader, AnErrorPointerIsOptional)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    EXPECT_FALSE(read_json("[",
                          &value,
                          nullptr) );
}

TEST(JsonReader, AValueIsClearedBeforeReading)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_TRUE(read_json("[1,2,3]",
                         &value,
                         nullptr) );
    ASSERT_TRUE(read_json("{\"a\":1}",
                         &value,
                         nullptr) );

    EXPECT_TRUE(value.item_vec.size() == 1u);
}

TEST(JsonReader, WhatTheWriterWritesIsRead)
{
    /* A long array of mixed items. */
    std::string                          text = "[";
    Analysis::APIDumpAnalysis::JsonValue value;

    for (int n_item = 0;
             n_item < 1000;
           ++n_item)
    {
        text += (n_item > 0) ? ",{\"i\":" + std::to_string(n_item) + ",\"s\":\"x\",\"b\":true,\"n\":null}"
                             : "{\"i\":0,\"s\":\"x\",\"b\":true,\"n\":null}";
    }

    text += "]";

    ASSERT_TRUE(read_json(text,
                         &value,
                         nullptr) );
    EXPECT_EQ  (value.item_vec.size(),
                1000u);
    EXPECT_DOUBLE_EQ(value.item_vec.at(999).at("i").number_value,
                     999.0);
}

TEST(JsonReader, TheFirstKeyWhichIsNotAllowedIsFound)
{
    Analysis::APIDumpAnalysis::JsonValue value;

    ASSERT_TRUE(read_json("{\"a\": 1, \"c\": 2, \"b\": 3}",
                         &value,
                          nullptr) );

    EXPECT_EQ(value.find_unknown_key_ptr({"a", "b", "c"}),
              nullptr);

    const std::string* unknown_ptr = value.find_unknown_key_ptr({"a", "b"});

    ASSERT_NE(unknown_ptr,
              nullptr);
    EXPECT_EQ(*unknown_ptr,
              "c");
}

TEST(JsonReader, AByteOrderMarkIsTakenOffAndAFileWhichIsNotThereIsNotRead)
{
    std::string text;

    EXPECT_EQ(Analysis::APIDumpAnalysis::JsonReader::strip_byte_order_mark("\xEF\xBB\xBF{}"),
              "{}");
    EXPECT_EQ(Analysis::APIDumpAnalysis::JsonReader::strip_byte_order_mark("{}"),
              "{}");
    EXPECT_FALSE(Analysis::APIDumpAnalysis::JsonReader::read_file("Z:/no/such/file.json",
                                                                 &text) );
}
