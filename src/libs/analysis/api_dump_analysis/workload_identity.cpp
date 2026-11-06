/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/hasher.h"
#include "analysis/api_dump_analysis/workload_identity.h"
#include <fstream>
#include <vector>

void Analysis::APIDumpAnalysis::WorkloadIdentity::get_piece_offsets(const uint64_t& in_n_bytes,
                                                                    uint64_t*       out_header_offset_ptr,
                                                                    uint64_t*       out_middle_offset_ptr,
                                                                    uint64_t*       out_tail_offset_ptr)
{
    *out_header_offset_ptr = 0;
    *out_middle_offset_ptr = 0;
    *out_tail_offset_ptr   = 0;

    if (in_n_bytes > N_PIECE_BYTES)
    {
        *out_middle_offset_ptr = (in_n_bytes - N_PIECE_BYTES) / 2;
        *out_tail_offset_ptr   =  in_n_bytes - N_PIECE_BYTES;
    }
}

bool Analysis::APIDumpAnalysis::WorkloadIdentity::hash_piece(const std::string& in_filename,
                                                             const uint64_t&    in_offset,
                                                             uint64_t*          out_hash_ptr)
{
    std::ifstream        file(in_filename,
                              std::ios::binary);
    std::vector<uint8_t> piece_u8_vec(N_PIECE_BYTES);
    bool                 result = false;

    if (file.is_open() == false)
    {
        goto end;
    }

    file.seekg(static_cast<std::streamoff>(in_offset) );

    if (file.fail() )
    {
        goto end;
    }

    file.read(reinterpret_cast<char*>(piece_u8_vec.data() ),
              static_cast<std::streamsize>(piece_u8_vec.size() ) );

    /* A file shorter than a piece is hashed as far as it goes; reaching its end is not an error. */
    piece_u8_vec.resize(static_cast<size_t>(file.gcount() ) );

    *out_hash_ptr = Hasher::hash_bytes(piece_u8_vec.data(),
                                       piece_u8_vec.size() );
    result        = true;

end:
    return result;
}

bool Analysis::APIDumpAnalysis::WorkloadIdentity::is_same_workload(const WorkloadReference& in_a,
                                                                   const WorkloadReference& in_b)
{
    /* A hash that is 0 was not recorded, and tells nothing. */
    const auto is_hash_alike_func = [](const uint64_t& in_hash_a,
                                       const uint64_t& in_hash_b)
    {
        return (in_hash_a == 0 ||
                in_hash_b == 0 ||
                in_hash_a == in_hash_b);
    };

    return (in_a.n_bytes == in_b.n_bytes                                     &&
            is_hash_alike_func(in_a.header_hash,
                               in_b.header_hash)                             &&
            is_hash_alike_func(in_a.middle_hash,
                               in_b.middle_hash)                             &&
            is_hash_alike_func(in_a.tail_hash,
                               in_b.tail_hash) );
}

bool Analysis::APIDumpAnalysis::WorkloadIdentity::read(const std::string& in_filename,
                                                       WorkloadReference* out_reference_ptr,
                                                       std::string*       out_error_ptr)
{
    std::ifstream     file(in_filename,
                           std::ios::binary | std::ios::ate);
    uint64_t          header_offset = 0;
    uint64_t          middle_offset = 0;
    WorkloadReference reference;
    bool              result        = false;
    uint64_t          tail_offset   = 0;

    reference.filename = in_filename;

    if (file.is_open() == false)
    {
        *out_error_ptr = "The workload " + in_filename + " cannot be opened.";

        goto end;
    }

    reference.n_bytes = static_cast<uint64_t>(file.tellg() );

    file.close();

    get_piece_offsets(reference.n_bytes,
                     &header_offset,
                     &middle_offset,
                     &tail_offset);

    if (hash_piece(in_filename,
                   header_offset,
                  &reference.header_hash) == false)
    {
        *out_error_ptr = "The start of the workload " + in_filename + " cannot be read.";

        goto end;
    }

    if (hash_piece(in_filename,
                   middle_offset,
                  &reference.middle_hash) == false)
    {
        *out_error_ptr = "The middle of the workload " + in_filename + " cannot be read.";

        goto end;
    }

    if (hash_piece(in_filename,
                   tail_offset,
                  &reference.tail_hash) == false)
    {
        *out_error_ptr = "The end of the workload " + in_filename + " cannot be read.";

        goto end;
    }

    *out_reference_ptr = reference;
    result             = true;

end:
    return result;
}

Analysis::APIDumpAnalysis::WorkloadMatch Analysis::APIDumpAnalysis::WorkloadIdentity::verify(const std::string&       in_filename,
                                                                                             const WorkloadReference& in_reference,
                                                                                             std::string*             out_reason_ptr)
{
    std::string       error;
    WorkloadReference file_reference;
    WorkloadMatch     result = WorkloadMatch::UNREADABLE;

    if (read(in_filename,
            &file_reference,
            &error) == false)
    {
        *out_reason_ptr = error;

        goto end;
    }

    result = WorkloadMatch::MISMATCH;

    if (file_reference.n_bytes != in_reference.n_bytes)
    {
        *out_reason_ptr = "The workload is " + std::to_string(file_reference.n_bytes) + " bytes, the analysis is of one of " +
                          std::to_string(in_reference.n_bytes) + ".";

        goto end;
    }

    if (in_reference.header_hash != 0                      &&
        in_reference.header_hash != file_reference.header_hash)
    {
        *out_reason_ptr = "The start of the workload is not what the analysis is of.";

        goto end;
    }

    if (in_reference.middle_hash != 0                      &&
        in_reference.middle_hash != file_reference.middle_hash)
    {
        *out_reason_ptr = "The middle of the workload is not what the analysis is of.";

        goto end;
    }

    if (in_reference.tail_hash != 0                      &&
        in_reference.tail_hash != file_reference.tail_hash)
    {
        *out_reason_ptr = "The end of the workload is not what the analysis is of.";

        goto end;
    }

    result = WorkloadMatch::MATCHES;

end:
    return result;
}
