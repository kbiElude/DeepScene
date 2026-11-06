/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_slicing/workload_writer.h"
#include "capture/api_dumper_dll/zstd_file_writer.h"
#include <string.h>

namespace
{
    constexpr int      COMPRESSION_LEVEL        = 3; /* the dumper's: it keeps up with a running program */
    constexpr size_t   N_SEGMENT_COUNTS         = 3; /* API calls, data chunks, surfaces                 */
    constexpr size_t   N_SURFACE_HEADER_FIELDS  = 3; /* width, height, n bytes                           */

    /* Appends the 32-bit values. */
    void append_u32_values(std::vector<uint8_t>*       inout_u8_vec_ptr,
                           const uint32_t*             in_value_ptr,
                           const size_t&               in_n_values)
    {
        const size_t n_bytes_before = inout_u8_vec_ptr->size();

        inout_u8_vec_ptr->resize(n_bytes_before + in_n_values * sizeof(uint32_t) );

        ::memcpy(inout_u8_vec_ptr->data() + n_bytes_before,
                 in_value_ptr,
                 in_n_values * sizeof(uint32_t) );
    }

    /* Appends the call as the dumper writes it: the header, the arguments, the results, and the returned value (which a call that has none has as an
     * argument of no type).
     */
    void serialize_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                        std::vector<uint8_t>*                        inout_u8_vec_ptr)
    {
        const size_t                       n_bytes_before = inout_u8_vec_ptr->size();
        uint8_t*                           header_u8_ptr  = nullptr;
        APIInterceptor::APIFunctionArgument no_value;

        no_value.type = APIInterceptor::APIFunctionArgumentType::UNKNOWN;

        inout_u8_vec_ptr->resize(n_bytes_before + APIInterceptor::WorkloadFormat::CALL_HEADER_N_BYTES);

        header_u8_ptr = inout_u8_vec_ptr->data() + n_bytes_before;

        {
            const uint32_t n_arguments_in  = static_cast<uint32_t>(in_call.arg_in_vec.size() );
            const uint32_t n_arguments_out = static_cast<uint32_t>(in_call.arg_out_vec.size() );

            ::memcpy(header_u8_ptr,
                     &in_call.func,
                     sizeof(APIInterceptor::APIFunction) );
            header_u8_ptr += sizeof(APIInterceptor::APIFunction);

            ::memcpy(header_u8_ptr,
                     &n_arguments_in,
                     sizeof(uint32_t) );
            header_u8_ptr += sizeof(uint32_t);

            ::memcpy(header_u8_ptr,
                     &n_arguments_out,
                     sizeof(uint32_t) );
            header_u8_ptr += sizeof(uint32_t);

            ::memcpy(header_u8_ptr,
                     &in_call.thread_id,
                     sizeof(uint32_t) );
            header_u8_ptr += sizeof(uint32_t);

            ::memcpy(header_u8_ptr,
                     &in_call.timestamp_qpc,
                     sizeof(uint64_t) );
        }

        for (const auto& argument : in_call.arg_in_vec)
        {
            argument.serialize_to_u8_vec(inout_u8_vec_ptr);
        }

        for (const auto& argument : in_call.arg_out_vec)
        {
            argument.serialize_to_u8_vec(inout_u8_vec_ptr);
        }

        if (in_call.returned_value_ptr != nullptr)
        {
            in_call.returned_value_ptr->serialize_to_u8_vec(inout_u8_vec_ptr);
        }
        else
        {
            no_value.serialize_to_u8_vec(inout_u8_vec_ptr);
        }
    }
}

Analysis::APIDumpSlicing::WorkloadWriter::WorkloadWriter()
    :m_file_handle    (nullptr),
     m_n_bytes_written(0)
{
    /* Stub */
}

Analysis::APIDumpSlicing::WorkloadWriter::~WorkloadWriter()
{
    close();
}

bool Analysis::APIDumpSlicing::WorkloadWriter::close()
{
    bool result = true;

    if (m_file_handle != nullptr)
    {
        result         = (::fclose(m_file_handle) == 0);
        m_file_handle  = nullptr;
    }

    return result;
}

Analysis::APIDumpSlicing::WorkloadWriterUniquePtr Analysis::APIDumpSlicing::WorkloadWriter::create(const char*                                               in_filename_ptr,
                                                                                                   const APIInterceptor::WorkloadFormat::WorkloadFileHeader& in_header,
                                                                                                   std::string*                                              out_error_ptr)
{
    WorkloadWriterUniquePtr result_ptr(new WorkloadWriter());

    result_ptr->m_file_handle = ::fopen(in_filename_ptr,
                                        "wb");

    if (result_ptr->m_file_handle == nullptr)
    {
        *out_error_ptr = std::string("The file ") + in_filename_ptr + " cannot be made.";

        result_ptr.reset();

        goto end;
    }

    if (::fwrite(&in_header,
                 sizeof(in_header),
                 1,
                 result_ptr->m_file_handle) != 1)
    {
        *out_error_ptr = std::string("The file ") + in_filename_ptr + " cannot be written.";

        result_ptr.reset();

        goto end;
    }

    result_ptr->m_n_bytes_written = sizeof(in_header);

end:
    return result_ptr;
}

