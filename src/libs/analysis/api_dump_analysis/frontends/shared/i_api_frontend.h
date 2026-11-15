/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_I_API_FRONTEND_H
#define APIDUMPANALYSIS_FRONTENDS_I_API_FRONTEND_H

#include "analysis/api_dump_analysis/types.h"
#include "capture/api_dump_loader/api_dump_loader.h"
#include <map>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            /* Follows the calls of one API (see Capture::APIDumpLoader::APIGroup), keeps the state of that API, and tells the model builder
             * what the calls amount to.
             */
            class IAPIFrontend
            {
            public:
                virtual ~IAPIFrontend() = default;

                /* The resource (or the part of a resource) that an application object stands for, looked up by the address of the object. Returns
                 * false if the front end does not follow the object or the object no longer exists. The address is valid from the call that
                 * creates the object.
                 */
                virtual bool find_resource_view(const uint64_t&                          in_address,
                                                Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const = 0;

                /* The calls that depend on the size of the render target in pixels, counted by kind (see ResolutionHazard). */
                virtual const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& get_n_resolution_hazards() const = 0;

                /* The calls with an unknown object or with arguments that could not be read. */
                virtual uint64_t get_n_unfollowed_calls() const = 0;

                /* The calls that the front end does not follow. Passes and draws that depend on them are wrong. */
                virtual uint64_t get_n_unsupported_calls() const = 0;

                /* Handles one call of the API. The calls arrive in the order they were made. The model builder has already been told which call
                 * this is (begin_call). The workload holds the data chunks of the call.
                 */
                virtual void on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     const Capture::APIDumpLoader::Workload&      in_workload) = 0;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_I_API_FRONTEND_H */
