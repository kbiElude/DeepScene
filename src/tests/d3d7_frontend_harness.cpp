/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "d3d7_frontend_harness.h"

Tests::D3D7FrontendHarness::D3D7FrontendHarness()
    :m_frontend           (&m_builder),
     m_n_builder_calls_fed(0),
     m_n_calls_fed        (0)
{
    /* Stub */
}

void Tests::D3D7FrontendHarness::feed(const D3D7CallBuilder& in_builder)
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

Analysis::APIDumpAnalysis::ModelBuilder& Tests::D3D7FrontendHarness::get_builder()
{
    return m_builder;
}

Analysis::APIDumpAnalysis::Frontends::D3D7Frontend& Tests::D3D7FrontendHarness::get_frontend()
{
    return m_frontend;
}

const Analysis::APIDumpAnalysis::Model& Tests::D3D7FrontendHarness::get_model() const
{
    return m_builder.get_model();
}
