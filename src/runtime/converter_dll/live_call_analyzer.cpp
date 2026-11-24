/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_vertex_reader.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_window_extents.h"
#include "analysis/api_dump_index/gl_function_classifier.h"
#include "capture/api_dump_loader/api_info.h"
#include "live_call_analyzer.h"
#include <algorithm>
#include <Windows.h>

Stereo::ConverterDLL::LiveCallAnalyzer::LiveCallAnalyzer()
    :m_completed_frame_shape_key    (0),
     m_has_completed_frame          (false),
     m_is_analyzed_before_the_call_u8_vec(APIInterceptor::APIFUNCTION_COUNT),
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

    for (uint32_t n_api_func = 0;
                  n_api_func < APIInterceptor::APIFUNCTION_COUNT;
                ++n_api_func)
    {
        m_is_analyzed_before_the_call_u8_vec.at(n_api_func) = is_analyzed_before_the_call(static_cast<APIInterceptor::APIFunction>(n_api_func) ) ? 1
                                                                                                                                               : 0;
    }
}

Stereo::ConverterDLL::LiveCallAnalyzer::ReturnedValueUniquePtr Stereo::ConverterDLL::LiveCallAnalyzer::acquire_returned_value(const APIInterceptor::APIFunctionArgument& in_value)
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

