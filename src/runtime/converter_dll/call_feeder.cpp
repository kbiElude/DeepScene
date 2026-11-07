/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "capture/api_dump_loader/api_info.h"
#include "call_feeder.h"
#include <algorithm>
#include <Windows.h>

Stereo::ConverterDLL::CallFeeder::CallFeeder()
    :m_completed_frame_shape_key    (0),
     m_has_completed_frame          (false),
     m_last_thread_id               (0),
     m_last_thread_pending_calls_ptr(nullptr),
     m_qpc_frequency                (0),
     m_session                      (false) /* in_keeps_history */
{
    LARGE_INTEGER frequency;

    ::QueryPerformanceFrequency(&frequency);

    m_qpc_frequency = static_cast<uint64_t>(frequency.QuadPart);

    m_workload.pointer_n_bytes = sizeof(void*);
    m_workload.qpc_frequency   = m_qpc_frequency;
}

Stereo::ConverterDLL::CallFeeder::ReturnedValueUniquePtr Stereo::ConverterDLL::CallFeeder::acquire_returned_value(const APIInterceptor::APIFunctionArgument& in_value)
{
    ReturnedValueUniquePtr result_ptr;

    if (m_returned_value_pool.empty() )
    {
        result_ptr.reset(new APIInterceptor::APIFunctionArgument(in_value) );
    }
    else
    {
        result_ptr = std::move(m_returned_value_pool.back() );

        m_returned_value_pool.pop_back();

        *result_ptr = in_value;
    }

    return result_ptr;
}

