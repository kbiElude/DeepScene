/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "frame_buffer.h"

Stereo::Replay::EngineDLL::FrameBuffer::FrameBuffer()
    :m_pointer_n_bytes(0)
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::FrameBuffer::add_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                      const IChunkSource&                          in_chunk_source)
{
    Capture::APIDumpLoader::DumpedAPICall copy = copy_call(in_call);

    m_pointer_n_bytes = in_chunk_source.get_pointer_n_bytes();

    for (const auto* argument_vec_ptr : {&in_call.arg_in_vec,
                                         &in_call.arg_out_vec})
    {
        for (const auto& argument : *argument_vec_ptr)
        {
            if (argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
            {
                const uint32_t chunk_id       = argument.get_data_chunk_id        ();
                const auto*    chunk_ptr      = in_chunk_source.get_data_chunk_ptr(chunk_id);
                const auto     chunk_iterator = m_chunk_map.lower_bound           (chunk_id);
                const bool     is_new         = (chunk_iterator        == m_chunk_map.end() ||
                                                 chunk_iterator->first != chunk_id);

                /* One search for both the check and the place to put the chunk. */
                if (chunk_ptr != nullptr &&
                    is_new)
                {
                    m_chunk_map.emplace_hint(chunk_iterator,
                                             chunk_id,
                                             *chunk_ptr);
                }
            }
        }
    }

    m_call_vec.push_back(std::move(copy) );
}

Capture::APIDumpLoader::DumpedAPICall Stereo::Replay::EngineDLL::FrameBuffer::copy_call(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    Capture::APIDumpLoader::DumpedAPICall result;

    result.arg_in_vec    = in_call.arg_in_vec;
    result.arg_out_vec   = in_call.arg_out_vec;
    result.func          = in_call.func;
    result.thread_id     = in_call.thread_id;
    result.timestamp_qpc = in_call.timestamp_qpc;

    if (in_call.returned_value_ptr != nullptr)
    {
        result.returned_value_ptr.reset(new APIInterceptor::APIFunctionArgument(*in_call.returned_value_ptr) );
    }

    return result;
}

void Stereo::Replay::EngineDLL::FrameBuffer::clear()
{
    m_call_vec.clear ();
    m_chunk_map.clear();
}

const std::vector<Capture::APIDumpLoader::DumpedAPICall>& Stereo::Replay::EngineDLL::FrameBuffer::get_call_vec() const
{
    return m_call_vec;
}

const Capture::APIDumpLoader::DataChunk* Stereo::Replay::EngineDLL::FrameBuffer::get_data_chunk_ptr(const uint32_t& in_data_chunk_id) const
{
    const auto chunk_iterator = m_chunk_map.find(in_data_chunk_id);

    return (chunk_iterator != m_chunk_map.end() ) ? &chunk_iterator->second
                                                  : nullptr;
}

uint32_t Stereo::Replay::EngineDLL::FrameBuffer::get_pointer_n_bytes() const
{
    return m_pointer_n_bytes;
}