void Stereo::ConverterDLL::LiveCallAnalyzer::analyze(const Capture::APIDumpLoader::DumpedAPICall& in_call)
{
    /* Chunk ids that are far apart indicate a problem. A chunk that is far from the first one is dropped instead of allocating room for it. */
    constexpr uint32_t MAX_CHUNK_ID_SPAN = 1u << 16;

    m_chunk_id_vec.clear();

    /* A call that reads a structure given to it has the chunk both in its in-arguments and in its out-arguments. The chunk is taken only once,
     * because taking it a second time would return nothing.
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

    /* The chunks of the previous call go back to the pool with their memory, so that the chunks of this call need no allocation. */
    for (auto& chunk_ptr : m_workload.data_chunk_ptr_vec)
    {
        m_chunk_pool.recycle(&chunk_ptr);
    }

    m_workload.data_chunk_ptr_vec.clear();

    m_workload.first_data_chunk_id = 0;

    if (m_chunk_id_vec.empty() == false)
    {
        /* Most calls have a single chunk. It needs no sorting. */
        if (m_chunk_id_vec.size() > 1)
        {
            std::sort(m_chunk_id_vec.begin(),
                      m_chunk_id_vec.end  () );

            const auto duplicates_begin_iterator = std::unique(m_chunk_id_vec.begin(),
                                                               m_chunk_id_vec.end  () );

            m_chunk_id_vec.erase(duplicates_begin_iterator,
                                 m_chunk_id_vec.end() );
        }

        const uint32_t first_chunk_id = m_chunk_id_vec.front();

        m_workload.first_data_chunk_id = first_chunk_id;

        /* The ids are sorted, so the ids that are too far from the first one are at the end. */
        while (m_chunk_id_vec.back() - first_chunk_id >= MAX_CHUNK_ID_SPAN)
        {
            m_chunk_id_vec.pop_back();
        }

        m_destination_ptr_vec.clear();

        m_workload.data_chunk_ptr_vec.resize(m_chunk_id_vec.back() - first_chunk_id + 1);

        /* Each chunk is taken into a vector from the pool. The vector stays in the workload if the chunk exists. */
        for (const auto& chunk_id : m_chunk_id_vec)
        {
            auto& chunk_ptr = m_workload.data_chunk_ptr_vec.at(chunk_id - first_chunk_id);

            chunk_ptr = m_chunk_pool.acquire();

            m_destination_ptr_vec.push_back(chunk_ptr.get() );
        }

        APIInterceptor::take_data_chunks_by_id(m_chunk_id_vec,
                                               m_destination_ptr_vec,
                                              &m_is_taken_u8_vec);

        for (size_t n_chunk = 0;
                    n_chunk < m_chunk_id_vec.size();
                  ++n_chunk)
        {
            const uint32_t chunk_id  = m_chunk_id_vec.at(n_chunk);
            auto&          chunk_ptr = m_workload.data_chunk_ptr_vec.at(chunk_id - first_chunk_id);

            if (m_is_taken_u8_vec.at(n_chunk) != 0)
            {
                if (m_chunk_func)
                {
                    m_chunk_func(in_call.func,
                                 chunk_ptr.get() );
                }
            }
            else
            {
                m_chunk_pool.recycle(&chunk_ptr);
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

void Stereo::ConverterDLL::LiveCallAnalyzer::fill_call(const APIInterceptor::APIFunction&         in_function,
                                                       const uint32_t&                            in_n_arguments,
                                                       const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                       Capture::APIDumpLoader::DumpedAPICall*     out_call_ptr)
{
    out_call_ptr->arg_in_vec.assign(in_arguments_ptr,
                                    in_arguments_ptr + in_n_arguments);
    out_call_ptr->arg_out_vec.clear();

    out_call_ptr->func          = in_function;
    out_call_ptr->thread_id     = static_cast<uint32_t>(::GetCurrentThreadId() );
    /* The call is not timed. The converter never uses the time of a call. The time matters only to the tools that read a workload file, where the dumper stamps each call. */
    out_call_ptr->timestamp_qpc = 0;

    if (out_call_ptr->returned_value_ptr != nullptr)
    {
        m_returned_value_pool.push_back(std::move(out_call_ptr->returned_value_ptr) );
    }
}

bool Stereo::ConverterDLL::LiveCallAnalyzer::get_last_call_chunk(const uint32_t& in_chunk_id,
                                                                 const void**    out_data_ptr_ptr,
                                                                 uint32_t*       out_n_bytes_ptr) const
{
    const size_t n_chunks    = m_workload.data_chunk_ptr_vec.size();
    const bool   is_in_range = (n_chunks                                     >  0                              &&
                                in_chunk_id                                  >= m_workload.first_data_chunk_id &&
                                in_chunk_id - m_workload.first_data_chunk_id <  n_chunks);
    bool         result      = false;

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

void Stereo::ConverterDLL::LiveCallAnalyzer::add_block_vertices(const uint32_t& in_n_vertices)
{
    const uint32_t thread_id = static_cast<uint32_t>(::GetCurrentThreadId() );

    m_session.add_block_vertices(thread_id,
                                 in_n_vertices);
}

bool Stereo::ConverterDLL::LiveCallAnalyzer::get_current_gl_matrices(std::array<float, 16>* out_modelview_f32mat_ptr,
                                                                     std::array<float, 16>* out_projection_f32mat_ptr,
                                                                     uint32_t*              out_matrix_mode_ptr) const
{
    const uint32_t thread_id = static_cast<uint32_t>(::GetCurrentThreadId() );

    return m_session.get_opengl_frontend().get_current_matrices(thread_id,
                                                                out_modelview_f32mat_ptr,
                                                                out_projection_f32mat_ptr,
                                                                out_matrix_mode_ptr);
}

const std::vector<Analysis::APIDumpAnalysis::DrawKeys>& Stereo::ConverterDLL::LiveCallAnalyzer::get_last_call_draw_key_vec() const
{
    return m_session.get_call_draw_key_vec();
}

uint64_t Stereo::ConverterDLL::LiveCallAnalyzer::get_last_draw_cover_key() const
{
    return m_session.get_last_draw_cover_key();
}

bool Stereo::ConverterDLL::LiveCallAnalyzer::get_last_draw_is_sampling_copied_color() const
{
    return m_session.get_last_draw_is_sampling_copied_color();
}

uint64_t Stereo::ConverterDLL::LiveCallAnalyzer::get_last_draw_signature_key() const
{
    return m_session.get_last_draw_signature_key();
}

Stereo::ConverterDLL::LiveCallAnalyzer::PendingCalls* Stereo::ConverterDLL::LiveCallAnalyzer::get_pending_calls_ptr()
{
    const uint32_t thread_id = static_cast<uint32_t>(::GetCurrentThreadId() );

    /* Almost all calls come from one thread, so the map is searched only when another thread makes a call. */
    if (m_last_thread_pending_calls_ptr == nullptr   ||
        m_last_thread_id                != thread_id)
    {
        m_last_thread_id                = thread_id;
        m_last_thread_pending_calls_ptr = &m_pending_calls_by_thread_map[thread_id];
    }

    return m_last_thread_pending_calls_ptr;
}

const Analysis::APIDumpAnalysis::Viewport& Stereo::ConverterDLL::LiveCallAnalyzer::get_viewport() const
{
    return m_session.get_viewport();
}

bool Stereo::ConverterDLL::LiveCallAnalyzer::is_analyzed_before_the_call(const APIInterceptor::APIFunction& in_function)
{
    Analysis::APIDumpAnalysis::Frontends::GLVertexLayout layout;

    /* The front end reads the arguments of a vertex call, never what it returns, and it returns nothing. */
    return Analysis::APIDumpAnalysis::Frontends::GLVertexReader::find_layout(in_function,
                                                                            &layout);
}

bool Stereo::ConverterDLL::LiveCallAnalyzer::is_attribute_call(const APIInterceptor::APIFunction& in_function)
{
    return (Analysis::APIDumpIndex::GLFunctionClassifier::get_kind(in_function) == Analysis::APIDumpIndex::GLFunctionKind::CURRENT_VALUE);
}

bool Stereo::ConverterDLL::LiveCallAnalyzer::is_draw(const APIInterceptor::APIFunction& in_function)
{
    return (in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVE        ||
            in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVESTRIDED ||
            in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWINDEXEDPRIMITIVEVB      ||
            in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVE               ||
            in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVESTRIDED        ||
            in_function == APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_DRAWPRIMITIVEVB             ||
            in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE        ||
            in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVEUP      ||
            in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVE               ||
            in_function == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWPRIMITIVEUP            ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLCALLLIST                                     ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLDRAWARRAYS                                   ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLDRAWELEMENTS                                 ||
            in_function == APIInterceptor::APIFUNCTION_GL_GLEND);
}

bool Stereo::ConverterDLL::LiveCallAnalyzer::is_followed(const APIInterceptor::APIFunction& in_function)
{
    const auto api_group = Capture::APIDumpLoader::get_api_group(in_function);

    /* SwapBuffers is a GDI32 function that ends an OpenGL frame. The calls that make the window of an OpenGL program tell its default viewport.
     * The rest of GDI32 and USER32 is not followed.
     */
    return (api_group == Capture::APIDumpLoader::APIGroup::D3D7 ||
            api_group == Capture::APIDumpLoader::APIGroup::D3D8 ||
            api_group == Capture::APIDumpLoader::APIGroup::GL   ||
            api_group == Capture::APIDumpLoader::APIGroup::WGL  ||
            in_function == APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS ||
            Analysis::APIDumpAnalysis::Frontends::GLWindowExtents::follows_function(in_function) );
}

void Stereo::ConverterDLL::LiveCallAnalyzer::on_post(const APIInterceptor::APIFunction&         in_function,
                                                     const uint32_t&                            in_n_arguments_out,
                                                     const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr,
                                                     const APIInterceptor::APIFunctionArgument* in_returned_value_ptr)
{
    auto* pending_calls_ptr = get_pending_calls_ptr();

    /* Find the latest call of this function that this thread still waits for. A call that the interceptor makes inside it comes later in the list. */
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

            /* Move the call behind the active ones. It keeps its vectors for reuse. */
            std::rotate(pending_calls_ptr->call_vec.begin() + static_cast<std::ptrdiff_t>(n_call - 1),
                        pending_calls_ptr->call_vec.begin() + static_cast<std::ptrdiff_t>(n_call),
                        pending_calls_ptr->call_vec.begin() + static_cast<std::ptrdiff_t>(pending_calls_ptr->n_calls) );

            pending_calls_ptr->n_calls--;

            break;
        }
    }
}

bool Stereo::ConverterDLL::LiveCallAnalyzer::on_pre(const APIInterceptor::APIFunction&         in_function,
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
        /* The front end only follows a draw that succeeded. The draw has not been made yet, so it is assumed to succeed. The call is not stored:
         * its slot stays past n_calls.
         */
        call.returned_value_ptr = acquire_returned_value(APIInterceptor::APIFunctionArgument::create_i32(0) );

        analyze(call);
    }
    else
    if (m_is_analyzed_before_the_call_u8_vec.at(in_function) != 0)
    {
        /* A call with no result is analysed as it is, and nobody calls on_post() for it. Its slot stays past n_calls. */
        analyze(call);
    }
    else
    {
        pending_calls_ptr->n_calls++;
    }

    return result;
}

void Stereo::ConverterDLL::LiveCallAnalyzer::set_chunk_func(const ChunkFunc& in_chunk_func)
{
    m_chunk_func = in_chunk_func;
}

bool Stereo::ConverterDLL::LiveCallAnalyzer::take_completed_frame_shape_key(uint64_t* out_shape_key_ptr)
{
    const bool result = m_has_completed_frame;

    if (result)
    {
        *out_shape_key_ptr    = m_completed_frame_shape_key;
        m_has_completed_frame = false;
    }

    return result;
}
