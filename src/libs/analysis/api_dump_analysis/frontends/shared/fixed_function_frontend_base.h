/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The common part of the front ends of the APIs that have a fixed-function pipeline (Direct3D 7 and 8). It follows the objects that the
 * application creates (see ObjectTracker) and the device state that does not depend on the API (transforms, lights, render states, textures),
 * and counts the calls that it cannot follow. What differs between the APIs (the resources, the draws, the vertices) is left to the front end.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_FRONTEND_BASE_H
#define APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_FRONTEND_BASE_H

#include "analysis/api_dump_analysis/frontends/shared/call_view.h"
#include "analysis/api_dump_analysis/frontends/shared/fixed_function_state.h"
#include "analysis/api_dump_analysis/frontends/shared/i_api_frontend.h"
#include "capture/api_dump_loader/object_namer.h"
#include "analysis/api_dump_analysis/frontends/shared/object_tracker.h"
#include "analysis/api_dump_analysis/frontends/shared/resolution_hazard_counter.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "analysis/api_dump_analysis/render_target_tracker.h"
#include <map>
#include <stdint.h>
#include <unordered_set>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class FixedFunctionFrontendBase : public IAPIFrontend
            {
            public:
                /* The builder must outlive the front end. */
                explicit FixedFunctionFrontendBase(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                bool find_resource_view(const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const override;

                const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& get_n_resolution_hazards() const override;

                uint64_t get_n_unfollowed_calls() const override;

                uint64_t get_n_unsupported_calls() const override;

                /* The objects that the front end knows about (used by the tests). */
                const ObjectTracker& get_tracker() const;

            protected:
                /* Returns nullptr if there is no such resource. */
                const Analysis::APIDumpAnalysis::ResourceDescription* find_resource_description_ptr(const Analysis::APIDumpAnalysis::ResourceId& in_id) const;

                /* Drops what is kept for a resource that no longer exists (for example, the vertices that were sampled from it). */
                virtual void forget_resource(const Analysis::APIDumpAnalysis::ResourceId& in_id) = 0;

                /* The whole surface as a viewport. */
                Analysis::APIDumpAnalysis::Viewport get_full_viewport(const Analysis::APIDumpAnalysis::ResourceView& in_view) const;

                /* The fixed-function pipeline state of the device. The API supplies the rules for it. */
                virtual FixedFunctionState& get_fixed_function_state() = 0;

                /* What a transform state is for; UNKNOWN for the states that the front end does not follow. */
                virtual Analysis::APIDumpAnalysis::ConstantSemantic get_transform_semantic(const uint32_t& in_state) const = 0;

                void handle_light_enable           (const CallView& in_view);
                void handle_release                (const CallView& in_view);
                void handle_set_light              (const CallView& in_view);
                void handle_set_render_state       (const CallView& in_view);
                void handle_set_texture            (const CallView& in_view);
                void handle_set_texture_stage_state(const CallView& in_view);
                void handle_set_transform          (const CallView& in_view);


                Analysis::APIDumpAnalysis::ModelBuilder*        m_builder_ptr;
                Capture::APIDumpLoader::ObjectNamer             m_namer;
                uint64_t                                        m_n_unfollowed_calls;
                uint64_t                                        m_n_unsupported_calls;
                std::unordered_set<APIInterceptor::APIFunction> m_release_function_set; /* the Release methods; filled by the derived front end */
                Analysis::APIDumpAnalysis::RenderTargetTracker  m_render_target_tracker; /* the color and the depth and stencil views that are rendered to */
                ResolutionHazardCounter                         m_resolution_hazard_counter;
                ObjectTracker                                   m_tracker;
                Analysis::APIDumpAnalysis::ConstantUpdate       m_transform_update_scratch;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_FIXED_FUNCTION_FRONTEND_BASE_H */
