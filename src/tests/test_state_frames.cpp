/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the state frames of the index of a workload: what they hold, and that a reader takes a segment from one when only its state is wanted.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_index/workload_index.h"
#include "capture/api_dump_loader/workload_reader.h"
#include "workload_file_builder.h"
#include <stdio.h>

namespace
{
    constexpr uint32_t N_MIXED_SEGMENTS = 4;

    /* A file of mixed segments with its index saved next to it, both removed with this. */
    class MixedFile
    {
    public:
        MixedFile()
        {
            std::vector<uint8_t> file_u8_vec;
            std::string          error;

            Tests::WorkloadFileBuilder::build_mixed_segments(&file_u8_vec,
                                                             N_MIXED_SEGMENTS);
            Tests::WorkloadFileBuilder::write_file          (file_u8_vec);

            EXPECT_TRUE(Analysis::APIDumpIndex::WorkloadIndex::build(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                                     0,
                                                                    &index,
                                                                    &error) ) << error;
            EXPECT_TRUE(index.save(get_index_filename().c_str(),
                                  &error) ) << error;
        }

        ~MixedFile()
        {
            ::remove(Tests::WorkloadFileBuilder::get_filename_ptr() );
            ::remove(get_index_filename().c_str() );
        }

        static std::string get_index_filename()
        {
            return Analysis::APIDumpIndex::WorkloadIndex::get_default_filename(Tests::WorkloadFileBuilder::get_filename_ptr() );
        }

        /* Reads the segments; those that the function wants as state only come from their state frames. */
        std::vector<Capture::APIDumpLoader::Workload> read(const Capture::APIDumpLoader::StateOnlyFunc& in_state_only_func,
                                                            const uint32_t&                              in_n_threads) const
        {
            Capture::APIDumpLoader::WorkloadReader::Options options;
            std::vector<Capture::APIDumpLoader::Workload>   result_vec;

            options.n_threads         = in_n_threads;
            options.segment_start_vec = index.make_segment_start_vec();
            options.state_filename    = get_index_filename();
            options.state_only_func   = in_state_only_func;

            auto reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                                             options);

            EXPECT_NE(reader_ptr,
                      nullptr);

            Capture::APIDumpLoader::Workload segment;

            while (reader_ptr != nullptr &&
                   reader_ptr->read_next_segment(&segment) )
            {
                result_vec.push_back(std::move(segment) );
            }

            return result_vec;
        }

        Analysis::APIDumpIndex::WorkloadIndex index;
    };
}

TEST(StateFrames, EverySegmentHasAStateFrame)
{
    MixedFile file;

    ASSERT_EQ(file.index.get_segment_vec().size(),
              static_cast<size_t>(N_MIXED_SEGMENTS) );

    for (const auto& entry : file.index.get_segment_vec() )
    {
        EXPECT_NE(entry.state_n_bytes,
                  0u);
    }
}

TEST(StateFrames, ASegmentWantedAsStateOnlyComesFromItsStateFrameWithTheContentLeftOut)
{
    MixedFile  file;
    const auto all_state_only_func = [](const uint64_t&,
                                        const uint64_t&)
    {
        return true;
    };

    for (const uint32_t n_threads : {0u, 2u})
    {
        const auto segment_vec = file.read(all_state_only_func,
                                           n_threads);

        ASSERT_EQ(segment_vec.size(),
                  static_cast<size_t>(N_MIXED_SEGMENTS) );

        for (size_t n_segment = 0;
                    n_segment < segment_vec.size();
                  ++n_segment)
        {
            const auto& segment = segment_vec.at(n_segment);
            const auto& entry   = file.index.get_segment_vec().at(n_segment);

            /* The glAccum, the Direct3D 7 call and the end of the frame stay; the vertex and the Direct3D 8 draw go. */
            ASSERT_EQ(segment.dumped_api_call_vec_ptr->size(),
                      3u);
            EXPECT_EQ(segment.dumped_api_call_vec_ptr->at(0).func,
                      APIInterceptor::APIFUNCTION_GL_GLACCUM);
            EXPECT_EQ(segment.dumped_api_call_vec_ptr->at(1).func,
                      APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE);
            EXPECT_EQ(segment.dumped_api_call_vec_ptr->at(2).func,
                      APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS);

            /* The numbers of the workload file are kept. */
            EXPECT_EQ(segment.file_offset_n_bytes,
                      entry.file_offset_n_bytes);
            EXPECT_EQ(segment.n_compressed_bytes,
                      entry.n_compressed_bytes);
            EXPECT_EQ(segment.first_data_chunk_id,
                      entry.first_data_chunk_id);

            /* The chunk of the glAccum is there; the one of the vertex is an empty place. */
            ASSERT_EQ(segment.data_chunk_ptr_vec.size(),
                      2u);
            EXPECT_EQ(segment.data_chunk_ptr_vec.at(0)->size(),
                      4u);
            EXPECT_TRUE(segment.data_chunk_ptr_vec.at(1)->empty() );
        }
    }
}

TEST(StateFrames, OnlyTheSegmentsWhoseFramesAreWantedAsStateOnlyComeFromTheirStateFrames)
{
    MixedFile  file;
    const auto first_and_last_func = [](const uint64_t& in_first_frame_index,
                                        const uint64_t&)
    {
        return (in_first_frame_index == 0 ||
                in_first_frame_index == N_MIXED_SEGMENTS - 1);
    };

    const auto segment_vec = file.read(first_and_last_func,
                                       2);

    ASSERT_EQ(segment_vec.size(),
              static_cast<size_t>(N_MIXED_SEGMENTS) );

    for (size_t n_segment = 0;
                n_segment < segment_vec.size();
              ++n_segment)
    {
        const bool is_state_only = (n_segment == 0 ||
                                    n_segment == N_MIXED_SEGMENTS - 1);

        EXPECT_EQ(segment_vec.at(n_segment).dumped_api_call_vec_ptr->size(),
                  (is_state_only) ? 3u
                                  : 5u);
    }
}

TEST(StateFrames, WithNoStateOnlyFunctionEverySegmentComesFromTheFile)
{
    MixedFile file;

    const auto segment_vec = file.read(Capture::APIDumpLoader::StateOnlyFunc(),
                                       0);

    ASSERT_EQ(segment_vec.size(),
              static_cast<size_t>(N_MIXED_SEGMENTS) );

    for (const auto& segment : segment_vec)
    {
        EXPECT_EQ(segment.dumped_api_call_vec_ptr->size(),
                  5u);
        EXPECT_EQ(segment.data_chunk_ptr_vec.at(1)->size(),
                  4u);
    }
}

TEST(StateFrames, AnIndexOfAnOlderVersionIsNotUsed)
{
    MixedFile                             file;
    Analysis::APIDumpIndex::WorkloadIndex index;
    std::string                           error;

    /* The version is the number after the 8 bytes of the magic. */
    FILE* const file_handle = ::fopen(MixedFile::get_index_filename().c_str(),
                                      "r+b");

    ASSERT_NE(file_handle,
              nullptr);

    const uint32_t old_version = 1;

    ::fseek (file_handle,
             8,
             SEEK_SET);
    ::fwrite(&old_version,
             sizeof(old_version),
             1,
             file_handle);
    ::fclose(file_handle);

    EXPECT_FALSE(Analysis::APIDumpIndex::WorkloadIndex::load(MixedFile::get_index_filename().c_str(),
                                                            &index,
                                                            &error) );
}
