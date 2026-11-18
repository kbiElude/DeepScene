/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the index of a workload: where its segments and frames are, that a reader can start at a segment and gets the calls and the chunks it would
 * have got on the way, and that an index is only used for the file it was made of.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_index/workload_index.h"
#include "workload_file_builder.h"
#include <stdio.h>

namespace
{
    constexpr uint32_t N_CALLS_PER_FRAME   = 4; /* and a fifth that ends it                  */
    constexpr uint32_t N_CALLS_PER_SEGMENT = 7; /* a segment does not hold a whole frame     */
    constexpr uint32_t N_SEGMENTS          = 6;

    /* A file of segments, which is removed with this. */
    class IndexedFile
    {
    public:
        IndexedFile()
        {
            std::vector<uint8_t> file_u8_vec;

            Tests::WorkloadFileBuilder::build_segments(&file_u8_vec,
                                                       N_SEGMENTS,
                                                       N_CALLS_PER_SEGMENT,
                                                       N_CALLS_PER_FRAME);
            Tests::WorkloadFileBuilder::write_file    (file_u8_vec);
        }

        ~IndexedFile()
        {
            ::remove(Tests::WorkloadFileBuilder::get_filename_ptr() );
            ::remove(Analysis::APIDumpIndex::WorkloadIndex::get_default_filename(Tests::WorkloadFileBuilder::get_filename_ptr() ).c_str() );
        }
    };
}

TEST(WorkloadIndex, TheSegmentsAreWhereTheFileHasThemWithTheirCallsAndFrames)
{
    IndexedFile                           file;
    Analysis::APIDumpIndex::WorkloadIndex index;
    std::string                           error;

    ASSERT_TRUE(Analysis::APIDumpIndex::WorkloadIndex::build(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                             0,
                                                            &index,
                                                            &error) ) << error;

    const auto& segment_vec = index.get_segment_vec();

    ASSERT_EQ(segment_vec.size(),
              static_cast<size_t>(N_SEGMENTS) );
    EXPECT_EQ(index.get_n_calls(),
              static_cast<uint64_t>(N_SEGMENTS * N_CALLS_PER_SEGMENT) );

    /* Six segments of seven calls; every fifth call ends a frame: calls 4, 9, 14, ... (42 calls, 8 frames end, and 2 calls are of a ninth). */
    EXPECT_EQ(index.get_n_frames(),
              9u);

    for (size_t n_segment = 0;
                n_segment < segment_vec.size();
              ++n_segment)
    {
        const auto& entry = segment_vec.at(n_segment);

        EXPECT_EQ(entry.first_call_index,
                  n_segment * N_CALLS_PER_SEGMENT);
        EXPECT_EQ(entry.first_data_chunk_id,
                  n_segment);
        EXPECT_EQ(entry.n_calls,
                  N_CALLS_PER_SEGMENT);
        EXPECT_EQ(entry.n_data_chunks,
                  1u);

        if (n_segment != 0)
        {
            EXPECT_EQ(entry.file_offset_n_bytes,
                      segment_vec.at(n_segment - 1).file_offset_n_bytes + segment_vec.at(n_segment - 1).n_compressed_bytes);
        }
    }

    /* The first frame that a segment has calls of is not after the last of the one before. */
    EXPECT_EQ(segment_vec.at(1).first_frame_index,
              segment_vec.at(0).first_frame_index + segment_vec.at(0).n_frames_ended);
}

TEST(WorkloadIndex, TheSegmentOfAFrameIsTheFirstOneThatHasACallOfIt)
{
    IndexedFile                           file;
    Analysis::APIDumpIndex::WorkloadIndex index;
    size_t                                n_segment = 0;
    std::string                           error;

    ASSERT_TRUE(Analysis::APIDumpIndex::WorkloadIndex::build(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                             0,
                                                            &index,
                                                            &error) ) << error;

    /* Frame 0 is calls 0..4, frame 1 is calls 5..9: it starts in the first segment (calls 0..6) and ends in the second (calls 7..13). */
    ASSERT_TRUE(index.find_segment_of_frame(0,
                                           &n_segment) );
    EXPECT_EQ(n_segment,
              0u);
    ASSERT_TRUE(index.find_segment_of_frame(1,
                                           &n_segment) );
    EXPECT_EQ(n_segment,
              0u);

    /* Frame 2 is calls 10..14: the second segment has calls 10..13. */
    ASSERT_TRUE(index.find_segment_of_frame(2,
                                           &n_segment) );
    EXPECT_EQ(n_segment,
              1u);

    /* The frame that is still going on at the end of the file is the last one; there is none after it. */
    ASSERT_TRUE(index.find_segment_of_frame(index.get_n_frames() - 1,
                                           &n_segment) );
    EXPECT_EQ(n_segment,
              static_cast<size_t>(N_SEGMENTS - 1) );
    EXPECT_FALSE(index.find_segment_of_frame(index.get_n_frames(),
                                            &n_segment) );
}

