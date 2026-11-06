/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the writing of a workload and of the slice of its first frames: that what is written is read back as it was, that the segments which are not
 * cut are the bytes of the workload, and that the slice ends with the frame it was asked to.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_slicing/head_slicer.h"
#include "analysis/api_dump_slicing/workload_writer.h"
#include "capture/api_dump_loader/workload_index.h"
#include "capture/api_dump_loader/workload_reader.h"
#include "temp_directory.h"
#include "workload_file_builder.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>

namespace
{
    constexpr uint32_t N_CALLS_PER_FRAME   = 4; /* and a fifth that ends it                                  */
    constexpr uint32_t N_CALLS_PER_SEGMENT = 7; /* a segment does not hold a whole frame                     */
    constexpr uint32_t N_SEGMENTS          = 6;

    /* A workload file of segments, which is removed with this (and its index). */
    class SegmentedFile
    {
    public:
        explicit SegmentedFile(const std::vector<uint8_t>& in_file_u8_vec)
        {
            Tests::WorkloadFileBuilder::write_file(in_file_u8_vec);
        }

        ~SegmentedFile()
        {
            ::remove(Tests::WorkloadFileBuilder::get_filename_ptr() );
            ::remove(Capture::APIDumpLoader::WorkloadIndex::get_default_filename(Tests::WorkloadFileBuilder::get_filename_ptr() ).c_str() );
        }
    };

    /* The segments of the file, each read as the reader gives them. */
    std::vector<Capture::APIDumpLoader::Workload> read_segments(const std::string& in_filename)
    {
        auto                                          reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(in_filename.c_str() );
        std::vector<Capture::APIDumpLoader::Workload> result_vec;

        if (reader_ptr != nullptr)
        {
            Capture::APIDumpLoader::Workload segment;

            while (reader_ptr->read_next_segment(&segment) )
            {
                result_vec.push_back(std::move(segment) );

                segment = Capture::APIDumpLoader::Workload();
            }
        }

        return result_vec;
    }

    std::vector<uint8_t> read_file(const std::string& in_filename)
    {
        std::vector<uint8_t> result_u8_vec;
        FILE* const          file_handle = ::fopen(in_filename.c_str(),
                                                   "rb");

        if (file_handle != nullptr)
        {
            uint8_t buffer_u8vec4096[4096];
            size_t  n_bytes_read = 0;

            while ( (n_bytes_read = ::fread(buffer_u8vec4096,
                                            1,
                                            sizeof(buffer_u8vec4096),
                                            file_handle) ) != 0)
            {
                result_u8_vec.insert(result_u8_vec.end(),
                                     buffer_u8vec4096,
                                     buffer_u8vec4096 + n_bytes_read);
            }

            ::fclose(file_handle);
        }

        return result_u8_vec;
    }

    /* The arguments of a call as the bytes which the file has them as. */
    std::vector<uint8_t> serialize_arguments(const Capture::APIDumpLoader::DumpedAPICall& in_call)
    {
        std::vector<uint8_t> result_u8_vec;

        for (const auto& argument : in_call.arg_in_vec)
        {
            argument.serialize_to_u8_vec(&result_u8_vec);
        }

        for (const auto& argument : in_call.arg_out_vec)
        {
            argument.serialize_to_u8_vec(&result_u8_vec);
        }

        return result_u8_vec;
    }
}

