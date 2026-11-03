/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what tells which workload an analysis is of: its size, and the hashes of its start, its middle and its end.
 */
#include <gtest/gtest.h>
#include "Analysis/APIDumpAnalysis/workload_identity.h"
#include <fstream>
#include <vector>

static constexpr size_t N_FILE_BYTES = 40000;

/* Bytes which differ from place to place, so that a piece taken from the wrong place is told. */
static std::vector<uint8_t> make_content(const size_t& in_n_bytes)
{
    std::vector<uint8_t> result_u8_vec(in_n_bytes);

    for (size_t n_byte = 0;
                n_byte < in_n_bytes;
              ++n_byte)
    {
        result_u8_vec.at(n_byte) = static_cast<uint8_t>((n_byte * 31 + n_byte / 251) & 0xFF);
    }

    return result_u8_vec;
}

static std::string write_file(const std::string&          in_name,
                              const std::vector<uint8_t>& in_content_u8_vec)
{
    const std::string filename = ::testing::TempDir() + in_name;
    std::ofstream     file(filename,
                           std::ios::binary | std::ios::trunc);

    file.write(reinterpret_cast<const char*>(in_content_u8_vec.data() ),
               static_cast<std::streamsize>(in_content_u8_vec.size() ) );

    return filename;
}

TEST(WorkloadIdentity, AFileIsReferredToByItsSizeAndThreePieces)
{
    const std::string                           filename = write_file("identity_reference.bin",
                                                                      make_content(N_FILE_BYTES) );
    Analysis::APIDumpAnalysis::WorkloadReference reference;
    std::string                                 error;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::WorkloadIdentity::read(filename,
                                                                 &reference,
                                                                 &error) );

    EXPECT_EQ(reference.filename,
              filename);
    EXPECT_EQ(reference.n_bytes,
              N_FILE_BYTES);
    EXPECT_NE(reference.header_hash,
              0u);
    EXPECT_NE(reference.middle_hash,
              0u);
    EXPECT_NE(reference.tail_hash,
              0u);

    /* The pieces are from three places. */
    EXPECT_NE(reference.header_hash,
              reference.middle_hash);
    EXPECT_NE(reference.middle_hash,
              reference.tail_hash);
}

TEST(WorkloadIdentity, TheSameFileMatchesAndTheNameDoesNotMatter)
{
    const auto                                  content_u8_vec = make_content(N_FILE_BYTES);
    const std::string                           filename       = write_file("identity_same.bin",
                                                                            content_u8_vec);
    const std::string                           copy_filename  = write_file("identity_same_moved.bin",
                                                                            content_u8_vec);
    Analysis::APIDumpAnalysis::WorkloadReference reference;
    std::string                                 error;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::WorkloadIdentity::read(filename,
                                                                 &reference,
                                                                 &error) );

    EXPECT_EQ(Analysis::APIDumpAnalysis::WorkloadIdentity::verify(filename,
                                                                  reference,
                                                                 &error),
              Analysis::APIDumpAnalysis::WorkloadMatch::MATCHES);
    EXPECT_EQ(Analysis::APIDumpAnalysis::WorkloadIdentity::verify(copy_filename,
                                                                  reference,
                                                                 &error),
              Analysis::APIDumpAnalysis::WorkloadMatch::MATCHES);
}

TEST(WorkloadIdentity, AChangeOfSizeIsTold)
{
    auto                                        content_u8_vec = make_content(N_FILE_BYTES);
    const std::string                           filename       = write_file("identity_size.bin",
                                                                            content_u8_vec);
    Analysis::APIDumpAnalysis::WorkloadReference reference;
    std::string                                 error;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::WorkloadIdentity::read(filename,
                                                                 &reference,
                                                                 &error) );

    /* A longer file, and a shorter one. */
    content_u8_vec.push_back(0);

    EXPECT_EQ(Analysis::APIDumpAnalysis::WorkloadIdentity::verify(write_file("identity_size_longer.bin",
                                                                             content_u8_vec),
                                                                  reference,
                                                                 &error),
              Analysis::APIDumpAnalysis::WorkloadMatch::MISMATCH);

    content_u8_vec.resize(N_FILE_BYTES - 1);

    EXPECT_EQ(Analysis::APIDumpAnalysis::WorkloadIdentity::verify(write_file("identity_size_shorter.bin",
                                                                             content_u8_vec),
                                                                  reference,
                                                                 &error),
              Analysis::APIDumpAnalysis::WorkloadMatch::MISMATCH);
    EXPECT_FALSE(error.empty() );
}

TEST(WorkloadIdentity, AChangeInTheStartTheMiddleOrTheEndIsTold)
{
    constexpr size_t MIDDLE_OFFSET = (N_FILE_BYTES - Analysis::APIDumpAnalysis::WorkloadIdentity::N_PIECE_BYTES) / 2;

    const auto                                  content_u8_vec = make_content(N_FILE_BYTES);
    const std::string                           filename       = write_file("identity_pieces.bin",
                                                                            content_u8_vec);
    Analysis::APIDumpAnalysis::WorkloadReference reference;
    std::string                                 error;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::WorkloadIdentity::read(filename,
                                                                 &reference,
                                                                 &error) );

    const size_t offset_array[] =
    {
        0,
        MIDDLE_OFFSET + 100,
        N_FILE_BYTES  - 1
    };

    for (const size_t& offset : offset_array)
    {
        auto changed_u8_vec = content_u8_vec;

        changed_u8_vec.at(offset) ^= 0x5A;

        EXPECT_EQ(Analysis::APIDumpAnalysis::WorkloadIdentity::verify(write_file("identity_pieces_changed.bin",
                                                                                 changed_u8_vec),
                                                                      reference,
                                                                     &error),
                  Analysis::APIDumpAnalysis::WorkloadMatch::MISMATCH)
            << "a change at " << offset;
    }
}

