/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "Analysis/APIDumpAnalysis/frontends/call_view.h"
#include "Analysis/APIDumpAnalysis/hasher.h"

Analysis::APIDumpAnalysis::Frontends::CallView::CallView(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                         const Capture::APIDumpLoader::Workload&      in_workload,
                                                         const size_t&                                in_n_head_arguments)
    :m_call            (in_call),
     m_n_head_arguments(in_n_head_arguments),
     m_workload        (in_workload)
{
    /* Stub */
}

const Capture::APIDumpLoader::DataChunk* Analysis::APIDumpAnalysis::Frontends::CallView::find_chunk_ptr(const ArgumentVector& in_argument_vec,
                                                                                                        const size_t&         in_n_argument) const
{
    const Capture::APIDumpLoader::DataChunk* result_ptr = nullptr;

    if (in_n_argument                          <  in_argument_vec.size()                &&
        in_argument_vec.at(in_n_argument).type == APIInterceptor::ARGTYPE_DATA_CHUNK_ID)
    {
        result_ptr = Capture::APIDumpLoader::get_data_chunk_ptr(m_workload,
                                                                in_argument_vec.at(in_n_argument).get_data_chunk_id() );
    }

    return result_ptr;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::CallView::find_pointer(const ArgumentVector& in_argument_vec,
                                                                      const size_t&         in_n_argument)
{
    uint64_t result = 0;

    if (in_n_argument                          <  in_argument_vec.size()           &&
        in_argument_vec.at(in_n_argument).type == APIInterceptor::ARGTYPE_VOID_PTR)
    {
        result = in_argument_vec.at(in_n_argument).get_ptr_as_u64();
    }

    return result;
}

const Capture::APIDumpLoader::DumpedAPICall& Analysis::APIDumpAnalysis::Frontends::CallView::get_call() const
{
    return m_call;
}

const Capture::APIDumpLoader::DataChunk* Analysis::APIDumpAnalysis::Frontends::CallView::get_chunk_ptr(const size_t& in_n_argument) const
{
    return find_chunk_ptr(m_call.arg_in_vec,
                          in_n_argument);
}

bool Analysis::APIDumpAnalysis::Frontends::CallView::get_data_reference(const size_t&                             in_n_argument,
                                                                        Analysis::APIDumpAnalysis::DataReference* out_reference_ptr) const
{
    const Capture::APIDumpLoader::DataChunk* chunk_ptr = get_chunk_ptr(in_n_argument);
    bool                                     result    = (chunk_ptr != nullptr);

    if (result)
    {
        out_reference_ptr->content_hash  = Hasher::hash_bytes                                   (chunk_ptr->data(),
                                                                                                 chunk_ptr->size() );
        out_reference_ptr->data_chunk_id = m_call.arg_in_vec.at(in_n_argument).get_data_chunk_id();
        out_reference_ptr->n_bytes       = chunk_ptr->size                                      ();
    }

    return result;
}

const Capture::APIDumpLoader::DataChunk* Analysis::APIDumpAnalysis::Frontends::CallView::get_extra_chunk_ptr(const size_t& in_n_extra) const
{
    return get_chunk_ptr(get_n_head_arguments() + in_n_extra);
}

float Analysis::APIDumpAnalysis::Frontends::CallView::get_f32(const size_t& in_n_argument) const
{
    float result = 0.0f;

    if (in_n_argument                            <  m_call.arg_in_vec.size()    &&
        m_call.arg_in_vec.at(in_n_argument).type == APIInterceptor::ARGTYPE_F32)
    {
        result = m_call.arg_in_vec.at(in_n_argument).get_fp32();
    }

    return result;
}

int32_t Analysis::APIDumpAnalysis::Frontends::CallView::get_i32(const size_t& in_n_argument) const
{
    int32_t result = 0;

    if (in_n_argument                            <  m_call.arg_in_vec.size()    &&
        m_call.arg_in_vec.at(in_n_argument).type == APIInterceptor::ARGTYPE_I32)
    {
        result = m_call.arg_in_vec.at(in_n_argument).get_i32();
    }

    return result;
}

size_t Analysis::APIDumpAnalysis::Frontends::CallView::get_n_head_arguments() const
{
    return m_n_head_arguments;
}

const Capture::APIDumpLoader::DataChunk* Analysis::APIDumpAnalysis::Frontends::CallView::get_out_chunk_ptr(const size_t& in_n_argument) const
{
    return find_chunk_ptr(m_call.arg_out_vec,
                          in_n_argument);
}

uint64_t Analysis::APIDumpAnalysis::Frontends::CallView::get_out_pointer(const size_t& in_n_argument) const
{
    return find_pointer(m_call.arg_out_vec,
                        in_n_argument);
}

uint32_t Analysis::APIDumpAnalysis::Frontends::CallView::get_out_u32(const size_t& in_n_argument) const
{
    uint32_t result = 0;

    if (in_n_argument                             <  m_call.arg_out_vec.size()   &&
        m_call.arg_out_vec.at(in_n_argument).type == APIInterceptor::ARGTYPE_U32)
    {
        result = m_call.arg_out_vec.at(in_n_argument).get_u32();
    }

    return result;
}

uint64_t Analysis::APIDumpAnalysis::Frontends::CallView::get_pointer(const size_t& in_n_argument) const
{
    return find_pointer(m_call.arg_in_vec,
                        in_n_argument);
}

uint64_t Analysis::APIDumpAnalysis::Frontends::CallView::get_returned_pointer() const
{
    uint64_t result = 0;

    if (m_call.returned_value_ptr       != nullptr                          &&
        m_call.returned_value_ptr->type == APIInterceptor::ARGTYPE_VOID_PTR)
    {
        result = m_call.returned_value_ptr->get_ptr_as_u64();
    }

    return result;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::CallView::get_returned_u32() const
{
    uint32_t result = 0;

    if (m_call.returned_value_ptr       != nullptr                     &&
        m_call.returned_value_ptr->type == APIInterceptor::ARGTYPE_U32)
    {
        result = m_call.returned_value_ptr->get_u32();
    }

    return result;
}

uint32_t Analysis::APIDumpAnalysis::Frontends::CallView::get_u32(const size_t& in_n_argument) const
{
    uint32_t result = 0;

    if (in_n_argument                            <  m_call.arg_in_vec.size()    &&
        m_call.arg_in_vec.at(in_n_argument).type == APIInterceptor::ARGTYPE_U32)
    {
        result = m_call.arg_in_vec.at(in_n_argument).get_u32();
    }

    return result;
}

bool Analysis::APIDumpAnalysis::Frontends::CallView::succeeded() const
{
    bool result = false;

    if (m_call.returned_value_ptr       != nullptr                     &&
        m_call.returned_value_ptr->type == APIInterceptor::ARGTYPE_I32)
    {
        result = (m_call.returned_value_ptr->get_i32() >= 0);
    }

    return result;
}
