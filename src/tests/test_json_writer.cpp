/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for JsonWriter: its text, that the text is valid JSON (read back by the strict reader), and misuse.
 */
#include <gtest/gtest.h>
#include "utils/json/json_writer.h"
#include "utils/json/json_reader.h"
#include <limits>
#include <math.h>

TEST(JsonWriter, AnArrayOfValuesIsWrittenOnOneLine)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_array();
    writer.write_u64(1);
    writer.write_i64(-2);
    writer.write_bool(true); /* in_value */
    writer.write_null();
    writer.write_string("a");
    writer.end_array();

    EXPECT_FALSE(writer.has_failed() );
    EXPECT_EQ   (writer.get_text(),
                 "[1,-2,true,null,\"a\"]");
}

TEST(JsonWriter, AnObjectKeepsTheOrderOfItsKeys)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_object();
    writer.write_key("z");
    writer.write_u64(1);
    writer.write_key("a");
    writer.write_u64(2);
    writer.end_object();

    EXPECT_FALSE(writer.has_failed() );
    EXPECT_EQ   (writer.get_text(),
                 "{\"z\":1,\"a\":2}");
}

TEST(JsonWriter, PrettyTextHasAnItemOnEachLine)
{
    Utils::Json::JsonWriter writer(true); /* in_is_pretty */

    writer.begin_object();
    writer.write_key("list");
    writer.begin_array();
    writer.write_u64(1);
    writer.write_u64(2);
    writer.end_array();
    writer.write_key("empty");
    writer.begin_object();
    writer.end_object();
    writer.end_object();

    EXPECT_FALSE(writer.has_failed() );
    EXPECT_EQ   (writer.get_text(),
                 "{\n"
                 "  \"list\": [\n"
                 "    1,\n"
                 "    2\n"
                 "  ],\n"
                 "  \"empty\": {}\n"
                 "}");
}

TEST(JsonWriter, EmptyContainersAreWrittenWithoutAnythingInThem)
{
    Utils::Json::JsonWriter writer(true); /* in_is_pretty */

    writer.begin_array();
    writer.begin_array();
    writer.end_array();
    writer.end_array();

    EXPECT_FALSE(writer.has_failed() );
    EXPECT_EQ   (writer.get_text(),
                 "[\n  []\n]");
}

TEST(JsonWriter, StringsAreEscaped)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.write_string(std::string("q\" b\\ n\n t\t r\r \b \f c") + '\x01' + "\xC3\xA9");

    EXPECT_FALSE(writer.has_failed() );
    EXPECT_EQ   (writer.get_text(),
                 "\"q\\\" b\\\\ n\\n t\\t r\\r \\b \\f c\\u0001\xC3\xA9\"");

    Utils::Json::JsonValue value;

    ASSERT_TRUE(Utils::Json::JsonReader::parse(writer.get_text(),
                                            &value,
                                            nullptr) );
    EXPECT_EQ  (value.string_value.substr(0, 3),
                "q\" ");
}

TEST(JsonWriter, KeysAreEscapedToo)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_object();
    writer.write_key("a\"b");
    writer.write_null();
    writer.end_object();

    EXPECT_EQ(writer.get_text(),
              "{\"a\\\"b\":null}");
}

TEST(JsonWriter, NumbersAreWrittenTheSameWayEveryTime)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_array();
    writer.write_double(0.0);
    writer.write_double(100.0);
    writer.write_double(0.02);
    writer.write_double(-1.5);
    writer.write_double(1e20);
    writer.write_u64(18446744073709551615ull);
    writer.write_i64(std::numeric_limits<int64_t>::min() );
    writer.write_hex_u64(0xabcdef);
    writer.end_array();

    EXPECT_FALSE(writer.has_failed() );
    EXPECT_EQ   (writer.get_text(),
                 "[0,100,0.02,-1.5,1e+20,18446744073709551615,-9223372036854775808,\"0x0000000000abcdef\"]");
}

TEST(JsonWriter, NumbersThatAreNotJsonAreWrittenAsNull)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_array();
    writer.write_double(std::numeric_limits<double>::quiet_NaN() );
    writer.write_double(std::numeric_limits<double>::infinity() );
    writer.write_double(-std::numeric_limits<double>::infinity() );
    writer.end_array();

    EXPECT_FALSE(writer.has_failed() );
    EXPECT_EQ   (writer.get_text(),
                 "[null,null,null]");
}

