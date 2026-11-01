/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the hashing which the match keys are built with. The pinned values must not change: keys are stored in files.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/hasher.h"
#include <limits>
#include <string.h>

TEST(Hasher, BytesAreHashedWithXXH64)
{
    const char text[] = "abc";

    /* The reference value of XXH64("abc") with seed 0. */
    EXPECT_EQ(Analysis::APIDumpAnalysis::Hasher::hash_bytes(text,
                                                            3),
              0x44BC2CF5AD770999ull);
}

TEST(Hasher, ScalarsAreAddedAsLittleEndianBytes)
{
    Analysis::APIDumpAnalysis::Hasher hasher;
    const uint8_t                     expected_bytes_u8vec7[7] = {0x78, 0x56, 0x34, 0x12, 0x01, 0xAA, 0x00};

    hasher.add_u32 (0x12345678);
    hasher.add_bool(true);
    hasher.add_u8  (0xAA);
    hasher.add_u8  (0x00);

    EXPECT_EQ(hasher.get_digest(),
              Analysis::APIDumpAnalysis::Hasher::hash_bytes(expected_bytes_u8vec7,
                                                            sizeof(expected_bytes_u8vec7) ));
}

TEST(Hasher, SixtyFourBitValuesAreLittleEndianToo)
{
    Analysis::APIDumpAnalysis::Hasher hasher;
    const uint8_t                     expected_bytes_u8vec8[8] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};

    hasher.add_u64(0x0102030405060708ull);

    EXPECT_EQ(hasher.get_digest(),
              Analysis::APIDumpAnalysis::Hasher::hash_bytes(expected_bytes_u8vec8,
                                                            sizeof(expected_bytes_u8vec8) ));
}

TEST(Hasher, TheOrderOfValuesMatters)
{
    Analysis::APIDumpAnalysis::Hasher a;
    Analysis::APIDumpAnalysis::Hasher b;

    a.add_u32(1);
    a.add_u32(2);
    b.add_u32(2);
    b.add_u32(1);

    EXPECT_NE(a.get_digest(),
              b.get_digest() );
}

TEST(Hasher, VariableSizedValuesCarryTheirSize)
{
    Analysis::APIDumpAnalysis::Hasher a;
    Analysis::APIDumpAnalysis::Hasher b;

    a.add_string("ab");
    a.add_string("c");
    b.add_string("a");
    b.add_string("bc");

    EXPECT_NE(a.get_digest(),
              b.get_digest() );
}

TEST(Hasher, NegativeZeroIsZero)
{
    Analysis::APIDumpAnalysis::Hasher a;
    Analysis::APIDumpAnalysis::Hasher b;

    a.add_f32( 0.0f);
    b.add_f32(-0.0f);

    EXPECT_EQ(a.get_digest(),
              b.get_digest() );
}

TEST(Hasher, AllNaNsAreTheSame)
{
    Analysis::APIDumpAnalysis::Hasher a;
    Analysis::APIDumpAnalysis::Hasher b;
    uint32_t                          other_nan_bits = 0x7FC12345;
    float                             other_nan      = 0.0f;

    ::memcpy(&other_nan,
             &other_nan_bits,
              sizeof(other_nan) );

    a.add_f32(std::numeric_limits<float>::quiet_NaN() );
    b.add_f32(other_nan);

    EXPECT_EQ(a.get_digest(),
              b.get_digest() );
}

TEST(Hasher, DifferentFloatsDiffer)
{
    Analysis::APIDumpAnalysis::Hasher a;
    Analysis::APIDumpAnalysis::Hasher b;

    a.add_f32(1.0f);
    b.add_f32(1.0000001f);

    EXPECT_NE(a.get_digest(),
              b.get_digest() );
}

TEST(Hasher, EmptyHasherHasTheDigestOfNoBytes)
{
    Analysis::APIDumpAnalysis::Hasher hasher;

    EXPECT_EQ(hasher.get_digest(),
              Analysis::APIDumpAnalysis::Hasher::hash_bytes(nullptr,
                                                            0) );
}

TEST(Hasher, ManyBytesAreHashedAsTheyWouldBeIfFew)
{
    constexpr size_t N_BYTES_PER_STEP = 8;

    /* Steps of several sizes, so that the 2048-byte limit of what is kept inside the hasher falls in the middle of a value. */
    for (size_t n_steps = 250;
                n_steps < 265;
              ++n_steps)
    {
        Analysis::APIDumpAnalysis::Hasher hasher;
        std::vector<uint8_t>              expected_u8_vec = {9};

        hasher.add_u8(9);

        for (size_t n_step = 0;
                    n_step < n_steps;
                  ++n_step)
        {
            hasher.add_u8 (static_cast<uint8_t>(n_step) );
            hasher.add_u32(0x01020304 + static_cast<uint32_t>(n_step) );
            hasher.add_u8 (static_cast<uint8_t>(n_step * 3) );
            hasher.add_u8 (1);
            hasher.add_u8 (2);

            expected_u8_vec.push_back(static_cast<uint8_t>(n_step) );

            for (size_t n_byte = 0;
                        n_byte < sizeof(uint32_t);
                      ++n_byte)
            {
                expected_u8_vec.push_back(static_cast<uint8_t>((0x01020304 + n_step) >> (8 * n_byte) ) );
            }

            expected_u8_vec.push_back(static_cast<uint8_t>(n_step * 3) );
            expected_u8_vec.push_back(1);
            expected_u8_vec.push_back(2);
        }

        ASSERT_EQ(expected_u8_vec.size(),
                  1 + n_steps * N_BYTES_PER_STEP);
        EXPECT_EQ(hasher.get_digest(),
                  Analysis::APIDumpAnalysis::Hasher::hash_bytes(expected_u8_vec.data(),
                                                                expected_u8_vec.size() ) ) << n_steps;
    }
}

TEST(Hasher, ABigBlockIsHashedWithItsSizeInFront)
{
    const std::vector<uint8_t> block_u8_vec(5000,
                                            0x5a);
    Analysis::APIDumpAnalysis::Hasher hasher;
    std::vector<uint8_t>              expected_u8_vec = {0x88, 0x13, 0, 0, 0, 0, 0, 0}; /* 5000, as 64 bits */

    hasher.add_bytes(block_u8_vec.data(),
                     block_u8_vec.size() );

    expected_u8_vec.insert(expected_u8_vec.end(),
                           block_u8_vec.begin(),
                           block_u8_vec.end  () );

    EXPECT_EQ(hasher.get_digest(),
              Analysis::APIDumpAnalysis::Hasher::hash_bytes(expected_u8_vec.data(),
                                                            expected_u8_vec.size() ) );
}
