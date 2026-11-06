/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_slicing/head_slicer.h"
#include "analysis/api_dump_slicing/workload_writer.h"
#include "Capture/APIDumpLoader/workload_index.h"
#include "Capture/APIDumpLoader/workload_reader.h"
#include "Capture/APIDumpLoader/api_info.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace
{
    /* Where the slice ends: the segment that has the end of the last frame (or the end of the workload, if that frame is still going on there), and how
     * many of its calls the slice has.
     */
    struct SliceEnd
    {
        size_t   n_calls_in_segment = 0; /* 0: all of them */
        size_t   n_segment          = 0;
    };

    /* False if the workload has no such frame. */
    bool find_slice_end(const Capture::APIDumpLoader::WorkloadIndex& in_index,
                        const uint64_t&                              in_last_frame,
                        SliceEnd*                                    out_end_ptr)
    {
        const auto& segment_vec = in_index.get_segment_vec();
        bool        result      = false;

        *out_end_ptr = SliceEnd();

        if (in_last_frame >= in_index.get_n_frames() )
        {
            goto end;
        }

        for (size_t n_segment = 0;
                    n_segment < segment_vec.size();
                  ++n_segment)
        {
            const auto& entry = segment_vec.at(n_segment);

            /* The end of the frame is the call of a segment that ends the (last - first + 1)th of the frames which end in it. */
            if (in_last_frame < entry.first_frame_index + entry.n_frames_ended)
            {
                out_end_ptr->n_segment = n_segment;

                result = true;

                goto end;
            }
        }

        /* The last frame has not ended: the workload goes on to its end in it. */
        out_end_ptr->n_segment = segment_vec.size() - 1;

        result = true;
end:
        return result;
    }

    /* Leaves the calls up to the one that ends the frame (inclusive) in the segment, and the data chunks which they refer to; returns how many calls are
     * left. The segment starts the frames at in_first_frame_index; a frame which ends nowhere in it (the last of the workload, still going on) keeps
     * all of its calls.
     */
    size_t cut_segment(const uint64_t&                  in_first_frame_index,
                       const uint64_t&                  in_last_frame,
                       Capture::APIDumpLoader::Workload* inout_segment_ptr)
    {
        auto&          call_vec          = *inout_segment_ptr->dumped_api_call_vec_ptr;
        const uint64_t n_frames_to_end   = in_last_frame - in_first_frame_index + 1;
        uint64_t       n_frames_ended    = 0;
        size_t         n_calls_kept      = call_vec.size();
        size_t         n_chunks_kept     = 0;

        for (size_t n_call = 0;
                    n_call < call_vec.size();
                  ++n_call)
        {
            if (Capture::APIDumpLoader::is_frame_boundary(call_vec.at(n_call) ) == false)
            {
                continue;
            }

            n_frames_ended++;

            if (n_frames_ended == n_frames_to_end)
            {
                n_calls_kept = n_call + 1;

                break;
            }
        }

        call_vec.resize(n_calls_kept);

        /* A call which was made later has a chunk which is numbered after the ones of the calls before it. */
        for (const auto& call : call_vec)
        {
            for (const auto* argument_vec_ptr : {&call.arg_in_vec, &call.arg_out_vec})
            {
                for (const auto& argument : *argument_vec_ptr)
                {
                    if (argument.type != APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
                    {
                        continue;
                    }

                    if (argument.get_data_chunk_id() >= inout_segment_ptr->first_data_chunk_id)
                    {
                        n_chunks_kept = std::max<size_t>(n_chunks_kept,
                                                         argument.get_data_chunk_id() - inout_segment_ptr->first_data_chunk_id + 1);
                    }
                }
            }
        }

        if (n_chunks_kept < inout_segment_ptr->data_chunk_ptr_vec.size() )
        {
            inout_segment_ptr->data_chunk_ptr_vec.resize(n_chunks_kept);
        }

        return n_calls_kept;
    }

    /* The calls of the segment which end a frame. */
    uint64_t count_frame_ends(const Capture::APIDumpLoader::Workload& in_segment)
    {
        uint64_t result = 0;

        for (const auto& call : *in_segment.dumped_api_call_vec_ptr)
        {
            if (Capture::APIDumpLoader::is_frame_boundary(call) )
            {
                result++;
            }
        }

        return result;
    }
}

bool Analysis::APIDumpSlicing::HeadSlicer::slice(const std::string&  in_workload_filename,
                                                 const std::string&  in_slice_filename,
                                                 const SliceOptions& in_options,
                                                 SliceReport*        out_report_ptr,
                                                 std::string*        out_error_ptr)
{
    APIInterceptor::WorkloadFormat::WorkloadFileHeader header           = {};
    FILE*                                              input_handle     = nullptr;
    Capture::APIDumpLoader::WorkloadIndex              index;
    bool                                               is_index_built   = false;
    Capture::APIDumpLoader::WorkloadReader::Options    reader_options;
    Capture::APIDumpLoader::WorkloadReaderUniquePtr    reader_ptr;
    bool                                               result           = false;
    SliceEnd                                           slice_end;
    WorkloadWriterUniquePtr                            writer_ptr;
    const bool                                         strips_chunks    = (in_options.strip_chunks_above_n_bytes != 0);

    *out_report_ptr = SliceReport();

    if (Capture::APIDumpLoader::WorkloadIndex::load_or_build(in_workload_filename.c_str(),
                                                             in_options.n_reader_threads,
                                                            &index,
                                                            &is_index_built,
                                                             out_error_ptr) == false)
    {
        goto end;
    }

    if (find_slice_end(index,
                       in_options.last_frame,
                      &slice_end) == false)
    {
        *out_error_ptr = "The workload has " + std::to_string(index.get_n_frames() ) + " frames: there is no frame " + std::to_string(in_options.last_frame) + ".";

        goto end;
    }

    input_handle = ::fopen(in_workload_filename.c_str(),
                           "rb");

    if (input_handle == nullptr)
    {
        *out_error_ptr = "The workload " + in_workload_filename + " cannot be opened.";

        goto end;
    }

    if (::fread(&header,
                sizeof(header),
                1,
                input_handle) != 1                                                       ||
        ::memcmp(header.magic_i8vec8,
                 APIInterceptor::WorkloadFormat::MAGIC,
                 sizeof(header.magic_i8vec8) ) != 0)
    {
        *out_error_ptr = "The file " + in_workload_filename + " is not a workload of a version that can be sliced.";

        goto end;
    }

    writer_ptr = WorkloadWriter::create(in_slice_filename.c_str(),
                                        header,
                                        out_error_ptr);

    if (writer_ptr == nullptr)
    {
        goto end;
    }

    /* A workload whose chunks are stripped is written again from its start; otherwise the segments before the last one are copied as they are, and the
     * reader starts at the last.
     */
    reader_options.n_threads                      = in_options.n_reader_threads;
    reader_options.skip_data_chunks_above_n_bytes = in_options.strip_chunks_above_n_bytes;

    if (strips_chunks == false)
    {
        for (size_t n_segment = 0;
                    n_segment < slice_end.n_segment;
                  ++n_segment)
        {
            const auto&          entry = index.get_segment_vec().at(n_segment);
            std::vector<uint8_t> segment_u8_vec(static_cast<size_t>(entry.n_compressed_bytes));

            if (::_fseeki64(input_handle,
                            static_cast<long long>(entry.file_offset_n_bytes),
                            SEEK_SET) != 0                                                  ||
                ::fread(segment_u8_vec.data(),
                        segment_u8_vec.size(),
                        1,
                        input_handle) != 1                                                   ||
                writer_ptr->write_compressed_segment(segment_u8_vec.data(),
                                                     segment_u8_vec.size() ) == false)
            {
                *out_error_ptr = "Segment " + std::to_string(n_segment) + " cannot be copied.";

                goto end;
            }

            out_report_ptr->n_calls_written   += entry.n_calls;
            out_report_ptr->n_frames_written  += entry.n_frames_ended;
            out_report_ptr->n_segments_copied++;
        }

        reader_options = index.make_reader_options(slice_end.n_segment,
                                                   reader_options);
    }

    reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(in_workload_filename.c_str(),
                                                                reader_options,
                                                                out_error_ptr);

    if (reader_ptr == nullptr)
    {
        goto end;
    }

    for (size_t n_segment = (strips_chunks) ? 0 : slice_end.n_segment;
                n_segment <= slice_end.n_segment;
              ++n_segment)
    {
        Capture::APIDumpLoader::Workload segment;
        const auto&                      entry = index.get_segment_vec().at(n_segment);

        if (reader_ptr->read_next_segment(&segment) == false)
        {
            *out_error_ptr = "Segment " + std::to_string(n_segment) + " cannot be read.";

            goto end;
        }

        if (n_segment == slice_end.n_segment)
        {
            const size_t n_calls_kept = cut_segment(entry.first_frame_index,
                                                    in_options.last_frame,
                                                   &segment);

            out_report_ptr->n_calls_written  += n_calls_kept;
            out_report_ptr->n_frames_written += count_frame_ends(segment);
        }
        else
        {
            out_report_ptr->n_calls_written  += segment.dumped_api_call_vec_ptr->size();
            out_report_ptr->n_frames_written += entry.n_frames_ended;
        }

        if (writer_ptr->write_segment(segment) == false)
        {
            *out_error_ptr = "Segment " + std::to_string(n_segment) + " cannot be written.";

            goto end;
        }

        out_report_ptr->n_segments_rewritten++;
    }

    if (writer_ptr->close() == false)
    {
        *out_error_ptr = "The file " + in_slice_filename + " cannot be written.";

        goto end;
    }

    out_report_ptr->n_bytes_written = writer_ptr->get_n_bytes_written();

    result = true;
end:
    if (input_handle != nullptr)
    {
        ::fclose(input_handle);
    }

    return result;
}
