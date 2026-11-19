/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Runs the calls of a call builder through the front end of an API and a model builder, for the tests. The builder is any class that has
 * get_workload(), as the call builders of the tests have.
 */
#ifndef TESTS_FRONTEND_HARNESS_H
#define TESTS_FRONTEND_HARNESS_H

#include "analysis/api_dump_analysis/model_builder.h"
#include <stddef.h>
#include <stdint.h>

namespace Tests
{
    template<typename FrontendType, typename CallBuilderType>
    class FrontendHarness
    {
    public:
        /* With in_keeps_history false the model is built as the live converter builds it (no frames, no geometry). */
        explicit FrontendHarness(const bool& in_keeps_history = true)
            :m_builder            (in_keeps_history),
             m_frontend           (&m_builder),
             m_n_builder_calls_fed(0),
             m_n_calls_fed        (0)
        {
            /* Stub */
        }

        /* Runs the calls that were added to the call builder since the last time. */
        void feed(const CallBuilderType& in_call_builder)
        {
            const auto& call_vec = *in_call_builder.get_workload().dumped_api_call_vec_ptr;

            for (;
                 m_n_builder_calls_fed < call_vec.size();
                 m_n_builder_calls_fed++)
            {
                m_builder.begin_call(m_n_calls_fed++,
                                     call_vec.at(m_n_builder_calls_fed).timestamp_qpc);
                m_frontend.on_call  (call_vec.at(m_n_builder_calls_fed),
                                     in_call_builder.get_workload() );
            }
        }

        Analysis::APIDumpAnalysis::ModelBuilder& get_builder()
        {
            return m_builder;
        }

        FrontendType& get_frontend()
        {
            return m_frontend;
        }

        const Analysis::APIDumpAnalysis::Model& get_model() const
        {
            return m_builder.get_model();
        }

    private:
        Analysis::APIDumpAnalysis::ModelBuilder m_builder;
        FrontendType                            m_frontend;
        size_t                                  m_n_builder_calls_fed;
        uint64_t                                m_n_calls_fed;
    };
}

#endif /* TESTS_FRONTEND_HARNESS_H */
