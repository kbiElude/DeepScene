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
            /* Follows the calls of one API (see Capture::APIDumpLoader::APIGroup), keeps the state that the API has, and tells the model
             * builder what the calls come to.
             */
            class IAPIFrontend
            {
            public:
                virtual ~IAPIFrontend() = default;

                /* The resource (or the part of one) that an object of the application stands for, by its address; false if the object is not one of those
                 * that the front end follows, or is gone. Valid from the call that makes the object on.
                 */
                virtual bool find_resource_view(const uint64_t&                          in_address,
                                                Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const = 0;

                /* The calls which depend on the size of what is rendered to in pixels, by what they are (see ResolutionHazard). */
                virtual const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& get_n_resolution_hazards() const = 0;

                /* Calls with an unknown object, or arguments which could not be read. */
                virtual uint64_t get_n_unfollowed_calls() const = 0;

                /* Calls of what the front end does not follow: passes and draws made with them are not right. */
                virtual uint64_t get_n_unsupported_calls() const = 0;

                /* A call of the API, in the order they were made. The model builder has been told which call it is (begin_call)
                 * by now. The workload holds the data chunks of the call.
                 */
                virtual void on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                     const Capture::APIDumpLoader::Workload&      in_workload) = 0;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_I_API_FRONTEND_H */
