/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/call_describer_set.h"
#include "analysis/api_dump_index/state_coalescer.h"
#include "analysis/api_dump_index/state_effect_describer_set.h"
#include "analysis/api_dump_index/state_segment_builder.h"
#include "analysis/api_dump_index/workload_index.h"
#include "capture/api_dump_loader/api_info.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>

namespace
{
    /* What an index file begins with. */
    constexpr char     INDEX_MAGIC[8] = {'A', 'P', 'I', 'D', 'W', 'I', 'D', 'X'};
    constexpr uint32_t INDEX_VERSION  = 3;

    struct IndexFileHeader
    {
        char     magic_i8vec8[8];
        uint32_t version;
        uint32_t is_truncated;
        uint64_t head_hash;
        uint64_t middle_hash;
        uint64_t n_bytes;
        uint64_t tail_hash;
        uint64_t n_calls;
        uint64_t n_frames;
        uint64_t n_segments;
    };

    /* The file is of the byte order of the machine that made it, which is the one that reads it: an index is for a file that is next to it. */
    struct IndexFileEntry
    {
        uint64_t file_offset_n_bytes;
        uint64_t first_call_index;
        uint64_t first_frame_index;
        uint64_t n_compressed_bytes;
        uint32_t first_data_chunk_id;
        uint32_t n_calls;
        uint32_t n_data_chunks;
        uint32_t n_frames_ended;
        uint64_t state_n_bytes;
        uint64_t state_offset_n_bytes;
    };

    static_assert(sizeof(IndexFileEntry) == 64,
                  "Unexpected padding in an entry of the index file");

    /* The hash of what is of the file from the offset on, up to 4 KiB of it. */
    uint64_t hash_bytes_at(FILE*          in_file_handle,
                           const uint64_t& in_offset)
    {
        constexpr size_t   N_BYTES_HASHED  = 4096;
        constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ull;
        constexpr uint64_t FNV_PRIME        = 1099511628211ull;

        uint8_t  byte_u8vec4096[N_BYTES_HASHED];
        uint64_t result = FNV_OFFSET_BASIS;
        size_t   n_bytes = 0;

        if (::_fseeki64(in_file_handle,
                        static_cast<__int64>(in_offset),
                        SEEK_SET) == 0)
        {
            n_bytes = ::fread(byte_u8vec4096,
                              1,
                              N_BYTES_HASHED,
                              in_file_handle);
        }

        for (size_t n_byte = 0;
                    n_byte < n_bytes;
                  ++n_byte)
        {
            result = (result ^ byte_u8vec4096[n_byte]) * FNV_PRIME;
        }

        return result;
    }
}

Analysis::APIDumpIndex::WorkloadIndex::WorkloadIndex()
    :m_is_truncated(false),
     m_n_calls     (0),
     m_n_frames    (0)
{
    /* Stub */
}

