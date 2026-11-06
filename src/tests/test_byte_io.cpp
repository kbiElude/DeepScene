/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for ByteWriter and ByteReader: what is written is read back, a read past the end fails, and a count which cannot fit
 * is refused.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/byte_reader.h"
#include "analysis/api_dump_analysis/byte_writer.h"

TEST(ByteIo, WhatIsWrittenIsReadBack)
{
    Analysis::APIDumpAnalysis::ByteWriter writer;

    writer.write_bool  (true);
    writer.write_f32   (1.5f);
    writer.write_string("abc");
    writer.write_u32   (0x12345678);
    writer.write_u64   (0x0102030405060708ull);
    writer.write_u8    (9);

    const auto                  byte_vec = writer.take_bytes();
    Analysis::APIDumpAnalysis::ByteReader reader(byte_vec.data(),
                                                 byte_vec.size() );

    EXPECT_TRUE(reader.read_bool() );
    EXPECT_EQ  (reader.read_f32(),
                1.5f);
    EXPECT_EQ  (reader.read_string(),
                "abc");
    EXPECT_EQ  (reader.read_u32(),
                0x12345678u);
    EXPECT_EQ  (reader.read_u64(),
                0x0102030405060708ull);
    EXPECT_EQ  (reader.read_u8(),
                9);
    EXPECT_TRUE(reader.is_at_end() );
    EXPECT_FALSE(reader.has_failed() );
}

TEST(ByteIo, TakingTheBytesLeavesTheWriterEmpty)
{
    Analysis::APIDumpAnalysis::ByteWriter writer;

    writer.write_u32(1);

    EXPECT_EQ(writer.take_bytes().size(),
              4u);
    EXPECT_TRUE(writer.get_bytes().empty() );
}

TEST(ByteIo, AReadPastTheEndFailsTheReader)
{
    const uint8_t               byte_array[2] = {1, 2};
    Analysis::APIDumpAnalysis::ByteReader reader(byte_array,
                                                 sizeof(byte_array) );

    EXPECT_EQ(reader.read_u32(),
              0u);
    EXPECT_TRUE(reader.has_failed() );
}

TEST(ByteIo, ACountOfMoreItemsThanCanFitIsRefused)
{
    Analysis::APIDumpAnalysis::ByteWriter writer;

    writer.write_u32(1000);
    writer.write_u64(1);

    const auto                  byte_vec = writer.take_bytes();
    Analysis::APIDumpAnalysis::ByteReader reader(byte_vec.data(),
                                                 byte_vec.size() );

    EXPECT_EQ(reader.read_count(sizeof(uint64_t) ),
              0u);
    EXPECT_TRUE(reader.has_failed() );
}

TEST(ByteIo, ACountWhichFitsIsAccepted)
{
    Analysis::APIDumpAnalysis::ByteWriter writer;

    writer.write_u32(2);
    writer.write_u64(1);
    writer.write_u64(2);

    const auto                  byte_vec = writer.take_bytes();
    Analysis::APIDumpAnalysis::ByteReader reader(byte_vec.data(),
                                                 byte_vec.size() );

    EXPECT_EQ(reader.read_count(sizeof(uint64_t) ),
              2u);
    EXPECT_FALSE(reader.has_failed() );
}