TEST(WorkloadIdentity, AHashThatWasNotRecordedIsNotCompared)
{
    auto                                        content_u8_vec = make_content(N_FILE_BYTES);
    const std::string                           filename       = write_file("identity_old.bin",
                                                                            content_u8_vec);
    Analysis::APIDumpAnalysis::WorkloadReference reference;
    std::string                                 error;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::WorkloadIdentity::read(filename,
                                                                 &reference,
                                                                 &error) );

    /* What a .passes file holds: the size and the start. */
    reference.middle_hash = 0;
    reference.tail_hash   = 0;

    content_u8_vec.back() ^= 0x5A;

    EXPECT_EQ(Analysis::APIDumpAnalysis::WorkloadIdentity::verify(write_file("identity_old_changed.bin",
                                                                             content_u8_vec),
                                                                  reference,
                                                                 &error),
              Analysis::APIDumpAnalysis::WorkloadMatch::MATCHES);

    content_u8_vec.front() ^= 0x5A;

    EXPECT_EQ(Analysis::APIDumpAnalysis::WorkloadIdentity::verify(write_file("identity_old_changed_start.bin",
                                                                             content_u8_vec),
                                                                  reference,
                                                                 &error),
              Analysis::APIDumpAnalysis::WorkloadMatch::MISMATCH);
}

TEST(WorkloadIdentity, TwoAnalysesAreOfTheSameWorkloadIfTheirSizesAndTheHashesTheyBothHoldAreTheSame)
{
    Analysis::APIDumpAnalysis::WorkloadReference full;
    Analysis::APIDumpAnalysis::WorkloadReference start_only;

    full.filename    = "a.workload";
    full.header_hash = 0x11;
    full.middle_hash = 0x22;
    full.n_bytes     = 1000;
    full.tail_hash   = 0x33;

    /* What a .passes file holds. */
    start_only.filename    = "elsewhere.workload";
    start_only.header_hash = 0x11;
    start_only.n_bytes     = 1000;

    EXPECT_TRUE(Analysis::APIDumpAnalysis::WorkloadIdentity::is_same_workload(full,
                                                                              start_only) );
    EXPECT_TRUE(Analysis::APIDumpAnalysis::WorkloadIdentity::is_same_workload(start_only,
                                                                              full) );

    /* The hash that both hold differs, or the size does. */
    auto other_start = start_only;

    other_start.header_hash = 0x99;

    EXPECT_FALSE(Analysis::APIDumpAnalysis::WorkloadIdentity::is_same_workload(full,
                                                                               other_start) );

    auto other_size = start_only;

    other_size.n_bytes = 1001;

    EXPECT_FALSE(Analysis::APIDumpAnalysis::WorkloadIdentity::is_same_workload(full,
                                                                               other_size) );

    /* A hash that one of them holds and the other does not is not a difference; one that both hold is. */
    auto other_tail = full;

    other_tail.tail_hash = 0x44;

    EXPECT_FALSE(Analysis::APIDumpAnalysis::WorkloadIdentity::is_same_workload(full,
                                                                               other_tail) );
    EXPECT_TRUE (Analysis::APIDumpAnalysis::WorkloadIdentity::is_same_workload(start_only,
                                                                               other_tail) );
}

TEST(WorkloadIdentity, AFileShorterThanAPieceIsReferredTo)
{
    const std::string                           filename = write_file("identity_short.bin",
                                                                      make_content(100) );
    Analysis::APIDumpAnalysis::WorkloadReference reference;
    std::string                                 error;

    ASSERT_TRUE(Analysis::APIDumpAnalysis::WorkloadIdentity::read(filename,
                                                                 &reference,
                                                                 &error) );

    EXPECT_EQ(reference.n_bytes,
              100u);
    EXPECT_EQ(Analysis::APIDumpAnalysis::WorkloadIdentity::verify(filename,
                                                                  reference,
                                                                 &error),
              Analysis::APIDumpAnalysis::WorkloadMatch::MATCHES);
}

TEST(WorkloadIdentity, AFileThatIsNotThereCannotBeTold)
{
    Analysis::APIDumpAnalysis::WorkloadReference reference;
    std::string                                 error;

    reference.n_bytes = 1;

    EXPECT_FALSE(Analysis::APIDumpAnalysis::WorkloadIdentity::read(::testing::TempDir() + "identity_missing.bin",
                                                                  &reference,
                                                                  &error) );
    EXPECT_EQ(Analysis::APIDumpAnalysis::WorkloadIdentity::verify(::testing::TempDir() + "identity_missing.bin",
                                                                  reference,
                                                                 &error),
              Analysis::APIDumpAnalysis::WorkloadMatch::UNREADABLE);
    EXPECT_FALSE(error.empty() );
}