bool Analysis::APIDumpIndex::WorkloadIndex::build(const char*     in_workload_filename_ptr,
                                                  const uint32_t& in_n_threads,
                                                  WorkloadIndex*  out_index_ptr,
                                                  std::string*    out_error_ptr)
{
    /* The state frames need the chunks of the calls they keep, so the segments are read in full (without their images). */
    constexpr uint32_t MAX_N_SEGMENTS_IN_FLIGHT = (sizeof(void*) == sizeof(uint32_t) ) ? 2 : 5; /* a 32-bit process has 2 GB */

    CallDescriberSet          call_describer_set;
    StateEffectDescriberSet   effect_describer_set;
    WorkloadIndex             index;
    bool                      is_in_open_frame = false; /* a call was made after the last one that ended a frame */
    Capture::APIDumpLoader::WorkloadReader::Options   options;
    Capture::APIDumpLoader::WorkloadReaderUniquePtr   reader_ptr;
    bool                      result = false;
    Capture::APIDumpLoader::Workload                  segment;

    /* One walk over all the calls of the workload, so that what a describer follows from call to call (a display list) goes across segments. */
    const auto describe_func = [&effect_describer_set](const Capture::APIDumpLoader::DumpedAPICall& in_call)
    {
        return effect_describer_set.describe(in_call);
    };

    StateCoalescer coalescer(describe_func);

    options.n_segments_in_flight = (in_n_threads != 0) ? (std::min)(in_n_threads + 1, MAX_N_SEGMENTS_IN_FLIGHT)
                                                       : 0;
    options.n_threads            = in_n_threads;
    options.skip_surfaces        = true;

    if (get_identity(in_workload_filename_ptr,
                    &index.m_identity) == false)
    {
        *out_error_ptr = "The file cannot be read.";

        goto end;
    }

    reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(in_workload_filename_ptr,
                                        options,
                                        out_error_ptr);

    if (reader_ptr == nullptr)
    {
        goto end;
    }

    while (reader_ptr->read_next_segment(&segment) )
    {
        SegmentIndexEntry entry;

        entry.file_offset_n_bytes = segment.file_offset_n_bytes;
        entry.first_call_index    = index.m_n_calls;
        entry.first_data_chunk_id = segment.first_data_chunk_id;
        entry.first_frame_index   = index.m_n_frames;
        entry.n_calls             = static_cast<uint32_t>(segment.dumped_api_call_vec_ptr->size() );
        entry.n_compressed_bytes  = segment.n_compressed_bytes;
        entry.n_data_chunks       = static_cast<uint32_t>(segment.data_chunk_ptr_vec.size() );

        std::vector<bool> is_kept_vec;

        is_kept_vec.reserve(entry.n_calls);

        for (const auto& call : *segment.dumped_api_call_vec_ptr)
        {
            const bool is_boundary = Capture::APIDumpLoader::is_frame_boundary(call);
            const auto call_class  = call_describer_set.describe(call);

            is_kept_vec.push_back(call_class != CallClass::CONTENT ||
                                  is_boundary);

            if (is_boundary)
            {
                entry.n_frames_ended++;

                is_in_open_frame = false;
            }
            else
            {
                is_in_open_frame = true;
            }
        }

        {
            const std::vector<size_t> call_index_vec = coalescer.coalesce(*segment.dumped_api_call_vec_ptr,
                                                                          is_kept_vec);
            std::vector<uint8_t>      state_frame_byte_vec;

            if (StateSegmentBuilder::build(segment,
                                           call_index_vec,
                                          &state_frame_byte_vec) )
            {
                entry.state_n_bytes        = state_frame_byte_vec.size();
                entry.state_offset_n_bytes = index.m_state_byte_vec.size();

                index.m_state_byte_vec.insert(index.m_state_byte_vec.end(),
                                              state_frame_byte_vec.begin(),
                                              state_frame_byte_vec.end() );
            }
        }

        index.m_n_calls  += entry.n_calls;
        index.m_n_frames += entry.n_frames_ended;

        index.m_segment_vec.push_back(entry);
    }

    index.m_is_truncated = reader_ptr->is_truncated();

    /* The last segment of a cut file is not whole: it is read from the file. */
    if (index.m_is_truncated &&
        index.m_segment_vec.empty() == false)
    {
        index.m_segment_vec.back().state_n_bytes = 0;
    }

    /* The calls after the last end of a frame are a frame too. */
    if (is_in_open_frame)
    {
        index.m_n_frames += 1;
    }

    *out_index_ptr = std::move(index);
    result         = true;

end:
    return result;
}

bool Analysis::APIDumpIndex::WorkloadIndex::find_segment_of_frame(const uint64_t& in_frame_index,
                                                                  size_t*         out_n_segment_ptr) const
{
    bool result = false;

    if (in_frame_index < m_n_frames)
    {
        /* The first segment that has a call of the frame: the frames it holds, to the last one that it starts, are not before it. */
        for (size_t n_segment = 0;
                    n_segment < m_segment_vec.size();
                  ++n_segment)
        {
            const auto& entry = m_segment_vec.at(n_segment);

            if (entry.first_frame_index + entry.n_frames_ended >= in_frame_index)
            {
                *out_n_segment_ptr = n_segment;
                result             = true;

                break;
            }
        }
    }

    return result;
}

std::string Analysis::APIDumpIndex::WorkloadIndex::get_default_filename(const std::string& in_workload_filename)
{
    return in_workload_filename + ".index";
}

bool Analysis::APIDumpIndex::WorkloadIndex::get_identity(const char* in_workload_filename_ptr,
                                                         Identity*   out_identity_ptr)
{
    FILE* const file_handle = ::fopen(in_workload_filename_ptr,
                                      "rb");
    bool        result      = false;

    if (file_handle == nullptr)
    {
        goto end;
    }

    if (::_fseeki64(file_handle,
                    0,
                    SEEK_END) == 0)
    {
        constexpr uint64_t N_BYTES_HASHED = 4096;

        const uint64_t n_bytes = static_cast<uint64_t>(::_ftelli64(file_handle) );

        out_identity_ptr->head_hash   = hash_bytes_at(file_handle,
                                                      0);
        out_identity_ptr->middle_hash = hash_bytes_at(file_handle,
                                                      n_bytes / 2);
        out_identity_ptr->n_bytes     = n_bytes;
        out_identity_ptr->tail_hash   = hash_bytes_at(file_handle,
                                                      (n_bytes > N_BYTES_HASHED) ? n_bytes - N_BYTES_HASHED
                                                                                 : 0);

        result = true;
    }

    ::fclose(file_handle);

end:
    return result;
}

