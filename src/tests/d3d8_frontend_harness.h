/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Runs the calls of a D3D8 call builder (or of a workload file) through the D3D8 front end and a model builder, for the
 * tests.
 */
#ifndef TESTS_D3D8_FRONTEND_HARNESS_H
#define TESTS_D3D8_FRONTEND_HARNESS_H

#include "analysis/api_dump_analysis/model_builder.h"
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_frontend.h"
#include "d3d8_call_builder.h"
#include <stddef.h>

namespace Tests
{
    class D3D8FrontendHarness
    {
    public:
        D3D8FrontendHarness();

        /* Runs the calls that were added to the builder since the last time. */
        void feed(const D3D8CallBuilder& in_builder);

        /* Runs all the calls of a workload file. Returns false if it cannot be read. */
        bool feed_file(const char* in_filename_ptr);

        Analysis::APIDumpAnalysis::ModelBuilder&            get_builder ();
        Analysis::APIDumpAnalysis::Frontends::D3D8Frontend& get_frontend();
        const Analysis::APIDumpAnalysis::Model&             get_model   () const;

    private:
        Analysis::APIDumpAnalysis::ModelBuilder            m_builder;
        Analysis::APIDumpAnalysis::Frontends::D3D8Frontend m_frontend;
        size_t                                             m_n_builder_calls_fed;
        uint64_t                                           m_n_calls_fed;
    };
}

#endif /* TESTS_D3D8_FRONTEND_HARNESS_H */
