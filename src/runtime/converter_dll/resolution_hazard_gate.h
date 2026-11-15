/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_RESOLUTION_HAZARD_GATE_H
#define STEREO_CONVERTERDLL_RESOLUTION_HAZARD_GATE_H

#include "analysis/api_dump_analysis/types.h"
#include "stereo/core/resolution_policy.h"
#include "stereo/core/recipe_stereo_planner.h"

namespace Stereo
{
    namespace ConverterDLL
    {
        /* The code that the API adapters share to stop an application that would be shown wrongly when it renders larger than it asks. It turns each
         * kind of call that depends on the size of its render target (see ResolutionHazard) into a reason for the user, and it ends the application at the
         * first such call that the adapter cannot make match the size.
         */
        class ResolutionHazardGate
        {
        public:
            /* Returns the text that says what the application does. It is the first part of the reason shown to the user when the application is stopped. */
            static const char* get_reason(const Analysis::APIDumpAnalysis::ResolutionHazard& in_hazard);

            /* Ends the application through the core, with the reason for in_hazard, if the application renders larger than it asks (the policy is not the
             * identity). The hazard is a call that the adapter cannot make match the size. Does nothing at scale 1, where nothing depends on the size.
             */
            static void require_unaffected_by_scale(Stereo::Core::RecipeStereoPlanner*                     in_core_ptr,
                                                    const Stereo::Core::ResolutionPolicy&                  in_policy,
                                                    const Analysis::APIDumpAnalysis::ResolutionHazard&     in_hazard);
        };
    }
}

#endif /* STEREO_CONVERTERDLL_RESOLUTION_HAZARD_GATE_H */
