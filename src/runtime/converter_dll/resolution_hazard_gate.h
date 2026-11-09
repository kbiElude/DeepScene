/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_RESOLUTION_HAZARD_GATE_H
#define STEREO_CONVERTERDLL_RESOLUTION_HAZARD_GATE_H

#include "analysis/api_dump_analysis/types.h"
#include "stereo/core/resolution_policy.h"
#include "stereo/core/stereo_core.h"

namespace Stereo
{
    namespace ConverterDLL
    {
        /* What the adapters share in keeping an application from being shown wrong when it renders larger than it asks: what each kind of call that depends on the size of
         * what it renders to (see ResolutionHazard) is told to the person as, and the end of the application at the first one that the adapter cannot make match the size.
         */
        class ResolutionHazardGate
        {
        public:
            /* What the application does, as it is told to the person it is stopped for. */
            static const char* get_reason(const Analysis::APIDumpAnalysis::ResolutionHazard& in_hazard);

            /* Ends the application, through the core, with the reason, if it is rendered larger than it asks (the policy is not the identity): the hazard is a call that the
             * adapter does not make match the size. Nothing is done at the scale of 1, where nothing depends on it.
             */
            static void require_unaffected_by_scale(Stereo::Core::StereoCore*                              in_core_ptr,
                                                    const Stereo::Core::ResolutionPolicy&                  in_policy,
                                                    const Analysis::APIDumpAnalysis::ResolutionHazard&     in_hazard);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_RESOLUTION_HAZARD_GATE_H */
