/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d8_frontend_harness.h"
#include "Capture/APIDumpLoader/workload_reader.h"
#include "Capture/APIDumpLoader/api_info.h"

Tests::D3D8FrontendHarness::D3D8FrontendHarness()
    :m_frontend           (&m_builder),
     m_n_builder_calls_fed(0),
     m_n_calls_fed        (0)
{
    /* Stub */
}

void Tests::D3D8FrontendHarness::feed(const D3D8CallBuilder& in_builder)
{
    const auto& call_vec = *in_builder.get_workload().dumped_api_call_vec_ptr;

    for (;
         m_n_builder_calls_fed < call_vec.size();
         m_n_builder_calls_fed++)
    {
        m_builder.begin_call(m_n_calls_fed++,
                             call_vec.at(m_n_builder_calls_fed).timestamp_qpc);

        m_frontend.on_call(call_vec.at(m_n_builder_calls_fed),
                           in_builder.get_workload() );
    }
}

bool Tests::D3D8FrontendHarness::feed_file(const char* in_filename_ptr)
{
    auto                             reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(in_filename_ptr);
    bool                             result     = (reader_ptr != nullptr);
    Capture::APIDumpLoader::Workload segment;

    while (result                                      &&
           reader_ptr->read_next_segment(&segment) )
    {
        for (const auto& call : *segment.dumped_api_call_vec_ptr)
        {
            m_builder.begin_call(m_n_calls_fed++,
                                 call.timestamp_qpc);

            if (Capture::APIDumpLoader::get_api_group(call.func) == Capture::APIDumpLoader::APIGroup::D3D8)
            {
                m_frontend.on_call(call,
                                   segment);
            }
        }
    }

    return result;
}

Analysis::APIDumpAnalysis::ModelBuilder& Tests::D3D8FrontendHarness::get_builder()
{
    return m_builder;
}

Analysis::APIDumpAnalysis::Frontends::D3D8Frontend& Tests::D3D8FrontendHarness::get_frontend()
{
    return m_frontend;
}

const Analysis::APIDumpAnalysis::Model& Tests::D3D8FrontendHarness::get_model() const
{
    return m_builder.get_model();
}
