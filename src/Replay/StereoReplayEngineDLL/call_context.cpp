/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "call_context.h"

Replay::StereoReplayEngineDLL::CallContext::CallContext(ObjectRemapper* in_remapper_ptr)
    :m_call_ptr        (nullptr),
     m_n_head_arguments(0),
     m_remapper_ptr    (in_remapper_ptr),
     m_walker_ptr      (nullptr)
{
    /* Stub */
}

void Replay::StereoReplayEngineDLL::CallContext::begin_call(const Capture::APIDumpLoader::DumpedAPICall* in_call_ptr,
                                                                     const CallWalker*                   in_walker_ptr,
                                                                     const size_t&                       in_n_head_arguments)
{
    m_call_ptr         = in_call_ptr;
    m_n_head_arguments = in_n_head_arguments;
    m_walker_ptr       = in_walker_ptr;
}

const Capture::APIDumpLoader::DumpedAPICall& Replay::StereoReplayEngineDLL::CallContext::get_call() const
{
    return *m_call_ptr;
}

const uint8_t* Replay::StereoReplayEngineDLL::CallContext::get_chunk_data_ptr(const APIInterceptor::APIFunctionArgument* in_argument_ptr,
                                                                              size_t*                                    out_n_bytes_ptr) const
{
    const uint8_t* result_ptr = nullptr;

    *out_n_bytes_ptr = 0;

    if (in_argument_ptr       != nullptr &&
        in_argument_ptr->type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        const auto chunk_ptr = m_walker_ptr->get_data_chunk_ptr(in_argument_ptr->get_data_chunk_id() );

        if (chunk_ptr != nullptr &&
            chunk_ptr->empty() == false)
        {
            result_ptr       = chunk_ptr->data();
            *out_n_bytes_ptr = chunk_ptr->size();
        }
    }

    return result_ptr;
}

const APIInterceptor::APIFunctionArgument* Replay::StereoReplayEngineDLL::CallContext::get_in_argument_ptr(const size_t& in_n_argument) const
{
    return (in_n_argument < m_call_ptr->arg_in_vec.size() ) ? &m_call_ptr->arg_in_vec.at(in_n_argument)
                                                            : nullptr;
}

size_t Replay::StereoReplayEngineDLL::CallContext::get_n_head_arguments() const
{
    return m_n_head_arguments;
}

const APIInterceptor::APIFunctionArgument* Replay::StereoReplayEngineDLL::CallContext::get_out_argument_ptr(const size_t& in_n_argument) const
{
    return (in_n_argument < m_call_ptr->arg_out_vec.size() ) ? &m_call_ptr->arg_out_vec.at(in_n_argument)
                                                             : nullptr;
}

Replay::StereoReplayEngineDLL::ObjectRemapper* Replay::StereoReplayEngineDLL::CallContext::get_remapper_ptr() const
{
    return m_remapper_ptr;
}
