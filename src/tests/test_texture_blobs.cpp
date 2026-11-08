/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for the storing of the contents of textures: the codec (nothing is lost, and a damaged or wrong content is refused), the writer (each content once) and
 * the reader (any content alone).
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_textures/texture_blob_codec.h"
#include "analysis/api_dump_textures/texture_blob_reader.h"
#include "analysis/api_dump_textures/texture_blob_writer.h"
#include "temp_directory.h"
#include <fstream>

namespace
{
    typedef Analysis::APIDumpTextures::BlobEntry         BlobEntry;
    typedef Analysis::APIDumpTextures::BlobFilter        BlobFilter;
    typedef Analysis::APIDumpTextures::TextureBlobCodec  TextureBlobCodec;
    typedef Analysis::APIDumpTextures::TextureBlobReader TextureBlobReader;
    typedef Analysis::APIDumpTextures::TextureBlobWriter TextureBlobWriter;

    /* Bytes that nothing compresses, the same for the same seed. */
    std::vector<uint8_t> make_noise(const size_t&   in_n_bytes,
                                    const uint32_t& in_seed)
    {
        std::vector<uint8_t> result_vec(in_n_bytes);
        uint32_t             state = in_seed * 2654435761u + 1;

        for (auto& byte : result_vec)
        {
            state = state * 1664525u + 1013904223u;
            byte  = static_cast<uint8_t>(state >> 24);
        }

        return result_vec;
    }

    /* Pixels of four bytes in which only a plane changes, as a gradient does. */
    std::vector<uint8_t> make_gradient(const size_t& in_n_pixels)
    {
        std::vector<uint8_t> result_vec(in_n_pixels * 4);

        for (size_t n_pixel = 0; n_pixel < in_n_pixels; ++n_pixel)
        {
            result_vec.at(n_pixel * 4 + 0) = static_cast<uint8_t>(n_pixel);
            result_vec.at(n_pixel * 4 + 1) = 17;
            result_vec.at(n_pixel * 4 + 2) = 99;
            result_vec.at(n_pixel * 4 + 3) = 255;
        }

        return result_vec;
    }

    /* The entry that goes with the stored bytes of a content. */
    BlobEntry make_entry(const std::vector<uint8_t>& in_raw_byte_vec,
                         const std::vector<uint8_t>& in_stored_byte_vec,
                         const BlobFilter&           in_filter)
    {
        BlobEntry result;

        TextureBlobCodec::compute_hashes(in_raw_byte_vec.data(),
                                         in_raw_byte_vec.size(),
                                        &result.content_hash,
                                        &result.length_hash);

        result.filter         = in_filter;
        result.n_raw_bytes    = in_raw_byte_vec.size();
        result.n_stored_bytes = in_stored_byte_vec.size();

        return result;
    }

    void flip_byte(const std::string& in_filename,
                   const uint64_t&    in_offset)
    {
        std::fstream file(in_filename,
                          std::ios::binary | std::ios::in | std::ios::out);
        char         byte = 0;

        file.seekg(static_cast<std::streamoff>(in_offset) );
        file.read (&byte,
                   1);

        byte = static_cast<char>(byte ^ 0x40);

        file.seekp(static_cast<std::streamoff>(in_offset) );
        file.write(&byte,
                   1);
    }
}

TEST(TextureBlobCodec, AContentComesBackAsItWasGivenWhateverItsSizeAndFilter)
{
    for (const BlobFilter filter : {BlobFilter::NONE, BlobFilter::BYTE_PLANES_OF_4})
    {
        for (const size_t n_bytes : {0u, 1u, 3u, 4u, 5u, 7u, 4096u, 4099u, 100003u})
        {
            const auto           raw_byte_vec = make_noise(n_bytes,
                                                           static_cast<uint32_t>(n_bytes) );
            std::vector<uint8_t> stored_byte_vec;
            std::vector<uint8_t> unpacked_byte_vec;
            std::string          error;

            ASSERT_TRUE(TextureBlobCodec::pack(raw_byte_vec.data(),
                                               raw_byte_vec.size(),
                                               filter,
                                              &stored_byte_vec) );

            const BlobEntry entry = make_entry(raw_byte_vec,
                                               stored_byte_vec,
                                               filter);

            ASSERT_TRUE(TextureBlobCodec::unpack(stored_byte_vec.data(),
                                                 stored_byte_vec.size(),
                                                 entry,
                                                &unpacked_byte_vec,
                                                &error) ) << error;

            EXPECT_EQ(unpacked_byte_vec,
                      raw_byte_vec);
        }
    }
}