TEST(WorkloadWriter, ASegmentIsReadBackAsItWasWrittenWithItsChunksAndSurfaces)
{
    std::vector<uint8_t> file_u8_vec;

    Tests::WorkloadFileBuilder::build_two_segments(&file_u8_vec);

    SegmentedFile file(file_u8_vec);

    const auto               original_segment_vec = read_segments(Tests::WorkloadFileBuilder::get_filename_ptr() );
    const Tests::TempDirectory directory;
    const std::string        written_filename     = directory.get_path() + "/written.workload";
    std::string              error;

    ASSERT_EQ(original_segment_vec.size(),
              2u);

    {
        APIInterceptor::WorkloadFormat::WorkloadFileHeader header = {};

        ::memcpy(header.magic_i8vec8,
                 APIInterceptor::WorkloadFormat::MAGIC,
                 sizeof(header.magic_i8vec8) );

        header.pointer_n_bytes = Tests::WorkloadFileBuilder::POINTER_N_BYTES;
        header.qpc_frequency   = Tests::WorkloadFileBuilder::QPC_FREQUENCY;
        header.version         = APIInterceptor::WorkloadFormat::FILE_VERSION;

        auto writer_ptr = Analysis::APIDumpSlicing::WorkloadWriter::create(written_filename.c_str(),
                                                                           header,
                                                                          &error);

        ASSERT_NE(writer_ptr,
                  nullptr) << error;

        for (const auto& segment : original_segment_vec)
        {
            ASSERT_TRUE(writer_ptr->write_segment(segment) );
        }

        ASSERT_TRUE(writer_ptr->close() );
    }

    const auto written_segment_vec = read_segments(written_filename);

    ASSERT_EQ(written_segment_vec.size(),
              original_segment_vec.size() );

    for (size_t n_segment = 0;
                n_segment < original_segment_vec.size();
              ++n_segment)
    {
        const auto& original = original_segment_vec.at(n_segment);
        const auto& written  = written_segment_vec.at(n_segment);

        ASSERT_EQ(written.dumped_api_call_vec_ptr->size(),
                  original.dumped_api_call_vec_ptr->size() );

        for (size_t n_call = 0;
                    n_call < original.dumped_api_call_vec_ptr->size();
                  ++n_call)
        {
            const auto& original_call = original.dumped_api_call_vec_ptr->at(n_call);
            const auto& written_call  = written.dumped_api_call_vec_ptr->at(n_call);

            EXPECT_EQ(written_call.func,
                      original_call.func);
            EXPECT_EQ(written_call.thread_id,
                      original_call.thread_id);
            EXPECT_EQ(written_call.timestamp_qpc,
                      original_call.timestamp_qpc);
            EXPECT_EQ(serialize_arguments(written_call),
                      serialize_arguments(original_call) );
            EXPECT_EQ(written_call.returned_value_ptr == nullptr,
                      original_call.returned_value_ptr == nullptr);
        }

        ASSERT_EQ(written.data_chunk_ptr_vec.size(),
                  original.data_chunk_ptr_vec.size() );

        for (size_t n_chunk = 0;
                    n_chunk < original.data_chunk_ptr_vec.size();
                  ++n_chunk)
        {
            EXPECT_EQ(*written.data_chunk_ptr_vec.at(n_chunk),
                      *original.data_chunk_ptr_vec.at(n_chunk) );
        }

        ASSERT_EQ(written.dumped_surface_vec_ptr->size(),
                  original.dumped_surface_vec_ptr->size() );

        for (size_t n_surface = 0;
                    n_surface < original.dumped_surface_vec_ptr->size();
                  ++n_surface)
        {
            EXPECT_EQ(written.dumped_surface_vec_ptr->at(n_surface).extents_u32vec2,
                      original.dumped_surface_vec_ptr->at(n_surface).extents_u32vec2);
            EXPECT_EQ(written.dumped_surface_vec_ptr->at(n_surface).data_u8_vec,
                      original.dumped_surface_vec_ptr->at(n_surface).data_u8_vec);
        }
    }
}

TEST(HeadSlicer, TheSliceEndsWithTheCallThatEndsTheFrameItWasAskedFor)
{
    std::vector<uint8_t> file_u8_vec;

    Tests::WorkloadFileBuilder::build_segments(&file_u8_vec,
                                               N_SEGMENTS,
                                               N_CALLS_PER_SEGMENT,
                                               N_CALLS_PER_FRAME);

    SegmentedFile                           file(file_u8_vec);
    const Tests::TempDirectory              directory;
    std::string                             error;
    Analysis::APIDumpSlicing::SliceOptions  options;
    Analysis::APIDumpSlicing::SliceReport   report;
    const std::string                       slice_filename = directory.get_path() + "/slice.workload";

    /* Frame 2 is calls 10 to 14, and call 14 is in the third segment (calls 14 to 20). */
    options.last_frame = 2;

    ASSERT_TRUE(Analysis::APIDumpSlicing::HeadSlicer::slice(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                            slice_filename,
                                                            options,
                                                           &report,
                                                           &error) ) << error;

    EXPECT_EQ(report.n_calls_written,
              15u);
    EXPECT_EQ(report.n_frames_written,
              3u);
    EXPECT_EQ(report.n_segments_copied,
              2u);
    EXPECT_EQ(report.n_segments_rewritten,
              1u);
    EXPECT_EQ(report.n_bytes_written,
              static_cast<uint64_t>(read_file(slice_filename).size() ) );

    const auto original_segment_vec = read_segments(Tests::WorkloadFileBuilder::get_filename_ptr() );
    const auto slice_segment_vec    = read_segments(slice_filename);

    ASSERT_EQ(slice_segment_vec.size(),
              3u);

    uint64_t n_calls = 0;

    for (size_t n_segment = 0;
                n_segment < slice_segment_vec.size();
              ++n_segment)
    {
        for (const auto& call : *slice_segment_vec.at(n_segment).dumped_api_call_vec_ptr)
        {
            const auto& original_call = original_segment_vec.at(n_segment).dumped_api_call_vec_ptr->at(static_cast<size_t>(n_calls - n_segment * N_CALLS_PER_SEGMENT) );

            EXPECT_EQ(call.timestamp_qpc,
                      original_call.timestamp_qpc);

            n_calls++;
        }
    }

    EXPECT_EQ(n_calls,
              15u);

    const auto& last_call = slice_segment_vec.back().dumped_api_call_vec_ptr->back();

    EXPECT_TRUE(Capture::APIDumpLoader::is_frame_boundary(last_call) );

    /* The frames are numbered as they were, and the workload has no frame after the last. */
    Capture::APIDumpLoader::WorkloadIndex slice_index;

    ASSERT_TRUE(Capture::APIDumpLoader::WorkloadIndex::build(slice_filename.c_str(),
                                                             0,
                                                            &slice_index,
                                                            &error) ) << error;
    EXPECT_EQ(slice_index.get_n_frames(),
              3u);
}

