/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Builds the model of a workload from what the front ends of the APIs tell it. A front end follows the state of its API
 * and reports what happens with everything resolved: a draw comes with the resources it uses, not with the calls that
 * bound them. Nothing here knows about any API.
 *
 * Passes: the builder starts a pass with the first draw or clear after the previous pass ended. A pass ends when what is
 * rendered to changes, when a clear or a viewport change comes after a draw, at a copy or an upload that touches what the
 * pass renders to (the pass must be done for it to see the result), and at the end of the frame. A buffer written to between
 * two draws does not end a pass. So rendering to the same target in two scenes, one after another, is two passes.
 */
#ifndef APIDUMPANALYSIS_MODEL_BUILDER_H
#define APIDUMPANALYSIS_MODEL_BUILDER_H

#include "analysis/api_dump_analysis/i_access_sink.h"
#include "analysis/api_dump_analysis/model.h"
#include "analysis/api_dump_analysis/resource_access_tracker.h"
#include "analysis/api_dump_analysis/resource_key_table.h"
#include "analysis/api_dump_analysis/depth_histogram.h"
#include "analysis/api_dump_analysis/types.h"
#include <array>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        class ModelBuilder
        {
        public:
            /* Without history, no frame records, frame details or camera series are kept: memory grows only with the kinds of
             * frames, passes and draws, and with the resources created. For use while a workload is played.
             */
            explicit ModelBuilder(const bool& in_keeps_history = true);

            /* Said before the events of each call of the workload, so that they can be traced back to it. */
            void begin_call(const uint64_t& in_call_index,
                            const uint64_t& in_timestamp_qpc);

            /* The frame completed by the last on_frame_presented(); empty before the first. */
            const CompletedFrame& get_completed_frame() const;

            /* The signature (see KeyBuilder::compute_draw_signature_key) of the last draw; 0 before the first. */
            /* The key of the last draw by its signature and size (see KeyBuilder::compute_draw_cover_key()). */
            uint64_t get_last_draw_cover_key() const;

            uint64_t get_last_draw_signature_key() const;

            const Model& get_model() const;

            /* The number of frames completed so far, kept or not. */
            uint64_t get_n_frames() const;

            /* The viewport the application has set, as the next draw is made with it. */
            const Viewport& get_viewport() const;

            void       on_clear           (const ClearOperation&      in_clear);
            void       on_copy            (const ResourceView&        in_source,
                                           const ResourceView&        in_destination);
            ResourceId on_create_resource (const ResourceDescription& in_description,
                                           const std::string&         in_debug_name,
                                           const ResourceView&        in_parent_view = ResourceView());
            void       on_destroy_resource(const ResourceId&          in_id);
            void       on_draw            (const DrawDescription&     in_draw);

            /* Ends the frame (a swapchain present). */
            void on_frame_presented();

            /* What is rendered to from now on. After a draw it starts a new pass even if nothing changed: call it on a change. */
            void on_set_attachments(const AttachmentSet& in_attachments);

            void on_set_viewport(const Viewport& in_viewport);

            /* Constants with no meaning known are of no interest, and are ignored. */
            void on_update_constant(const ConstantUpdate& in_update);

            void on_upload(const ResourceView&  in_destination,
                           const DataReference& in_data);

            /* Where the uses of resources are told (see IAccessSink); nullptr, the default, is nowhere. The sink is not the builder's: it must
             * outlive it.
             */
            void set_access_sink(IAccessSink* in_sink_ptr);

            /* Hands the model over, instead of a copy of it: for when the analysis is done. The builder is to be left alone after it. */
            Model take_model();

            /* True if a draw made now is to come with a sample of its geometry (see DrawDescription): a front end that has to work to
             * tell the vertices asks first.
             */
            bool wants_geometry() const;


            /* The match key of a frame shape of the model: made of the keys of its passes, as the shape key of a completed frame is. */
            static uint64_t compute_shape_key(const Model&    in_model,
                                              const uint32_t& in_shape_id);

            /* The pass of a completed frame that a call belongs to: the first whose last draw or clear is at or after the call
             * (state applies to the draws that follow it). False for the calls after the last pass, such as the present. The call
             * index is as given to begin_call(), the pass index a position in pass_instance_vec.
             */
            static bool find_pass_index(const CompletedFrame& in_frame,
                                        const uint64_t&       in_call_index,
                                        uint32_t*             out_pass_index_ptr);

        private:
            typedef std::tuple<ConstantBlockKind, uint32_t, uint64_t, uint64_t> ConstantStateKey;

            struct ConstantState
            {
                uint64_t       block_key = 0;
                ConstantUpdate update;
            };

            /* The matrices a draw is made with, as the constants say. */
            struct DrawMatrices
            {
                bool                  has_projection   = false;
                std::array<float, 16> model_f32mat     = {1.0f, 0.0f, 0.0f, 0.0f,
                                                          0.0f, 1.0f, 0.0f, 0.0f,
                                                          0.0f, 0.0f, 1.0f, 0.0f,
                                                          0.0f, 0.0f, 0.0f, 1.0f};
                std::array<float, 16> projection_f32mat = {};
                std::array<float, 16> view_f32mat       = {};
            };

            /* A pass until it ends. */
            struct OpenPass
            {
                AttachmentSet                attachments;
                uint64_t                     begin_call_index     = 0;
                uint64_t                     begin_timestamp_qpc  = 0;
                bool                         clears_color         = false;
                bool                         clears_depth         = false;
                bool                         clears_stencil       = false;
                std::set<uint64_t>           covering_draw_key_set;        /* keys of the draws of the pass that cover the view */
                DepthHistogram               depth_histogram;
                std::map<uint64_t, uint64_t> draw_signature_map;           /* key of the draw structure -> key of its signature */
                bool                         has_draws            = false;
                uint64_t                     last_draw_call_index = 0;     /* last draw, or the clear if there is none */
                uint64_t                     n_draws              = 0;
                uint64_t                     n_vertices           = 0;
                DepthHistogram               surface_depth_histogram;
                Viewport                     viewport;
            };

            void close_pass               ();
            void close_pass_if_rendered_to(const ResourceView& in_view);
            void complete_pass            ();

            /* Of the draw in the open pass, as KeyBuilder::compute_draw_signature_key() says. */
            uint64_t compute_draw_signature_key(const DrawDescription& in_draw);

            /* Tells the sink, if there is one, of a use of a resource at this call. A resource which is NONE is no use. */
            void emit_access(const ResourceId&       in_resource_id,
                             const AccessRole&       in_role,
                             const AccessMode&       in_mode,
                             const AccessCoverage&   in_coverage,
                             const SubresourceRange& in_subresource_range);

            /* With the keys the resources have by the time of the draw, or with the ones they would have if no pass had written to them. */
            void fill_draw_structure(const DrawDescription& in_draw,
                                     const bool&            in_uses_unwritten_keys,
                                     DrawStructure*         out_structure_ptr) const;
            /* The matrices of the draw, if its camera has depth to converge at (a view, and a perspective projection). */
            bool find_draw_matrices(DrawMatrices* out_matrices_ptr) const;

            /* Adds what the histograms of a frame hold to those of the model, for the shape of the frame. */
            static void merge_depth_histograms(const uint32_t&                              in_shape_id,
                                               const std::vector<DepthHistogram>&           in_histogram_vec,
                                               const std::vector<PassInstance>&             in_pass_instance_vec,
                                               std::map<DepthHistogramKey, DepthHistogram>* inout_map_ptr);

            /* How much of the view a clear reaches: all of it, or what the viewport and the rectangles of the clear leave of it. */
            AccessCoverage get_clear_coverage(const ClearOperation& in_clear,
                                              const ResourceView&   in_view) const;

            /* The part of a resource that a view is. */
            static SubresourceRange get_subresource_range(const ResourceView& in_view);

            void open_pass_if_needed();
            void record_draw_access (const DrawDescription& in_draw);

            /* Counts the draw at the depth of the origin of its object, as the camera of the pass sees it. */
            void record_draw_depth(const DrawDescription& in_draw);

            /* Counts the draw by the depth of the vertices of its sample (see DrawDescription::geometry_ptr), those in front of the camera
             * and in view, each for its share of the vertices of the draw.
             */
            /* What the vertices of the draw, as far as they were looked at, say of the view: they reach all four edges, or they do not, or
             * there was nothing to look at.
             */
            ViewCoverage record_draw_surface_depth(const DrawDescription& in_draw);

            /* The same, for a draw whose vertices are already in the space of the picture (see GeometrySample::is_pre_transformed): the depth of a
             * vertex is the reciprocal of what it says, it is in view when it is in the viewport, and the draw reaches the edges of the view when its
             * vertices reach those of the viewport. No matrix is asked.
             */
            ViewCoverage record_screen_depth(const DrawDescription& in_draw);


            /* True for the constants that describe the camera, which is the same for all the draws of a pass. */
            static bool is_camera_semantic(const ConstantSemantic& in_semantic);


            IAccessSink*                              m_access_sink_ptr;
            ResourceAccessTracker                     m_access_tracker;
            AttachmentSet                             m_attachments;
            uint64_t                                  m_call_index;
            CompletedFrame                            m_completed_frame;
            std::map<ConstantStateKey, ConstantState> m_constant_state_map;
            DrawStructure                             m_draw_signature_scratch; /* for on_draw(), so that it need not allocate each time */
            DrawStructure                             m_draw_structure_scratch;
            uint64_t                                  m_frame_begin_timestamp_qpc;
            std::vector<DepthHistogram>               m_frame_depth_histogram_vec; /* of the passes above, in the same order */
            uint64_t                                  m_frame_first_call_index;
            std::vector<DrawInstance>                 m_frame_draw_instance_vec;
            std::vector<PassInstance>                 m_frame_pass_instance_vec;
            std::vector<bool>                         m_frame_pass_is_presented_vec; /* of the passes above, in the same order */
            std::vector<DepthHistogram>               m_frame_surface_depth_histogram_vec; /* of the passes above, in the same order */
            std::vector<TransferRecord>               m_frame_transfer_record_vec;
            bool                                      m_has_frame_calls;
            bool                                      m_has_open_pass;
            bool                                      m_keeps_history;
            ResourceKeyTable                          m_key_table;
            uint64_t                                  m_last_draw_cover_key;
            uint64_t                                  m_last_draw_signature_key;
            Model                                     m_model;
            uint64_t                                  m_n_frames;
            OpenPass                                  m_open_pass;
            std::vector<ResourceId>                   m_pending_forget_vec;
            uint64_t                                  m_timestamp_qpc;
            Viewport                                  m_viewport;
        };
    }
}

#endif /* APIDUMPANALYSIS_MODEL_BUILDER_H */