void Stereo::ConverterDLL::CallFeeder::analyze(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    /* Chunks are far apart only if something is wrong; one that is far from the others is left out rather than allocate for it. */
    constexpr uint32_t MAX_CHUNK_ID_SPAN = 1u << 16;

    m_chunk_id_vec.clear();

    /* A chunk is in the arguments of the call and in its results when the call reads it (a structure it is given): it is taken
     * once, or the second time would replace it with nothing.
     */
    for (const auto* argument_vec_ptr : {&in_call.arg_in_vec, &in_call.arg_out_vec})
    {
        for (const auto& argument : *argument_vec_ptr)
        {
            if (argument.type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
            {
                m_chunk_id_vec.push_back(argument.get_data_chunk_id() );
            }
        }
    }

    m_workload.data_chunk_ptr_vec.clear();

    m_workload.first_data_chunk_id = 0;

    if (m_chunk_id_vec.empty() == false)
    {
        std::sort(m_chunk_id_vec.begin(),
                  m_chunk_id_vec.end  () );

        const auto duplicates_begin_iterator = std::unique(m_chunk_id_vec.begin(),
                                                           m_chunk_id_vec.end  () );

        m_chunk_id_vec.erase(duplicates_begin_iterator,
                             m_chunk_id_vec.end() );

        const uint32_t first_chunk_id = m_chunk_id_vec.front();

        m_workload.first_data_chunk_id = first_chunk_id;

        for (const auto& chunk_id : m_chunk_id_vec)
        {
            if (chunk_id - first_chunk_id < MAX_CHUNK_ID_SPAN)
            {
                if (m_workload.data_chunk_ptr_vec.size() <= chunk_id - first_chunk_id)
                {
                    m_workload.data_chunk_ptr_vec.resize(chunk_id - first_chunk_id + 1);
                }

                Capture::APIDumpLoader::DataChunkUniquePtr chunk_ptr(new Capture::APIDumpLoader::DataChunk());

                if (APIInterceptor::take_data_chunk(chunk_id,
                                                    chunk_ptr.get() ) )
                {
                    if (m_chunk_func)
                    {
                        m_chunk_func(in_call.func,
                                     chunk_ptr.get() );
                    }

                    m_workload.data_chunk_ptr_vec.at(chunk_id - first_chunk_id) = std::move(chunk_ptr);
                }
            }
        }
    }

    if (m_session.process_call(in_call,
                               m_workload) )
    {
        m_completed_frame_shape_key = m_session.get_completed_frame().shape_key;
        m_has_completed_frame       = true;
    }
}

void Stereo::ConverterDLL::CallFeeder::fill_call(const APIInterceptor::APIFunction&         in_function,
                                                 const uint32_t&                            in_n_arguments,
                                                 const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                 Capture::APIDumpLoader::DumpedAPICall*     out_call_ptr)
{
    LARGE_INTEGER timestamp;

    ::QueryPerformanceCounter(&timestamp);

    out_call_ptr->arg_in_vec.assign(in_arguments_ptr,
                                    in_arguments_ptr + in_n_arguments);
    out_call_ptr->arg_out_vec.clear();

    out_call_ptr->func          = in_function;
    out_call_ptr->thread_id     = static_cast<uint32_t>(::GetCurrentThreadId() );
    out_call_ptr->timestamp_qpc = static_cast<uint64_t>(timestamp.QuadPart);

    if (out_call_ptr->returned_value_ptr != nullptr)
    {
        m_returned_value_pool.push_back(std::move(out_call_ptr->returned_value_ptr) );
    }
}

bool Stereo::ConverterDLL::CallFeeder::get_last_call_chunk(const uint32_t& in_chunk_id,
                                                           const void**    out_data_ptr_ptr,
                                                           uint32_t*       out_n_bytes_ptr) const
{
    const bool is_in_range = (m_workload.data_chunk_ptr_vec.empty() == false                                     &&
                              in_chunk_id                             >= m_workload.first_data_chunk_id            &&
                              in_chunk_id - m_workload.first_data_chunk_id < m_workload.data_chunk_ptr_vec.size() );
    bool       result      = false;

    if (is_in_range)
    {
        const auto& chunk_ptr = m_workload.data_chunk_ptr_vec.at(in_chunk_id - m_workload.first_data_chunk_id);

        if (chunk_ptr != nullptr)
        {
            *out_data_ptr_ptr = chunk_ptr->data();
            *out_n_bytes_ptr  = static_cast<uint32_t>(chunk_ptr->size() );
            result            = true;
        }
    }

    return result;
}

uint64_t Stereo::ConverterDLL::CallFeeder::get_last_draw_cover_key() const
{
    return m_session.get_last_draw_cover_key();
}

uint64_t Stereo::ConverterDLL::CallFeeder::get_last_draw_signature_key() const
{
    return m_session.get_last_draw_signature_key();
}

Stereo::ConverterDLL::CallFeeder::PendingCalls* Stereo::ConverterDLL::CallFeeder::get_pending_calls_ptr()
{
    const uint32_t thread_id = static_cast<uint32_t>(::GetCurrentThreadId() );

    /* One thread makes almost all the calls: the map is asked only when another one makes one. */
    if (m_last_thread_pending_calls_ptr == nullptr   ||
        m_last_thread_id                != thread_id)
    {
        m_last_thread_id                = thread_id;
        m_last_thread_pending_calls_ptr = &m_pending_calls_by_thread_map[thread_id];
    }

    return m_last_thread_pending_calls_ptr;
}

const Analysis::APIDumpAnalysis::Viewport& Stereo::ConverterDLL::CallFeeder::get_viewport() const
{
    return m_session.get_viewport();
}

bool Stereo::ConverterDLL::CallFeeder::is_draw(const APIInterceptor::APIFunction& in_function)
{
    return (in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE         ||
            in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVESTRIDED ||
            in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVEVB      ||
            in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE               ||
            in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVESTRIDED        ||
            in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVEVB             ||
            in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE        ||
            in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP      ||
            in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE               ||
            in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP);
}

bool Stereo::ConverterDLL::CallFeeder::is_followed(const APIInterceptor::APIFunction& in_function)
{
    const auto api_group = Capture::APIDumpLoader::get_api_group(in_function);

    return (api_group == Capture::APIDumpLoader::APIGroup::D3D7 ||
            api_group == Capture::APIDumpLoader::APIGroup::D3D8);
}

void Stereo::ConverterDLL::CallFeeder::on_post(const APIInterceptor::APIFunction&         in_function,
                                               const uint32_t&                            in_n_arguments_out,
                                               const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr,
                                               const APIInterceptor::APIFunctionArgument* in_returned_value_ptr)
{
    auto* pending_calls_ptr = get_pending_calls_ptr();

    /* The latest call of the function that this thread is still waiting for; a call the interceptor made inside it is above it. */
    for (size_t n_call = pending_calls_ptr->n_calls;
                n_call > 0;
              --n_call)
    {
        auto& call = pending_calls_ptr->call_vec.at(n_call - 1);

        if (call.func == in_function)
        {
            call.arg_out_vec.assign(in_arguments_out_ptr,
                                    in_arguments_out_ptr + in_n_arguments_out);

            if (in_returned_value_ptr != nullptr)
            {
                call.returned_value_ptr = acquire_returned_value(*in_returned_value_ptr);
            }

            analyze(call);

            /* The call goes past the others, with its vectors, to be used again. */
            std::rotate(pending_calls_ptr->call_vec.begin() + static_cast<std::ptrdiff_t>(n_call - 1),
                        pending_calls_ptr->call_vec.begin() + static_cast<std::ptrdiff_t>(n_call),
                        pending_calls_ptr->call_vec.begin() + static_cast<std::ptrdiff_t>(pending_calls_ptr->n_calls) );

            pending_calls_ptr->n_calls--;

            break;
        }
    }
}

bool Stereo::ConverterDLL::CallFeeder::on_pre(const APIInterceptor::APIFunction&         in_function,
                                              const uint32_t&                            in_n_arguments,
                                              const APIInterceptor::APIFunctionArgument* in_arguments_ptr)
{
    auto*      pending_calls_ptr = get_pending_calls_ptr();
    const bool result            = is_draw              (in_function);

    if (pending_calls_ptr->n_calls == pending_calls_ptr->call_vec.size() )
    {
        pending_calls_ptr->call_vec.emplace_back();
    }

    auto& call = pending_calls_ptr->call_vec.at(pending_calls_ptr->n_calls);

    fill_call(in_function,
              in_n_arguments,
              in_arguments_ptr,
             &call);

    if (result)
    {
        /* The front end follows a draw that succeeded; the call is not made yet, so it is taken to succeed. It is not kept: the
         * slot stays past n_calls.
         */
        call.returned_value_ptr = acquire_returned_value(APIInterceptor::APIFunctionArgument::create_i32(0) );

        analyze(call);
    }
    else
    {
        pending_calls_ptr->n_calls++;
    }

    return result;
}

void Stereo::ConverterDLL::CallFeeder::set_chunk_func(const ChunkFunc& in_chunk_func)
{
    m_chunk_func = in_chunk_func;
}

bool Stereo::ConverterDLL::CallFeeder::take_completed_frame_shape_key(uint64_t* out_shape_key_ptr)
{
    const bool result = m_has_completed_frame;

    if (result)
    {
        *out_shape_key_ptr    = m_completed_frame_shape_key;
        m_has_completed_frame = false;
    }

    return result;
}