TEST(JsonWriter, WhatIsWrittenIsReadBackByAStrictReader)
{
    Utils::Json::JsonValue            value;
    Utils::Json::JsonWriter writer(true); /* in_is_pretty */

    writer.begin_object();
    writer.write_key  ("name");
    writer.write_string("fr08");
    writer.write_key  ("frames");
    writer.begin_array();
    writer.write_u64(1);
    writer.write_double(2.5);
    writer.write_bool(false); /* in_value */
    writer.write_null ();
    writer.end_array();
    writer.write_key  ("nested");
    writer.begin_object();
    writer.write_key  ("key");
    writer.write_hex_u64(0xffffffffffffffffull);
    writer.end_object();
    writer.end_object();

    ASSERT_FALSE(writer.has_failed() );
    ASSERT_TRUE (Utils::Json::JsonReader::parse(writer.get_text(),
                                             &value,
                                             nullptr) );

    EXPECT_EQ(value.at("name").string_value,
              "fr08");
    EXPECT_EQ(value.at("frames").item_vec.size(),
              4u);
    EXPECT_DOUBLE_EQ(value.at("frames").item_vec.at(1).number_value,
                     2.5);
    EXPECT_EQ(value.at("nested").at("key").string_value,
              "0xffffffffffffffff");
}

TEST(JsonWriter, AValueInAnObjectNeedsAKey)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_object();
    writer.write_u64(1);
    writer.end_object();

    EXPECT_TRUE(writer.has_failed() );
}

TEST(JsonWriter, AKeyNeedsAnObject)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_array();
    writer.write_key("a");
    writer.end_array();

    EXPECT_TRUE(writer.has_failed() );
}

TEST(JsonWriter, AKeyNeedsAValueBeforeTheNextKey)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_object();
    writer.write_key("a");
    writer.write_key("b");
    writer.write_u64(1);
    writer.end_object();

    EXPECT_TRUE(writer.has_failed() );
}

TEST(JsonWriter, AnObjectCannotEndWhileAKeyWaitsForItsValue)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_object();
    writer.write_key("a");
    writer.end_object();

    EXPECT_TRUE(writer.has_failed() );
}

TEST(JsonWriter, AnEndMustMatchItsBegin)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_array();
    writer.end_object();

    EXPECT_TRUE(writer.has_failed() );
}

TEST(JsonWriter, AnEndWithoutABeginFails)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.end_array();

    EXPECT_TRUE(writer.has_failed() );
}

TEST(JsonWriter, OnlyOneValueMayBeAtTheTop)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.write_u64(1);
    writer.write_u64(2);

    EXPECT_TRUE(writer.has_failed() );
}

TEST(JsonWriter, TextWithAContainerStillOpenIsNotComplete)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_array();

    EXPECT_TRUE(writer.has_failed() );
}

TEST(JsonWriter, TextWithNothingInItIsNotComplete)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    EXPECT_TRUE(writer.has_failed() );
}

TEST(JsonReader, TextThatIsNotJsonIsRefused)
{
    const char* text_ptr_array[] =
    {
        "",
        "[1,]",
        "{\"a\":1,}",
        "{a:1}",
        "[01]",
        "[1 2]",
        "\"abc",
        "[\"a\nb\"]",
        "tru",
        "[1] x",
        "{\"a\"}",
        "-",
        "[\"\\q\"]"
    };

    for (const auto& text_ptr : text_ptr_array)
    {
        Utils::Json::JsonValue value;

        EXPECT_FALSE(Utils::Json::JsonReader::parse(text_ptr,
                                                 &value,
                                                 nullptr) ) << text_ptr;
    }
}

TEST(JsonWriter, AnInlineArrayIsOnOneLineAlsoInPrettyText)
{
    Utils::Json::JsonWriter writer(true); /* in_is_pretty */

    writer.begin_object();
    writer.write_key("values");
    writer.begin_inline_array();
    writer.write_double(1.0);
    writer.write_double(2.5);
    writer.write_u64(3);
    writer.end_array();
    writer.write_key("empty");
    writer.begin_inline_array();
    writer.end_array();
    writer.end_object();

    EXPECT_FALSE(writer.has_failed() );
    EXPECT_EQ   (writer.get_text(),
                 "{\n"
                 "  \"values\": [1, 2.5, 3],\n"
                 "  \"empty\": []\n"
                 "}");
}

TEST(JsonWriter, AnInlineArrayInCompactTextHasNoSpaces)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_inline_array();
    writer.write_u64(1);
    writer.write_u64(2);
    writer.end_array();

    EXPECT_EQ(writer.get_text(),
              "[1,2]");
}

TEST(JsonWriter, AnInlineArrayHoldsNoContainers)
{
    Utils::Json::JsonWriter writer(false); /* in_is_pretty */

    writer.begin_inline_array();
    writer.begin_array();

    EXPECT_TRUE(writer.has_failed() );
}
