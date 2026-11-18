/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_index/state_segment_builder.h"
#include "common/macros.h"
#include "common/workload_format.h"
#include "zstd.h"
#include <string.h>

void Analysis::APIDumpIndex::StateSegmentBuilder::append_call(const Capture::APIDumpLoader::DumpedAPICall&  in_call,
                                                              std::vector<uint8_t>* inout_byte_vec_ptr)
{
    static_assert(sizeof(APIInterceptor::APIFunction) == sizeof(uint32_t),
                  "The function takes 4 bytes in the file");

    const APIInterceptor::APIFunctionArgument no_value;

    const uint32_t n_in_arguments  = static_cast<uint32_t>(in_call.arg_in_vec.size() );
    const uint32_t n_out_arguments = static_cast<uint32_t>(in_call.arg_out_vec.size() );

    append_u32(static_cast<uint32_t>(in_call.func),
               inout_byte_vec_ptr);
    append_u32(n_in_arguments,
               inout_byte_vec_ptr);
    append_u32(n_out_arguments,
               inout_byte_vec_ptr);
    append_u32(in_call.thread_id,
               inout_byte_vec_ptr);

    const size_t timestamp_offset = inout_byte_vec_ptr->size();

    inout_byte_vec_ptr->resize(timestamp_offset + sizeof(uint64_t) );

    ::memcpy(inout_byte_vec_ptr->data() + timestamp_offset,
            &in_call.timestamp_qpc,
             sizeof(uint64_t) );

    for (const auto& argument : in_call.arg_in_vec)
    {
        argument.serialize_to_u8_vec(inout_byte_vec_ptr);
    }

    for (const auto& argument : in_call.arg_out_vec)
    {
        argument.serialize_to_u8_vec(inout_byte_vec_ptr);
    }

    if (in_call.returned_value_ptr != nullptr)
    {
        in_call.returned_value_ptr->serialize_to_u8_vec(inout_byte_vec_ptr);
    }
    else
    {
        no_value.serialize_to_u8_vec(inout_byte_vec_ptr);
    }
}

void Analysis::APIDumpIndex::StateSegmentBuilder::append_u32(const uint32_t&       in_value,
                                                             std::vector<uint8_t>* inout_byte_vec_ptr)
{
    const size_t offset = inout_byte_vec_ptr->size();

    inout_byte_vec_ptr->resize(offset + sizeof(uint32_t) );

    ::memcpy(inout_byte_vec_ptr->data() + offset,
            &in_value,
             sizeof(uint32_t) );
}

bool Analysis::APIDumpIndex::StateSegmentBuilder::build(const Capture::APIDumpLoader::Workload&            in_segment,
                                                        const std::vector<size_t>& in_call_index_vec,
                                                        std::vector<uint8_t>*      out_frame_byte_vec_ptr)
{
    const size_t         n_calls          = (in_segment.dumped_api_call_vec_ptr != nullptr) ? in_segment.dumped_api_call_vec_ptr->size()
                                                                                            : 0;
    const size_t         n_chunks         = in_segment.data_chunk_ptr_vec.size();
    const size_t         n_surfaces       = (in_segment.dumped_surface_vec_ptr  != nullptr) ? in_segment.dumped_surface_vec_ptr->size()
                                                                                            : 0;
    std::vector<uint8_t> call_byte_vec;
    std::vector<bool>    is_chunk_kept_vec(n_chunks,
                                           false);
    std::vector<uint8_t> raw_byte_vec;
    bool                 result           = false;

    for (const auto& call_index : in_call_index_vec)
    {
        AI_ASSERT_ALWAYS(call_index < n_calls,
                         "A state frame is made of a call that the segment does not have.");

        const auto& call = in_segment.dumped_api_call_vec_ptr->at(call_index);

        append_call(call,
                   &call_byte_vec);

        for (const auto* argument_vec_ptr : {&call.arg_in_vec, &call.arg_out_vec})
        {
            for (const auto& argument : *argument_vec_ptr)
            {
                if (argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
                {
                    const uint32_t chunk_id = argument.get_data_chunk_id();

                    if (chunk_id                                  >= in_segment.first_data_chunk_id &&
                        chunk_id - in_segment.first_data_chunk_id <  n_chunks)
                    {
                        is_chunk_kept_vec.at(chunk_id - in_segment.first_data_chunk_id) = true;
                    }
                }
            }
        }
    }

    append_u32(static_cast<uint32_t>(in_call_index_vec.size() ),
              &raw_byte_vec);
    append_u32(static_cast<uint32_t>(n_chunks),
              &raw_byte_vec);
    append_u32(static_cast<uint32_t>(n_surfaces),
              &raw_byte_vec);

    raw_byte_vec.insert(raw_byte_vec.end(),
                        call_byte_vec.begin(),
                        call_byte_vec.end() );

    for (size_t n_chunk = 0;
                n_chunk < n_chunks;
              ++n_chunk)
    {
        const auto* chunk_ptr = in_segment.data_chunk_ptr_vec.at(n_chunk).get();
        const bool  is_kept   = (is_chunk_kept_vec.at(n_chunk) &&
                                 chunk_ptr                     != nullptr);

        append_u32((is_kept) ? static_cast<uint32_t>(chunk_ptr->size() )
                             : 0,
                  &raw_byte_vec);

        if (is_kept)
        {
            raw_byte_vec.insert(raw_byte_vec.end(),
                                chunk_ptr->begin(),
                                chunk_ptr->end() );
        }
    }

    for (size_t n_surface = 0;
                n_surface < n_surfaces;
              ++n_surface)
    {
        const auto& surface = in_segment.dumped_surface_vec_ptr->at(n_surface);

        append_u32(surface.extents_u32vec2[0],
                  &raw_byte_vec);
        append_u32(surface.extents_u32vec2[1],
                  &raw_byte_vec);
        append_u32(0, /* in_value: no bytes of image */
                  &raw_byte_vec);
    }

    {
        constexpr int COMPRESSION_LEVEL = 3;

        out_frame_byte_vec_ptr->resize(::ZSTD_compressBound(raw_byte_vec.size() ) );

        const size_t n_frame_bytes = ::ZSTD_compress(out_frame_byte_vec_ptr->data(),
                                                     out_frame_byte_vec_ptr->size(),
                                                     raw_byte_vec.data(),
                                                     raw_byte_vec.size(),
                                                     COMPRESSION_LEVEL);

        result = (::ZSTD_isError(n_frame_bytes) == 0);

        out_frame_byte_vec_ptr->resize((result) ? n_frame_bytes
                                                : 0);
    }

    return result;
}
