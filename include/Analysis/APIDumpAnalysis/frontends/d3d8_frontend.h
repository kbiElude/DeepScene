/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * The front end of Direct3D 8: follows the objects, the state of the device and the draws, and reports them to the model
 * builder.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_D3D8_FRONTEND_H
#define APIDUMPANALYSIS_FRONTENDS_D3D8_FRONTEND_H

#include "Analysis/APIDumpAnalysis/frontends/call_view.h"
#include "Analysis/APIDumpAnalysis/frontends/d3d8_geometry_tracker.h"
#include "Analysis/APIDumpAnalysis/frontends/object_namer.h"
#include "Analysis/APIDumpAnalysis/frontends/object_tracker.h"
#include "Analysis/APIDumpAnalysis/frontends/d3d8_pipeline_state.h"
#include "Analysis/APIDumpAnalysis/frontends/i_api_frontend.h"
#include "Analysis/APIDumpAnalysis/model_builder.h"
#include "Interception/Core/D3D8Utils/types.h"
#include <map>
#include <set>
#include <unordered_set>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class D3D8Frontend : public IAPIFrontend
            {
            public:
                /* The builder must outlive the front end. */
                D3D8Frontend(Analysis::APIDumpAnalysis::ModelBuilder* in_builder_ptr);

                /* The calls which depend on the size of what is rendered to in pixels, by what they are (see ResolutionHazard). */
                const std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t>& get_n_resolution_hazards() const override;

                /* Calls with an unknown object, or arguments which could not be read. */
                uint64_t get_n_unfollowed_calls() const override;

                /* Calls of what the front end does not follow (state blocks, patches, additional swap chains...): passes and draws
                 * made with them are not right.
                 */
                uint64_t get_n_unsupported_calls() const override;

                /* What the front end knows about the objects (for the checks of the tests). */
                const ObjectTracker& get_tracker() const;

                void on_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                             const Capture::APIDumpLoader::Workload&      in_workload) override;

            private:
                /* Tells what a Create* method makes. */
                typedef Analysis::APIDumpAnalysis::ResourceDescription (*DescribeFuncPtr)(const CallView& in_view);

                /* A vertex shader that was made: the resource, and what its declaration says the vertices are like. */
                struct VertexShader
                {
                    uint64_t                              declaration_hash = 0;
                    Analysis::APIDumpAnalysis::ResourceId id               = Analysis::APIDumpAnalysis::ResourceId::NONE;
                };


                /* Makes the back buffer and the depth buffer of a device, which renders to them to begin with. */
                void create_swap_chain_resources(const Interception::Core::D3D8Utils::PresentParameters& in_parameters);

                /* The whole of a surface, as a viewport. */
                Analysis::APIDumpAnalysis::Viewport get_full_viewport(const Analysis::APIDumpAnalysis::ResourceView& in_view) const;

                void handle_clear                    (const CallView&        in_view);
                void handle_copy_rects               (const CallView&        in_view);
                void handle_create_device            (const CallView&        in_view);
                void handle_create_pixel_shader      (const CallView&        in_view);
                void handle_create_resource          (const CallView&        in_view,
                                                      const DescribeFuncPtr& in_describe_func_ptr);
                void handle_create_vertex_shader     (const CallView&        in_view);
                void handle_delete_pixel_shader      (const CallView&        in_view);
                void handle_delete_vertex_shader     (const CallView&        in_view);
                void handle_draw                     (const CallView&        in_view,
                                                      const bool&            in_is_indexed);
                void handle_draw_user_pointer        (const CallView&        in_view,
                                                      const bool&            in_is_indexed);
                void handle_get_back_buffer          (const CallView&        in_view);
                void handle_get_cube_map_surface     (const CallView&        in_view);
                void handle_get_depth_stencil_surface(const CallView&        in_view);
                void handle_get_render_target        (const CallView&        in_view);
                void handle_get_surface_level        (const CallView&        in_view);
                void handle_light_enable             (const CallView&        in_view);
                void handle_lock                     (const CallView&        in_view);
                void handle_release                  (const CallView&        in_view);
                void handle_reset                    (const CallView&        in_view);
                void handle_set_indices              (const CallView&        in_view);
                void handle_set_light                (const CallView&        in_view);
                void handle_set_pixel_shader         (const CallView&        in_view);
                void handle_set_render_state         (const CallView&        in_view);
                void handle_set_render_target        (const CallView&        in_view);
                void handle_set_stream_source        (const CallView&        in_view);
                void handle_set_texture              (const CallView&        in_view);
                void handle_set_texture_stage_state  (const CallView&        in_view);
                void handle_set_transform            (const CallView&        in_view);
                void handle_set_vertex_shader        (const CallView&        in_view);
                void handle_set_viewport             (const CallView&        in_view);
                void handle_unlock                   (const CallView&        in_view,
                                                      const size_t&          in_n_data_argument,
                                                      const uint32_t&        in_layer,
                                                      const uint32_t&        in_mip_level);
                void handle_update_texture           (const CallView&        in_view);

                /* Tells the geometry tracker about the buffer that a Create* method made. */
                void register_buffer(const CallView&                              in_view,
                                     const Analysis::APIDumpAnalysis::ResourceId& in_id);

                /* Names the objects that the call has created or returned, like the printer does. */
                void register_names(const CallView& in_view);


                /* Some of the vertices of a draw, if the front end can tell them; null if not. */
                std::shared_ptr<const Analysis::APIDumpAnalysis::GeometrySample> sample_geometry(const CallView& in_view,
                                                                                                 const bool&     in_is_indexed,
                                                                                                 const uint32_t& in_vertex_count);


                /* nullptr for the methods that do not create a resource. */
                static DescribeFuncPtr get_describe_func_ptr(const APIInterceptor::APIFunction& in_function);

                static Analysis::APIDumpAnalysis::PrimitiveTopology get_primitive_topology(const Interception::Hooks::D3D8::D3DPRIMITIVETYPE& in_type);
                static Analysis::APIDumpAnalysis::ConstantSemantic  get_transform_semantic(const uint32_t&                                    in_state);
                static uint32_t                                     get_vertex_count      (const Interception::Hooks::D3D8::D3DPRIMITIVETYPE& in_type,
                                                                                           const uint32_t&                                    in_n_primitives);

                /* True for the methods which set the state of the device. */
                static bool is_state_function(const APIInterceptor::APIFunction& in_function);

                /* True for the methods whose effects the front end does not follow. */
                static bool is_unsupported_function(const APIInterceptor::APIFunction& in_function);


                Analysis::APIDumpAnalysis::ResourceId                           m_auto_depth_stencil_id; /* the one the device made; NONE if none */
                Analysis::APIDumpAnalysis::ResourceView                         m_back_buffer_view;
                uint32_t                                                        m_base_vertex_index;  /* of the indices which are set     */
                Analysis::APIDumpAnalysis::ModelBuilder*                        m_builder_ptr;
                Analysis::APIDumpAnalysis::ResourceView                         m_color_view;         /* what is rendered to                 */
                Analysis::APIDumpAnalysis::ResourceView                         m_depth_stencil_view; /* resource_id is NONE if there is none */
                D3D8GeometryTracker                                             m_geometry_tracker;
                std::map<Analysis::APIDumpAnalysis::ResolutionHazard, uint64_t> m_n_resolution_hazard_map;
                uint64_t                                                        m_n_unfollowed_calls;
                uint64_t                                                        m_n_unsupported_calls;
                ObjectNamer                                                 m_namer;
                D3D8PipelineState                                               m_pipeline_state;
                std::map<uint32_t, Analysis::APIDumpAnalysis::ResourceId>       m_pixel_shader_map;   /* by handle */
                std::unordered_set<APIInterceptor::APIFunction>                 m_release_function_set;
                ObjectTracker                                               m_tracker;
                Analysis::APIDumpAnalysis::ConstantUpdate                       m_transform_update_scratch;
                std::map<uint32_t, VertexShader>                                m_vertex_shader_map;  /* by handle */
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_D3D8_FRONTEND_H */