TEST(TextureBlobCodec, ThePlanesOfThePixelsMakeAGradientSmaller)
{
    const auto           gradient_byte_vec = make_gradient(4096);
    std::vector<uint8_t> plain_byte_vec;
    std::vector<uint8_t> planes_byte_vec;

    ASSERT_TRUE(TextureBlobCodec::pack(gradient_byte_vec.data(),
                                       gradient_byte_vec.size(),
                                       BlobFilter::NONE,
                                      &plain_byte_vec) );
    ASSERT_TRUE(TextureBlobCodec::pack(gradient_byte_vec.data(),
                                       gradient_byte_vec.size(),
                                       BlobFilter::BYTE_PLANES_OF_4,
                                      &planes_byte_vec) );

    EXPECT_LT(planes_byte_vec.size(),
              plain_byte_vec.size() );
}

TEST(TextureBlobCodec, AContentWhoseEntryHasOtherHashesIsRefused)
{
    const auto           raw_byte_vec = make_noise(1000,
                                                   3);
    std::vector<uint8_t> stored_byte_vec;
    std::vector<uint8_t> unpacked_byte_vec;
    std::string          error;

    ASSERT_TRUE(TextureBlobCodec::pack(raw_byte_vec.data(),
                                       raw_byte_vec.size(),
                                       BlobFilter::NONE,
                                      &stored_byte_vec) );

    BlobEntry entry = make_entry(raw_byte_vec,
                                 stored_byte_vec,
                                 BlobFilter::NONE);

    entry.length_hash ^= 1;

    EXPECT_FALSE(TextureBlobCodec::unpack(stored_byte_vec.data(),
                                          stored_byte_vec.size(),
                                          entry,
                                         &unpacked_byte_vec,
                                         &error) );
    EXPECT_TRUE (unpacked_byte_vec.empty() );
}

TEST(TextureBlobCodec, AStoredContentWithAByteFlippedIsRefused)
{
    const auto           raw_byte_vec = make_noise(5000,
                                                   9);
    std::vector<uint8_t> stored_byte_vec;
    std::vector<uint8_t> unpacked_byte_vec;
    std::string          error;

    ASSERT_TRUE(TextureBlobCodec::pack(raw_byte_vec.data(),
                                       raw_byte_vec.size(),
                                       BlobFilter::NONE,
                                      &stored_byte_vec) );

    const BlobEntry entry = make_entry(raw_byte_vec,
                                       stored_byte_vec,
                                       BlobFilter::NONE);

    for (const size_t n_byte : {stored_byte_vec.size() / 2, stored_byte_vec.size() - 1})
    {
        auto damaged_byte_vec = stored_byte_vec;

        damaged_byte_vec.at(n_byte) ^= 0x08;

        EXPECT_FALSE(TextureBlobCodec::unpack(damaged_byte_vec.data(),
                                              damaged_byte_vec.size(),
                                              entry,
                                             &unpacked_byte_vec,
                                             &error) );
    }
}