uint64_t Analysis::APIDumpSlicing::WorkloadWriter::get_n_bytes_written() const
{
    return m_n_bytes_written;
}

bool Analysis::APIDumpSlicing::WorkloadWriter::write_compressed_segment(const uint8_t* in_data_ptr,
                                                                        const size_t&  in_n_bytes)
{
    const bool result = (m_file_handle != nullptr &&
                         ::fwrite(in_data_ptr,
                                  in_n_bytes,
                                  1,
                                  m_file_handle) == 1);

    if (result)
    {
        m_n_bytes_written += in_n_bytes;
    }

    return result;
}

bool Analysis::APIDumpSlicing::WorkloadWriter::write_segment(const Capture::APIDumpLoader::Workload& in_segment)
{
    const uint32_t                                          counts_u32vec3[N_SEGMENT_COUNTS] =
    {
        static_cast<uint32_t>(in_segment.dumped_api_call_vec_ptr->size() ),
        static_cast<uint32_t>(in_segment.data_chunk_ptr_vec.size() ),
        static_cast<uint32_t>(in_segment.dumped_surface_vec_ptr->size() )
    };
    const long long                                         n_file_bytes_before              = (m_file_handle != nullptr) ? ::_ftelli64(m_file_handle)
                                                                                                                          : -1;
    std::vector<uint8_t>                                    helper_u8_vec;
    bool                                                    result                           = false;
    Capture::APIDumperDLL::ZstdFileWriterUniquePtr          writer_ptr;

    /* Items are written out as they are put together, so that a big segment need not exist in memory twice. */
    const auto flush_func = [&]()
    {
        bool is_written = true;

        if (helper_u8_vec.empty() == false)
        {
            is_written = writer_ptr->write(helper_u8_vec.data(),
                                           helper_u8_vec.size() );

            helper_u8_vec.clear();
        }

        return is_written;
    };

    if (n_file_bytes_before < 0)
    {
        goto end;
    }

    writer_ptr = Capture::APIDumperDLL::ZstdFileWriter::create(m_file_handle,
                                                               COMPRESSION_LEVEL);

    if (writer_ptr == nullptr)
    {
        goto end;
    }

    append_u32_values(&helper_u8_vec,
                      counts_u32vec3,
                      N_SEGMENT_COUNTS);

    if (flush_func() == false)
    {
        goto end;
    }

    for (const auto& call : *in_segment.dumped_api_call_vec_ptr)
    {
        serialize_call(call,
                       &helper_u8_vec);

        if (flush_func() == false)
        {
            goto end;
        }
    }

    for (const auto& chunk_ptr : in_segment.data_chunk_ptr_vec)
    {
        const uint32_t n_chunk_bytes = (chunk_ptr != nullptr) ? static_cast<uint32_t>(chunk_ptr->size() )
                                                              : 0;

        append_u32_values(&helper_u8_vec,
                          &n_chunk_bytes,
                          1);

        if (flush_func() == false)
        {
            goto end;
        }

        if (n_chunk_bytes != 0)
        {
            if (writer_ptr->write(chunk_ptr->data(),
                                  n_chunk_bytes) == false)
            {
                goto end;
            }
        }
    }

    for (const auto& surface : *in_segment.dumped_surface_vec_ptr)
    {
        const uint32_t header_u32vec3[N_SURFACE_HEADER_FIELDS] =
        {
            surface.extents_u32vec2.at(0),
            surface.extents_u32vec2.at(1),
            static_cast<uint32_t>(surface.data_u8_vec.size() )
        };

        append_u32_values(&helper_u8_vec,
                          header_u32vec3,
                          N_SURFACE_HEADER_FIELDS);

        if (flush_func() == false)
        {
            goto end;
        }

        if (surface.data_u8_vec.empty() == false)
        {
            if (writer_ptr->write(surface.data_u8_vec.data(),
                                  surface.data_u8_vec.size() ) == false)
            {
                goto end;
            }
        }
    }

    if (writer_ptr->finish() == false)
    {
        goto end;
    }

    m_n_bytes_written += static_cast<uint64_t>(::_ftelli64(m_file_handle) - n_file_bytes_before);

    result = true;
end:
    return result;
}
