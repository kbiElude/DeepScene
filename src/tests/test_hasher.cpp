/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the hashing which the match keys are built with. The pinned values must not change: keys are stored in files.
 */
#include <gtest/gtest.h>
#include "utils/hashing/hasher.h"
#include <limits>
#include <string.h>
#include <vector>

TEST(Hasher, BytesAreHashedWithXXH64)
{
    const char text[] = "abc";

    /* The reference value of XXH64("abc") with seed 0. */
    EXPECT_EQ(Utils::Hashing::Hasher::hash_bytes(text,
                                                 3),
              0x44BC2CF5AD770999ull);
}

TEST(Hasher, ScalarsAreAddedAsLittleEndianBytes)
{
    Utils::Hashing::Hasher hasher;
    const uint8_t                     expected_bytes_u8vec7[7] = {0x78, 0x56, 0x34, 0x12, 0x01, 0xAA, 0x00};

    hasher.add_u32 (0x12345678);
    hasher.add_bool(true);
    hasher.add_u8  (0xAA);
    hasher.add_u8  (0x00);

    EXPECT_EQ(hasher.get_digest(),
              Utils::Hashing::Hasher::hash_bytes(expected_bytes_u8vec7,
                                                 sizeof(expected_bytes_u8vec7) ));
}

TEST(Hasher, SixtyFourBitValuesAreLittleEndianToo)
{
    Utils::Hashing::Hasher hasher;
    const uint8_t                     expected_bytes_u8vec8[8] = {0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01};

    hasher.add_u64(0x0102030405060708ull);

    EXPECT_EQ(hasher.get_digest(),
              Utils::Hashing::Hasher::hash_bytes(expected_bytes_u8vec8,
                                                 sizeof(expected_bytes_u8vec8) ));
}

TEST(Hasher, TheOrderOfValuesMatters)
{
    Utils::Hashing::Hasher a;
    Utils::Hashing::Hasher b;

    a.add_u32(1);
    a.add_u32(2);
    b.add_u32(2);
    b.add_u32(1);

    EXPECT_NE(a.get_digest(),
              b.get_digest() );
}

TEST(Hasher, VariableSizedValuesCarryTheirSize)
{
    Utils::Hashing::Hasher a;
    Utils::Hashing::Hasher b;

    a.add_string("ab");
    a.add_string("c");
    b.add_string("a");
    b.add_string("bc");

    EXPECT_NE(a.get_digest(),
              b.get_digest() );
}

TEST(Hasher, NegativeZeroIsZero)
{
    Utils::Hashing::Hasher a;
    Utils::Hashing::Hasher b;

    a.add_f32( 0.0f);
    b.add_f32(-0.0f);

    EXPECT_EQ(a.get_digest(),
              b.get_digest() );
}

TEST(Hasher, AllNaNsAreTheSame)
{
    Utils::Hashing::Hasher a;
    Utils::Hashing::Hasher b;
    uint32_t               other_nan_bits = 0x7FC12345;
    float                  other_nan      = 0.0f;

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
    Utils::Hashing::Hasher a;
    Utils::Hashing::Hasher b;

    a.add_f32(1.0f);
    b.add_f32(1.0000001f);

    EXPECT_NE(a.get_digest(),
              b.get_digest() );
}

TEST(Hasher, EmptyHasherHasTheDigestOfNoBytes)
{
    Utils::Hashing::Hasher hasher;

    EXPECT_EQ(hasher.get_digest(),
              Utils::Hashing::Hasher::hash_bytes(nullptr,
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
        Utils::Hashing::Hasher hasher;
        std::vector<uint8_t>   expected_u8_vec = {9};

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
                  Utils::Hashing::Hasher::hash_bytes(expected_u8_vec.data(),
                                                     expected_u8_vec.size() ) ) << n_steps;
    }
}