uint64_t Analysis::APIDumpIndex::WorkloadIndex::get_n_calls() const
{
    return m_n_calls;
}

uint64_t Analysis::APIDumpIndex::WorkloadIndex::get_n_frames() const
{
    return m_n_frames;
}

const std::vector<Analysis::APIDumpIndex::SegmentIndexEntry>& Analysis::APIDumpIndex::WorkloadIndex::get_segment_vec() const
{
    return m_segment_vec;
}

bool Analysis::APIDumpIndex::WorkloadIndex::is_of(const char* in_workload_filename_ptr) const
{
    Identity identity;

    const bool has_identity = get_identity(in_workload_filename_ptr,
                                          &identity);

    return (has_identity                                   &&
            identity.head_hash   == m_identity.head_hash   &&
            identity.middle_hash == m_identity.middle_hash &&
            identity.n_bytes     == m_identity.n_bytes     &&
            identity.tail_hash   == m_identity.tail_hash);
}

bool Analysis::APIDumpIndex::WorkloadIndex::is_truncated() const
{
    return m_is_truncated;
}

bool Analysis::APIDumpIndex::WorkloadIndex::load(const char*    in_index_filename_ptr,
                                                 WorkloadIndex* out_index_ptr,
                                                 std::string*   out_error_ptr)
{
    FILE* const     file_handle     = ::fopen(in_index_filename_ptr,
                                              "rb");
    IndexFileHeader header          = {};
    WorkloadIndex   index;
    bool            is_header_valid = false;
    bool            result          = false;

    if (file_handle == nullptr)
    {
        *out_error_ptr = "The index file cannot be opened.";

        goto end;
    }

    {
        const size_t n_headers_read = ::fread(&header,
                                              sizeof(header),
                                              1,
                                              file_handle);

        if (n_headers_read == 1)
        {
            const int magic_difference = ::memcmp(header.magic_i8vec8,
                                                  INDEX_MAGIC,
                                                  sizeof(header.magic_i8vec8) );

            is_header_valid = (magic_difference == 0 &&
                               header.version   == INDEX_VERSION);
        }
    }

    if (is_header_valid == false)
    {
        *out_error_ptr = "This is not an index of this version.";

        goto close;
    }

    index.m_identity.head_hash   = header.head_hash;
    index.m_identity.middle_hash = header.middle_hash;
    index.m_identity.n_bytes     = header.n_bytes;
    index.m_identity.tail_hash   = header.tail_hash;
    index.m_is_truncated         = (header.is_truncated != 0);
    index.m_n_calls              = header.n_calls;
    index.m_n_frames             = header.n_frames;

    for (uint64_t n_segment = 0;
                  n_segment < header.n_segments;
                ++n_segment)
    {
        IndexFileEntry    file_entry = {};
        SegmentIndexEntry entry;

        if (::fread(&file_entry,
                    sizeof(file_entry),
                    1,
                    file_handle) != 1)
        {
            *out_error_ptr = "The index file is cut short.";

            goto close;
        }

        entry.file_offset_n_bytes = file_entry.file_offset_n_bytes;
        entry.first_call_index    = file_entry.first_call_index;
        entry.first_data_chunk_id = file_entry.first_data_chunk_id;
        entry.first_frame_index   = file_entry.first_frame_index;
        entry.n_calls             = file_entry.n_calls;
        entry.n_compressed_bytes  = file_entry.n_compressed_bytes;
        entry.n_data_chunks       = file_entry.n_data_chunks;
        entry.n_frames_ended      = file_entry.n_frames_ended;
        entry.state_n_bytes        = file_entry.state_n_bytes;
        entry.state_offset_n_bytes = file_entry.state_offset_n_bytes;

        index.m_segment_vec.push_back(entry);
    }

    *out_index_ptr = std::move(index);
    result         = true;

close:
    ::fclose(file_handle);

end:
    return result;
}

bool Analysis::APIDumpIndex::WorkloadIndex::load_or_build(const char*     in_workload_filename_ptr,
                                                          const uint32_t& in_n_threads,
                                                          WorkloadIndex*  out_index_ptr,
                                                          bool*           out_was_built_ptr,
                                                          std::string*    out_error_ptr)
{
    const std::string index_filename = get_default_filename(in_workload_filename_ptr);
    std::string       ignored_error;
    bool              result         = false;

    *out_was_built_ptr = false;

    const bool is_loaded = load(index_filename.c_str(),
                                out_index_ptr,
                               &ignored_error);

    bool is_of_workload = false;

    if (is_loaded)
    {
        is_of_workload = out_index_ptr->is_of(in_workload_filename_ptr);
    }

    if (is_of_workload)
    {
        result = true;

        goto end;
    }

    if (build(in_workload_filename_ptr,
              in_n_threads,
              out_index_ptr,
              out_error_ptr) == false)
    {
        goto end;
    }

    *out_was_built_ptr = true;
    result             = true;

    /* A folder which cannot be written to is no reason to go without the index. */
    out_index_ptr->save(index_filename.c_str(),
                        &ignored_error);

end:
    return result;
}