TEST(TextureBlobWriter, ThePlaceOfAContentThatIsAddedAgainIsTheSameAndItIsNotStoredAgain)
{
    Tests::TempDirectory directory;
    TextureBlobWriter    writer;
    std::string          error;
    uint32_t             first_index  = 0;
    uint32_t             second_index = 0;
    uint32_t             other_index  = 0;
    const auto           content      = make_noise(3000,
                                                   1);
    const auto           other        = make_noise(3000,
                                                   2);

    ASSERT_TRUE(writer.open(directory.get_path() + "\\blobs.data",
                           &error) ) << error;

    ASSERT_TRUE(writer.add(content.data(), content.size(), BlobFilter::NONE, &first_index,  &error) ) << error;

    const uint64_t n_bytes_after_first = writer.get_n_bytes();

    ASSERT_TRUE(writer.add(content.data(), content.size(), BlobFilter::BYTE_PLANES_OF_4, &second_index, &error) ) << error;

    EXPECT_EQ(second_index,
              first_index);
    EXPECT_EQ(writer.get_n_bytes(),
              n_bytes_after_first);
    EXPECT_EQ(writer.get_blob_vec().size(),
              1u);

    ASSERT_TRUE(writer.add(other.data(), other.size(), BlobFilter::NONE, &other_index, &error) ) << error;

    EXPECT_NE(other_index,
              first_index);
    EXPECT_EQ(writer.get_blob_vec().size(),
              2u);

    ASSERT_TRUE(writer.close(&error) ) << error;
}

TEST(TextureBlobReader, EveryContentComesBackAloneAndAsItWasGiven)
{
    Tests::TempDirectory              directory;
    TextureBlobWriter                 writer;
    TextureBlobReader                 reader;
    std::string                       error;
    std::vector<std::vector<uint8_t>> content_vec;
    std::vector<uint32_t>             index_vec;
    const std::string                 filename = directory.get_path() + "\\blobs.data";

    content_vec.push_back(make_noise(10,    1));
    content_vec.push_back(make_gradient(2000));
    content_vec.push_back(make_noise(70001, 2));
    content_vec.push_back(std::vector<uint8_t>() );
    content_vec.push_back(make_gradient(1));

    ASSERT_TRUE(writer.open(filename,
                           &error) ) << error;

    for (size_t n_content = 0; n_content < content_vec.size(); ++n_content)
    {
        uint32_t index = 0;

        ASSERT_TRUE(writer.add(content_vec.at(n_content).data(),
                               content_vec.at(n_content).size(),
                               (n_content % 2 == 0) ? BlobFilter::NONE : BlobFilter::BYTE_PLANES_OF_4,
                              &index,
                              &error) ) << error;

        index_vec.push_back(index);
    }

    ASSERT_TRUE(writer.close(&error) ) << error;
    ASSERT_TRUE(reader.open(filename,
                           &error) ) << error;

    EXPECT_EQ(reader.get_n_bytes(),
              writer.get_n_bytes() );

    /* Backwards, so that the order of the file is not what reads them. */
    for (size_t n_content = content_vec.size(); n_content-- > 0;)
    {
        std::vector<uint8_t> read_byte_vec;

        ASSERT_TRUE(reader.read(writer.get_blob_vec().at(index_vec.at(n_content) ),
                               &read_byte_vec,
                               &error) ) << error;

        EXPECT_EQ(read_byte_vec,
                  content_vec.at(n_content) );
    }
}

TEST(TextureBlobReader, ADamagedFileAndAnEntryOutsideItAreRefused)
{
    Tests::TempDirectory directory;
    TextureBlobWriter    writer;
    TextureBlobReader    reader;
    std::string          error;
    uint32_t             index    = 0;
    const auto           content  = make_noise(4000,
                                               5);
    const std::string    filename = directory.get_path() + "\\blobs.data";
    std::vector<uint8_t> read_byte_vec;

    ASSERT_TRUE(writer.open(filename,
                           &error) ) << error;
    ASSERT_TRUE(writer.add(content.data(),
                           content.size(),
                           BlobFilter::NONE,
                          &index,
                          &error) ) << error;
    ASSERT_TRUE(writer.close(&error) ) << error;

    BlobEntry entry = writer.get_blob_vec().at(index);

    /* An entry that says what the file does not have. */
    {
        BlobEntry outside = entry;

        outside.file_offset_n_bytes = writer.get_n_bytes() + 10;

        ASSERT_TRUE(reader.open(filename,
                               &error) ) << error;

        EXPECT_FALSE(reader.read(outside,
                                &read_byte_vec,
                                &error) );
    }

    flip_byte(filename,
              entry.file_offset_n_bytes + entry.n_stored_bytes / 2);

    ASSERT_TRUE(reader.open(filename,
                           &error) ) << error;

    EXPECT_FALSE(reader.read(entry,
                            &read_byte_vec,
                            &error) );
}