TEST(WorkloadIndex, AReaderWhichStartsAtASegmentGetsWhatTheReaderWhichReadOnToItGets)
{
    constexpr size_t N_SEGMENT_TO_START_AT = 3;

    IndexedFile                           file;
    Analysis::APIDumpIndex::WorkloadIndex index;
    std::string                           error;

    ASSERT_TRUE(Analysis::APIDumpIndex::WorkloadIndex::build(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                             0,
                                                            &index,
                                                            &error) ) << error;

    /* The one that read on: its third segment is what the other starts with. */
    auto                             on_reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(Tests::WorkloadFileBuilder::get_filename_ptr() );
    Capture::APIDumpLoader::Workload on_segment;

    ASSERT_NE(on_reader_ptr,
              nullptr);

    for (size_t n_segment = 0;
                n_segment <= N_SEGMENT_TO_START_AT;
              ++n_segment)
    {
        ASSERT_TRUE(on_reader_ptr->read_next_segment(&on_segment) );
    }

    auto                             start_reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                                                                         index.make_reader_options(N_SEGMENT_TO_START_AT,
                                                                                                                                   Capture::APIDumpLoader::WorkloadReader::Options() ) );
    Capture::APIDumpLoader::Workload start_segment;

    ASSERT_NE(start_reader_ptr,
              nullptr);
    ASSERT_TRUE(start_reader_ptr->read_next_segment(&start_segment) );

    EXPECT_EQ(start_segment.dumped_api_call_vec_ptr->size(),
              on_segment.dumped_api_call_vec_ptr->size() );
    EXPECT_EQ(start_segment.first_data_chunk_id,
              on_segment.first_data_chunk_id);
    EXPECT_EQ(start_segment.file_offset_n_bytes,
              on_segment.file_offset_n_bytes);
    EXPECT_EQ(start_segment.dumped_api_call_vec_ptr->front().timestamp_qpc,
              on_segment.dumped_api_call_vec_ptr->front().timestamp_qpc);

    /* And the one after it is the one after it. */
    ASSERT_TRUE(on_reader_ptr->read_next_segment(&on_segment) );
    ASSERT_TRUE(start_reader_ptr->read_next_segment(&start_segment) );

    EXPECT_EQ(start_segment.first_data_chunk_id,
              on_segment.first_data_chunk_id);
}

TEST(WorkloadIndex, TheIndexIsKeptInAFileAndIsReadBackAsItWas)
{
    IndexedFile                           file;
    Analysis::APIDumpIndex::WorkloadIndex built_index;
    Analysis::APIDumpIndex::WorkloadIndex loaded_index;
    bool                                  was_built = false;
    std::string                           error;

    ASSERT_TRUE(Analysis::APIDumpIndex::WorkloadIndex::load_or_build(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                                     0,
                                                                    &built_index,
                                                                    &was_built,
                                                                    &error) ) << error;
    EXPECT_TRUE(was_built);

    ASSERT_TRUE(Analysis::APIDumpIndex::WorkloadIndex::load_or_build(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                                     0,
                                                                    &loaded_index,
                                                                    &was_built,
                                                                    &error) ) << error;
    EXPECT_FALSE(was_built); /* the file of the first time is used */

    EXPECT_EQ(loaded_index.get_n_calls(),
              built_index.get_n_calls() );
    EXPECT_EQ(loaded_index.get_n_frames(),
              built_index.get_n_frames() );
    ASSERT_EQ(loaded_index.get_segment_vec().size(),
              built_index.get_segment_vec().size() );
    EXPECT_EQ(loaded_index.get_segment_vec().back().file_offset_n_bytes,
              built_index.get_segment_vec().back().file_offset_n_bytes);
}

TEST(WorkloadIndex, AnIndexOfAnotherFileIsNotUsed)
{
    IndexedFile                           file;
    Analysis::APIDumpIndex::WorkloadIndex index;
    bool                                  was_built = false;
    std::string                           error;

    ASSERT_TRUE(Analysis::APIDumpIndex::WorkloadIndex::load_or_build(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                                     0,
                                                                    &index,
                                                                    &was_built,
                                                                    &error) ) << error;

    /* The file is made again with more of it: the index file that is next to it is of the file as it was. */
    std::vector<uint8_t> longer_u8_vec;

    Tests::WorkloadFileBuilder::build_segments(&longer_u8_vec,
                                               N_SEGMENTS + 2,
                                               N_CALLS_PER_SEGMENT,
                                               N_CALLS_PER_FRAME);
    Tests::WorkloadFileBuilder::write_file    (longer_u8_vec);

    EXPECT_FALSE(index.is_of(Tests::WorkloadFileBuilder::get_filename_ptr() ) );

    Analysis::APIDumpIndex::WorkloadIndex new_index;

    ASSERT_TRUE(Analysis::APIDumpIndex::WorkloadIndex::load_or_build(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                                     0,
                                                                    &new_index,
                                                                    &was_built,
                                                                    &error) ) << error;
    EXPECT_TRUE(was_built);
    EXPECT_EQ(new_index.get_segment_vec().size(),
              static_cast<size_t>(N_SEGMENTS + 2) );
}

TEST(WorkloadIndex, ARefusedFileIsNotAnIndex)
{
    Analysis::APIDumpIndex::WorkloadIndex index;
    std::string                           error;

    EXPECT_FALSE(Analysis::APIDumpIndex::WorkloadIndex::load("no_such_index_file.index",
                                                            &index,
                                                            &error) );
    EXPECT_FALSE(error.empty() );
}