TEST(Hasher, ABigBlockIsHashedWithItsSizeInFront)
{
    const std::vector<uint8_t> block_u8_vec(5000,
                                            0x5a);
    Utils::Hashing::Hasher hasher;
    std::vector<uint8_t>   expected_u8_vec = {0x88, 0x13, 0, 0, 0, 0, 0, 0}; /* 5000, as 64 bits */

hasher.add_bytes(block_u8_vec.data(),
          block_u8_vec.size() );

    expected_u8_vec.insert(expected_u8_vec.end(),
                           block_u8_vec.begin(),
                           block_u8_vec.end  () );

    EXPECT_EQ(hasher.get_digest(),
              Utils::Hashing::Hasher::hash_bytes(expected_u8_vec.data(),
                                                 expected_u8_vec.size() ) );
}

namespace
{
    /* The bytes that the pinned digests below are of. */
    std::vector<uint8_t> get_pattern(const size_t& in_n_bytes)
    {
        std::vector<uint8_t> result(in_n_bytes);

        for (size_t n_byte = 0;
                    n_byte < in_n_bytes;
                  ++n_byte)
        {
            result.at(n_byte) = static_cast<uint8_t>(n_byte * 31 + 7);
        }

        return result;
    }
}

TEST(Hasher, BytesOfEverySizeAreHashedAsXXH64HashesThem)
{
    /* Reference values of XXH64 with seed 0 of the pattern of bytes, around the sizes where the way it is done changes (4, 8, 32 bytes, and the 64 from which the stripes are done by AVX2). */
    struct Pinned
    {
        size_t   n_bytes;
        uint64_t digest;
    };

    const Pinned pinned_array[] =
    {
        {0, 0xEF46DB3751D8E999ull},
        {1, 0xA96C7F0CE858BBB7ull},
        {3, 0x56E6957632A487F9ull},
        {4, 0xC60D15B1E3FF8F04ull},
        {7, 0xAFBEFC3D6C6F9A8Eull},
        {8, 0x3DA5C7AA269683E0ull},
        {15, 0xAE2A37EB9357CAA7ull},
        {31, 0x4A74F3A1A39AD4A1ull},
        {32, 0x8D57D6A4671CC43Dull},
        {33, 0x62C9FD21ED857664ull},
        {63, 0x5C320A0D2707057Full},
        {64, 0x7BBABBC45729D17Eull},
        {65, 0xF3980C34BAE65DC1ull},
        {95, 0x90F6CB6D1DB41380ull},
        {96, 0x1A4B207385051B55ull},
        {100, 0xEFA0AD2D3E70C151ull},
        {127, 0x4822F4E67F60EA91ull},
        {128, 0x725A5B9B3BEDFE94ull},
        {255, 0x2C3DB4BB567F731Eull},
        {256, 0x7C1FF7B1D57C10D5ull},
        {257, 0x6FF15897658784E0ull},
        {1000, 0x99594F4828043D35ull},
        {1024, 0x149AA44972CDAE00ull},
        {4096, 0xE21174BE82DC78D9ull},
        {4097, 0xD42496BBBECACB1Dull},
        {65536, 0xCE06607392C5113Full},
        {100000, 0x3AC9CBC5A9B7F843ull},
    };

    const auto pattern_vec = get_pattern(100000);

    for (const Pinned& pinned : pinned_array)
    {
        EXPECT_EQ(Utils::Hashing::Hasher::hash_bytes(pattern_vec.data(),
                                                     pinned.n_bytes),
                  pinned.digest) << pinned.n_bytes << " bytes";
    }
}

TEST(Hasher, BytesAreHashedTheSameWhereverTheyStartInMemory)
{
    for (const size_t n_bytes : {63, 64, 65, 96, 100, 255, 1000, 4000})
    {
        const auto pattern_vec = get_pattern(n_bytes);

        for (size_t n_offset = 0;
                    n_offset < 8;
                  ++n_offset)
        {
            /* The same bytes at an address that is n_offset more than the one of a vector of its own. */
            std::vector<uint8_t> shifted_vec(n_offset + n_bytes,
                                             0xA5);

            ::memcpy(shifted_vec.data() + n_offset,
                     pattern_vec.data(),
                     n_bytes);

            EXPECT_EQ(Utils::Hashing::Hasher::hash_bytes(shifted_vec.data() + n_offset,
                                                         n_bytes),
                      Utils::Hashing::Hasher::hash_bytes(pattern_vec.data(),
                                                         n_bytes) ) << n_bytes << " bytes, offset " << n_offset;
        }
    }
}
