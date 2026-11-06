/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Runs the calls of a D3D7 call builder through the D3D7 front end and a model builder, for the tests.
 */
#ifndef TESTS_D3D7_FRONTEND_HARNESS_H
#define TESTS_D3D7_FRONTEND_HARNESS_H

#include "analysis/api_dump_analysis/frontends/d3d7_frontend.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "d3d7_call_builder.h"
#include <stddef.h>
#include <stdint.h>

namespace Tests
{
    class D3D7FrontendHarness
    {
    public:
        D3D7FrontendHarness();

        /* Runs the calls that were added to the builder since the last time. */
        void feed(const D3D7CallBuilder& in_builder);

        Analysis::APIDumpAnalysis::ModelBuilder&            get_builder ();
        Analysis::APIDumpAnalysis::Frontends::D3D7Frontend& get_frontend();
        const Analysis::APIDumpAnalysis::Model&             get_model   () const;

    private:
        Analysis::APIDumpAnalysis::ModelBuilder            m_builder;
        Analysis::APIDumpAnalysis::Frontends::D3D7Frontend m_frontend;
        size_t                                             m_n_builder_calls_fed;
        uint64_t                                           m_n_calls_fed;
    };
}

#endif /* TESTS_D3D7_FRONTEND_HARNESS_H */
