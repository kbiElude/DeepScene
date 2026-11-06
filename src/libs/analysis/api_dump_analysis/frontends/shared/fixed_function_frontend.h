/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the front ends of the APIs that have a fixed-function pipeline (Direct3D 7 and 8) do alike: they follow the objects the application
 * makes (see ObjectTracker), and the state of the device that does not depend on the API (transforms, lights, render states, textures), and
 * count the calls they cannot follow. What the APIs do not do alike (the resources, the draws, the vertices) is the front end's own.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_FRONTEND_H
#define APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_FRONTEND_H

#include "analysis/api_dump_analysis/frontends/shared/call_view.h"
#include "analysis/api_dump_analysis/frontends/shared/fixed_function_state.h"
#include "analysis/api_dump_analysis/frontends/shared/i_api_frontend.h"
#include "Capture/APIDumpLoader/object_namer.h"
#include "analysis/api_dump_analysis/frontends/shared/object_tracker.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include <map>
#include <stdint.h>
#include <unordered_set>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class FixedFunctionFrontend : public IAPIFrontend
            {
            public:
                /* The builder must outlive the front end. */
                explicit FixedFunctionFrontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                bool find_resource_view(const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const override;

                const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& get_n_resolution_hazards() const override;

                uint64_t get_n_unfollowed_calls() const override;

                uint64_t get_n_unsupported_calls() const override;

                /* What the front end knows about the objects (for the checks of the tests). */
                const ObjectTracker& get_tracker() const;

            protected:
                /* nullptr if there is no such resource. */
                const Analysis::APIDumpAnalysis::ResourceDescription* find_resource_description_ptr(const Analysis::APIDumpAnalysis::ResourceId& in_id) const;

                /* Lets go of what is kept of a resource that is gone (the vertices that were seen of it, say). */
                virtual void forget_resource(const Analysis::APIDumpAnalysis::ResourceId& in_id) = 0;

                /* The whole of a surface, as a viewport. */
                Analysis::APIDumpAnalysis::Viewport get_full_viewport(const Analysis::APIDumpAnalysis::ResourceView& in_view) const;

                /* The state of the fixed-function pipeline of the device, which the API says the rules of. */
                virtual FixedFunctionState& get_fixed_function_state() = 0;

                /* What a transform state is for; UNKNOWN for the states the front end does not follow. */
                virtual Analysis::APIDumpAnalysis::ConstantSemantic get_transform_semantic(const uint32_t& in_state) const = 0;

                void handle_light_enable           (const CallView& in_view);
                void handle_release                (const CallView& in_view);
                void handle_set_light              (const CallView& in_view);
                void handle_set_render_state       (const CallView& in_view);
                void handle_set_texture            (const CallView& in_view);
                void handle_set_texture_stage_state(const CallView& in_view);
                void handle_set_transform          (const CallView& in_view);


                Analysis::APIDumpAnalysis::ModelBuilder*                        m_builder_ptr;
                Analysis::APIDumpAnalysis::ResourceView                         m_color_view;         /* what is rendered to                  */
                Analysis::APIDumpAnalysis::ResourceView                         m_depth_stencil_view; /* resource_id is NONE if there is none */
                Capture::APIDumpLoader::ObjectNamer                                 m_namer;
                std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t> m_n_resolution_hazard_map;
                uint64_t                                                        m_n_unfollowed_calls;
                uint64_t                                                        m_n_unsupported_calls;
                std::unordered_set<APIInterceptor::APIFunction>                 m_release_function_set; /* the methods which Release, for the front end to fill */
                ObjectTracker                                                   m_tracker;
                Analysis::APIDumpAnalysis::ConstantUpdate                       m_transform_update_scratch;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_FRONTEND_H */