TEST(HeadSlicer, TheSegmentsBeforeTheOneThatIsCutAreTheBytesOfTheWorkload)
{
    std::vector<uint8_t> file_u8_vec;

    Tests::WorkloadFileBuilder::build_segments(&file_u8_vec,
                                               N_SEGMENTS,
                                               N_CALLS_PER_SEGMENT,
                                               N_CALLS_PER_FRAME);

    SegmentedFile                           file(file_u8_vec);
    const Tests::TempDirectory              directory;
    std::string                             error;
    Capture::APIDumpLoader::WorkloadIndex   index;
    Analysis::APIDumpSlicing::SliceOptions  options;
    Analysis::APIDumpSlicing::SliceReport   report;
    const std::string                       slice_filename = directory.get_path() + "/slice.workload";

    options.last_frame = 2;

    ASSERT_TRUE(Analysis::APIDumpSlicing::HeadSlicer::slice(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                            slice_filename,
                                                            options,
                                                           &report,
                                                           &error) ) << error;
    ASSERT_TRUE(Capture::APIDumpLoader::WorkloadIndex::build(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                             0,
                                                            &index,
                                                            &error) ) << error;

    const auto&  second_entry = index.get_segment_vec().at(1);
    const size_t n_bytes      = static_cast<size_t>(second_entry.file_offset_n_bytes + second_entry.n_compressed_bytes);
    const auto   slice_u8_vec = read_file(slice_filename);

    ASSERT_GE(slice_u8_vec.size(),
              n_bytes);
    EXPECT_TRUE(std::equal(slice_u8_vec.begin(),
                           slice_u8_vec.begin() + n_bytes,
                           file_u8_vec.begin() ) );
}

TEST(HeadSlicer, AFrameThatTheWorkloadDoesNotHaveIsRefused)
{
    std::vector<uint8_t> file_u8_vec;

    Tests::WorkloadFileBuilder::build_segments(&file_u8_vec,
                                               N_SEGMENTS,
                                               N_CALLS_PER_SEGMENT,
                                               N_CALLS_PER_FRAME);

    SegmentedFile                           file(file_u8_vec);
    const Tests::TempDirectory              directory;
    std::string                             error;
    Analysis::APIDumpSlicing::SliceOptions  options;
    Analysis::APIDumpSlicing::SliceReport   report;

    options.last_frame = 1000;

    EXPECT_FALSE(Analysis::APIDumpSlicing::HeadSlicer::slice(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                             directory.get_path() + "/slice.workload",
                                                             options,
                                                            &report,
                                                            &error) );
    EXPECT_FALSE(error.empty() );
}

TEST(HeadSlicer, StrippedChunksAreWrittenEmptyWithTheirNumbersKept)
{
    constexpr size_t N_CHUNK_BYTES = 1000;

    std::vector<uint8_t> file_u8_vec;

    Tests::WorkloadFileBuilder::build_many_segments(&file_u8_vec,
                                                    3, /* in_n_segments */
                                                    5, /* in_n_calls_per_segment */
                                                    N_CHUNK_BYTES);

    SegmentedFile                           file(file_u8_vec);
    const Tests::TempDirectory              directory;
    std::string                             error;
    Analysis::APIDumpSlicing::SliceOptions  options;
    Analysis::APIDumpSlicing::SliceReport   report;
    const std::string                       slice_filename = directory.get_path() + "/slice.workload";

    /* The calls have no frame end: the one frame there is goes on to the end of the file, and is the last. */
    options.last_frame                 = 0;
    options.strip_chunks_above_n_bytes = 100;

    ASSERT_TRUE(Analysis::APIDumpSlicing::HeadSlicer::slice(Tests::WorkloadFileBuilder::get_filename_ptr(),
                                                            slice_filename,
                                                            options,
                                                           &report,
                                                           &error) ) << error;

    EXPECT_EQ(report.n_segments_copied,
              0u);
    EXPECT_EQ(report.n_segments_rewritten,
              3u);
    EXPECT_EQ(report.n_calls_written,
              15u);

    const auto slice_segment_vec = read_segments(slice_filename);

    ASSERT_EQ(slice_segment_vec.size(),
              3u);

    /* The first segments keep the numbers of their chunks, which are empty. */
    ASSERT_EQ(slice_segment_vec.at(0).data_chunk_ptr_vec.size(),
              static_cast<size_t>(Tests::WorkloadFileBuilder::N_CHUNKS_PER_SEGMENT) );
    EXPECT_TRUE(slice_segment_vec.at(0).data_chunk_ptr_vec.at(0)->empty() );
    EXPECT_LT(read_file(slice_filename).size(),
              file_u8_vec.size() );
}
