/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The front end of DirectDraw 7 and Direct3D 7: follows the surfaces and the vertex buffers, the state of the device and the draws,
 * and reports them to the model builder.
 *
 * Surfaces are resources. The primary surface and the back buffers that it flips with are swap chain images; a mip level of a texture
 * is a part of the texture, as the surface that GetAttachedSurface gives for it. A depth buffer is the surface that was attached to
 * what is rendered to. A frame ends with a flip, or with the blit to the primary surface that does the work of one.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D7_FRONTEND_H
#define APIDUMPANALYSIS_FRONTENDS_D3D7_FRONTEND_H

#include "Analysis/APIDumpAnalysis/frontends/call_view.h"
#include "Analysis/APIDumpAnalysis/frontends/d3d7_geometry_tracker.h"
#include "Analysis/APIDumpAnalysis/frontends/d3d7_pipeline_state.h"
#include "Analysis/APIDumpAnalysis/frontends/fixed_function_frontend.h"
#include "Analysis/APIDumpAnalysis/frontends/i_api_frontend.h"
#include "Analysis/APIDumpAnalysis/frontends/object_namer.h"
#include "Analysis/APIDumpAnalysis/frontends/object_tracker.h"
#include "Analysis/APIDumpAnalysis/model_builder.h"
#include "Interception/Hooks/D3D7/types.h"
#include <map>
#include <stddef.h>
#include <stdint.h>
#include <unordered_set>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class D3D7Frontend : public FixedFunctionFrontend
            {
            public:
                /* The builder must outlive the front end. */
                D3D7Frontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);






                void on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                             const Capture::APIDumpLoader::Workload&      in_workload) override;

            private:
                /* What the base class needs of the front end. */
                void                                                forget_resource       (const Analysis::APIDumpAnalysis::ResourceId& in_id) override;
                FixedFunctionState&                                 get_fixed_function_state() override;
                Analysis::APIDumpAnalysis::ConstantSemantic         get_transform_semantic(const uint32_t&                              in_state) const override;

                /* Where the arguments of a draw are. */
                struct DrawLayout
                {
                    size_t                      count_argument;         /* the vertices, or the indices if the draw is indexed */
                    size_t                      fvf_argument;
                    APIInterceptor::APIFunction function;
                    bool                        is_indexed;
                    size_t                      strided_argument;       /* the structure that tells the strides */
                    size_t                      vertex_buffer_argument;
                };

                /* Renders to the surface from now on, with the depth buffer that is attached to it. */
                void apply_render_target(const Analysis::APIDumpAnalysis::ResourceView& in_target);

                void count_resolution_hazards(const CallView& in_view);



                void handle_add_attached_surface   (const CallView&   in_view);
                void handle_clear                  (const CallView&   in_view);
                void handle_create_device          (const CallView&   in_view);
                void handle_create_surface         (const CallView&   in_view);
                void handle_create_vertex_buffer   (const CallView&   in_view);
                void handle_draw                   (const CallView&   in_view,
                                                    const DrawLayout& in_layout);
                void handle_duplicate_surface      (const CallView&   in_view);
                void handle_get_attached_surface   (const CallView&   in_view);
                void handle_set_display_mode       (const CallView&   in_view);
                void handle_set_render_target      (const CallView&   in_view);
                void handle_set_viewport           (const CallView&   in_view);
                void handle_unlock                 (const CallView&   in_view);

                /* A copy from a surface to another, or a fill of a surface if there is no source: Blt, BltFast and Load. */
                void handle_write_to_surface(const CallView& in_view,
                                             const size_t&   in_destination_argument,
                                             const size_t&   in_source_argument);

                /* Names the objects that the call has created or returned, like the printer does. */
                void register_names(const CallView& in_view);


                /* The type of the object that a call makes, by the interface it asks for if it asks for one. */
                static const char* get_created_object_type_name_ptr(const CallView&                                in_view,
                                                                    const Interception::Hooks::D3D7::ArgumentInfo& in_info);

                /* Some of the vertices of a draw, if the front end can tell them; null if not. */
                std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> sample_geometry(const CallView&                              in_view,
                                                                                                 const DrawLayout&                            in_layout,
                                                                                                 const uint32_t&                              in_fvf,
                                                                                                 const Analysis::APIDumpAnalysis::ResourceId& in_vertex_buffer_id);


                /* nullptr for the functions that are not draws. */
                static const DrawLayout* find_draw_layout_ptr(const APIInterceptor::APIFunction& in_function);

                static Analysis::APIDumpAnalysis::PrimitiveTopology get_primitive_topology(const uint32_t& in_type);

                /* True for the methods whose effects the front end does not follow. */
                static bool is_unsupported_function(const APIInterceptor::APIFunction& in_function);


                std::map<uint32_t, Analysis::APIDumpAnalysis::ResourceView>     m_attached_depth_map; /* by the resource of the surface they are attached to */
                D3D7GeometryTracker                                             m_geometry_tracker;
                D3D7PipelineState                                               m_pipeline_state;
                Analysis::APIDumpAnalysis::ResourceDescription                  m_primary_description;
                uint32_t                                                        m_screen_height;
                uint32_t                                                        m_screen_width;
                std::map<uint32_t, uint32_t>                                    m_vertex_buffer_fvf_map; /* by the resource of the vertex buffer */
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D7_FRONTEND_H */
