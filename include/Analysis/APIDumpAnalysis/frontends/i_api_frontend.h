/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_I_API_FRONTEND_H
#define APIDUMPANALYSIS_FRONTENDS_I_API_FRONTEND_H

#include "Capture/APIDumpLoader/APIDumpLoader.h"

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