Capture::APIDumpLoader::WorkloadReader::Options Analysis::APIDumpIndex::WorkloadIndex::make_reader_options(const size_t&                   in_n_segment,
                                                                                                           const Capture::APIDumpLoader::WorkloadReader::Options& in_options) const
{
    Capture::APIDumpLoader::WorkloadReader::Options result = in_options;

    if (in_n_segment < m_segment_vec.size() )
    {
        result.n_data_chunks_before  = m_segment_vec.at(in_n_segment).first_data_chunk_id;
        result.start_offset_n_bytes  = m_segment_vec.at(in_n_segment).file_offset_n_bytes;
    }

    return result;
}

std::vector<Capture::APIDumpLoader::WorkloadReader::SegmentStart> Analysis::APIDumpIndex::WorkloadIndex::make_segment_start_vec() const
{
    const uint64_t                            state_area_offset = sizeof(IndexFileHeader) + m_segment_vec.size() * sizeof(IndexFileEntry);
    std::vector<Capture::APIDumpLoader::WorkloadReader::SegmentStart> result;

    for (const auto& entry : m_segment_vec)
    {
        Capture::APIDumpLoader::WorkloadReader::SegmentStart start;

        start.file_offset_n_bytes  = entry.file_offset_n_bytes;
        start.first_data_chunk_id  = entry.first_data_chunk_id;
        start.first_frame_index    = entry.first_frame_index;
        start.n_compressed_bytes   = entry.n_compressed_bytes;
        start.n_frames_ended       = entry.n_frames_ended;
        start.state_n_bytes        = entry.state_n_bytes;
        start.state_offset_n_bytes = state_area_offset + entry.state_offset_n_bytes;

        result.push_back(start);
    }

    return result;
}

bool Analysis::APIDumpIndex::WorkloadIndex::save(const char*  in_index_filename_ptr,
                                                 std::string* out_error_ptr) const
{
    FILE* const     file_handle = ::fopen(in_index_filename_ptr,
                                          "wb");
    IndexFileHeader header      = {};
    bool            result      = false;

    if (file_handle == nullptr)
    {
        *out_error_ptr = "The index file cannot be written.";

        goto end;
    }

    ::memcpy(header.magic_i8vec8,
             INDEX_MAGIC,
             sizeof(header.magic_i8vec8) );

    header.head_hash    = m_identity.head_hash;
    header.is_truncated = (m_is_truncated) ? 1 : 0;
    header.middle_hash  = m_identity.middle_hash;
    header.n_bytes      = m_identity.n_bytes;
    header.n_calls      = m_n_calls;
    header.n_frames     = m_n_frames;
    header.n_segments   = m_segment_vec.size();
    header.tail_hash    = m_identity.tail_hash;
    header.version      = INDEX_VERSION;

    result = (::fwrite(&header,
                       sizeof(header),
                       1,
                       file_handle) == 1);

    for (const auto& entry : m_segment_vec)
    {
        IndexFileEntry file_entry = {};

        file_entry.file_offset_n_bytes = entry.file_offset_n_bytes;
        file_entry.first_call_index    = entry.first_call_index;
        file_entry.first_data_chunk_id = entry.first_data_chunk_id;
        file_entry.first_frame_index   = entry.first_frame_index;
        file_entry.n_calls             = entry.n_calls;
        file_entry.n_compressed_bytes  = entry.n_compressed_bytes;
        file_entry.n_data_chunks       = entry.n_data_chunks;
        file_entry.n_frames_ended      = entry.n_frames_ended;
        file_entry.state_n_bytes        = entry.state_n_bytes;
        file_entry.state_offset_n_bytes = entry.state_offset_n_bytes;

        if (result)
        {
            const size_t n_entries_written = ::fwrite(&file_entry,
                                                      sizeof(file_entry),
                                                      1,
                                                      file_handle);

            result = (n_entries_written == 1);
        }
    }

    if (result &&
        m_state_byte_vec.empty() == false)
    {
        result = (::fwrite(m_state_byte_vec.data(),
                           1,
                           m_state_byte_vec.size(),
                           file_handle) == m_state_byte_vec.size() );
    }

    {
        const int close_result = ::fclose(file_handle);

        result = (close_result == 0 &&
                  result);
    }

    if (result == false)
    {
        *out_error_ptr = "The index file cannot be written.";
    }

end:
    return result;
}