TEST(TextureBlobReader, AFileThatIsNotAFileOfBlobsIsRefused)
{
    Tests::TempDirectory directory;
    TextureBlobReader    reader;
    std::string          error;
    const std::string    filename = directory.get_path() + "\\other.data";

    {
        std::ofstream file(filename,
                           std::ios::binary);

        file << "This is not a file of blobs, though it is long enough to have a header.";
    }

    EXPECT_FALSE(reader.open(filename,
                            &error) );
    EXPECT_FALSE(reader.open(directory.get_path() + "\\missing.data",
                            &error) );
}

TEST(TextureBlobWriter, AContentLeftInTheWorkloadHasAnEntryAndNoBytesInTheFile)
{
    Tests::TempDirectory directory;
    TextureBlobWriter    writer;
    std::string          error;
    uint32_t             first_index  = 0;
    uint32_t             again_index  = 0;
    uint32_t             other_index  = 0;
    uint32_t             stored_index = 0;
    const auto           content      = make_noise(3000,
                                                   4);
    const auto           other        = make_noise(500,
                                                   6);

    ASSERT_TRUE(writer.open(directory.get_path() + "\\blobs.data",
                           &error) ) << error;

    const uint64_t n_bytes_of_header = writer.get_n_bytes();

    ASSERT_TRUE(writer.add_workload_reference(content.data(),
                                              content.size(),
                                              12,
                                             &first_index,
                                             &error) ) << error;
    ASSERT_TRUE(writer.add_workload_reference(content.data(),
                                              content.size(),
                                              12,
                                             &again_index,
                                             &error) ) << error;
    ASSERT_TRUE(writer.add_workload_reference(other.data(),
                                              other.size(),
                                              13,
                                             &other_index,
                                             &error) ) << error;

    EXPECT_EQ(again_index,
              first_index);
    EXPECT_NE(other_index,
              first_index);
    /* Nothing of them is written. */
    EXPECT_EQ(writer.get_n_bytes(),
              n_bytes_of_header);

    const BlobEntry entry = writer.get_blob_vec().at(first_index);

    EXPECT_EQ(entry.location,
              Analysis::APIDumpTextures::BlobLocation::WORKLOAD_CHUNK);
    EXPECT_EQ(entry.data_chunk_id,
              12u);
    EXPECT_EQ(entry.n_raw_bytes,
              content.size() );

    uint64_t content_hash = 0;
    uint64_t length_hash  = 0;

    TextureBlobCodec::compute_hashes(content.data(),
                                     content.size(),
                                    &content_hash,
                                    &length_hash);

    /* The hashes are those of the bytes, so that the chunk that is read from the workload can be told to be the one. */
    EXPECT_EQ(entry.content_hash,
              content_hash);
    EXPECT_EQ(entry.length_hash,
              length_hash);

    /* A stored content added after them has its own place in the one table. */
    ASSERT_TRUE(writer.add(content.data(),
                           content.size(),
                           BlobFilter::NONE,
                          &stored_index,
                          &error) ) << error;

    EXPECT_NE(stored_index,
              first_index);
}

TEST(TextureBlobReader, AContentThatIsInTheWorkloadIsNotInTheFileOfBlobs)
{
    Tests::TempDirectory directory;
    TextureBlobWriter    writer;
    TextureBlobReader    reader;
    std::string          error;
    uint32_t             index   = 0;
    const auto           content = make_noise(100,
                                              8);
    std::vector<uint8_t> read_byte_vec;

    ASSERT_TRUE(writer.open(directory.get_path() + "\\blobs.data",
                           &error) ) << error;
    ASSERT_TRUE(writer.add_workload_reference(content.data(),
                                              content.size(),
                                              3,
                                             &index,
                                             &error) ) << error;
    ASSERT_TRUE(writer.close(&error) ) << error;
    ASSERT_TRUE(reader.open(directory.get_path() + "\\blobs.data",
                           &error) ) << error;

    EXPECT_FALSE(reader.read(writer.get_blob_vec().at(index),
                            &read_byte_vec,
                            &error) );
}
