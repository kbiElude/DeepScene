/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Builds the model of a workload from front end events, resolved: a draw arrives with its resources, not the binding calls. API-neutral.
 * A pass starts with the first draw or clear after the previous pass ended, and ends when
 *  - the render targets change,
 *  - a clear or a viewport change comes after a draw,
 *  - a copy or upload touches what the pass renders to, or
 *  - the frame ends.
 * Writing to a buffer between draws does not end a pass.
 */
#ifndef APIDUMPANALYSIS_MODEL_BUILDER_H
#define APIDUMPANALYSIS_MODEL_BUILDER_H

#include "analysis/api_dump_analysis/draw_key_cache.h"
#include "analysis/api_dump_analysis/i_access_sink.h"
#include "analysis/api_dump_analysis/model.h"
#include "analysis/api_dump_analysis/pass_tracker.h"
#include "analysis/api_dump_analysis/resource_access_tracker.h"
#include "analysis/api_dump_analysis/resource_key_table.h"
#include "analysis/api_dump_analysis/depth_histogram.h"
#include "analysis/api_dump_analysis/types.h"
#include "analysis/api_dump_textures/i_texture_sink.h"
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
            /* Without history, no frame records, frame details or camera series are kept, so memory grows only with the kinds of frames,
             * passes and draws and the resources. Use it while a workload is played.
             */
            explicit ModelBuilder(const bool& in_keeps_history = true);

            /* Called before the events of each call of the workload, so that the events can be traced back to the call. */
            void begin_call(const uint64_t& in_call_index,
                            const uint64_t& in_timestamp_qpc);

            /* The keys of the draws the current call made, in order (a list call can make several or none). begin_call() empties it. */
            const std::vector<DrawKeys>& get_call_draw_key_vec() const;

            /* The number of the call that the current events belong to (the index that begin_call() received last). */
            uint64_t get_call_index() const;

            /* The frame that the last on_frame_presented() completed; empty before the first one. */
            const CompletedFrame& get_completed_frame() const;

            /* The key of the last draw, built from its signature and size (see MatchKeyBuilder::compute_draw_cover_key()); 0 before the first draw. */
            uint64_t get_last_draw_cover_key() const;

            /* The signature of the last draw (see MatchKeyBuilder::compute_draw_signature_key()); 0 before the first draw. */
            uint64_t get_last_draw_signature_key() const;

            const Model& get_model() const;

            /* The number of frames completed so far, whether their records are kept or not. */
            uint64_t get_n_frames() const;

            /* The viewport that the application has set; the next draw is made with it. */
            const Viewport& get_viewport() const;

            void       on_clear           (const ClearOperation&      in_clear);
            /* The rectangle is the area of the destination that the copy writes, if the front end knows it. */
            void       on_copy            (const ResourceView&        in_source,
                                           const ResourceView&        in_destination,
                                           const AccessRectangle&     in_destination_rectangle = AccessRectangle());
            ResourceId on_create_resource (const ResourceDescription& in_description,
                                           const std::string&         in_debug_name,
                                           const ResourceView&        in_parent_view = ResourceView());
            void       on_destroy_resource(const ResourceId&          in_id);
            void       on_draw            (const DrawDescription&     in_draw);

            /* Ends the frame (a swapchain present). */
            void on_frame_presented();

            /* Sets the render targets. After a draw it starts a new pass even if nothing changed, so call it only on a change. */
            void on_set_attachments(const AttachmentSet& in_attachments);

            void on_set_viewport(const Viewport& in_viewport);

            /* Reports what happens to a texture (see ITextureSink). The builder fills in the call and the frame. Does nothing if there is no sink. */
            void on_texture_event(const Analysis::APIDumpTextures::TextureEvent& in_event,
                                  const uint8_t*                                 in_data_ptr,
                                  const size_t&                                  in_n_data_bytes,
                                  const uint32_t&                                in_data_chunk_id);

            /* Constants whose meaning is unknown are of no interest and are ignored. */
            void on_update_constant(const ConstantUpdate& in_update);

            /* The coverage and the rectangle say how much of the level the data is written to (the whole level, or the rectangle), if the front end knows. */
            void on_upload(const ResourceView&     in_destination,
                           const DataReference&    in_data,
                           const AccessCoverage&   in_coverage  = AccessCoverage::UNKNOWN,
                           const AccessRectangle&  in_rectangle = AccessRectangle());

            /* Sets the sink for resource accesses (see IAccessSink); nullptr (default) means none. Not owned: it must outlive the builder. */
            void set_access_sink(IAccessSink* in_sink_ptr);

            /* On by default. Off, every draw works out its keys from scratch: a check that the cache changes nothing. */
            void set_caches_draw_keys(const bool& in_caches_draw_keys);

            /* Sets the sink for texture events (see ITextureSink); nullptr (default) means none. Not owned: it must outlive the builder. */
            void set_texture_sink(Analysis::APIDumpTextures::ITextureSink* in_sink_ptr);

            /* Moves the model out of the builder instead of copying it. Call it when the analysis is done; do not use the builder afterwards. */
            Model take_model();

            /* True if a draw should come with a geometry sample (see DrawDescription); front ends that need extra work to read vertices ask first. */
            bool wants_geometry() const;

            /* True if there is a texture sink; front ends that need extra work to produce events (upload bytes) ask first. */
            bool wants_texture_events() const;

            /* The match key of a frame shape of the model, built from the keys of its passes in the same way as the shape key of a completed frame. */
            static uint64_t compute_shape_key(const Model&    in_model,
                                              const uint32_t& in_shape_id);

            /* Finds the pass of a completed frame a call belongs to: the first pass whose last draw or clear is at or after the call.
             * False for calls after the last pass (the present). The call index is the begin_call() one; the pass index is into pass_instance_vec.
             */
            static bool find_pass_index(const CompletedFrame& in_frame,
                                        const uint64_t&       in_call_index,
                                        uint32_t*             out_pass_index_ptr);

        private:
            /* Completes the open pass, if there is one. */
            void close_pass();

            void close_pass_if_rendered_to(const ResourceView& in_view);

            /* The signature key of the draw in the open pass, as MatchKeyBuilder::compute_draw_signature_key() defines it. */
            uint64_t compute_draw_signature_key(const DrawDescription& in_draw,
                                                const bool&            in_is_orthographic);

            /* Tells the sink, if there is one, about an access to a resource at the current call. A resource that is NONE is not an access. */
            void emit_access(const ResourceId&       in_resource_id,
                             const AccessRole&       in_role,
                             const AccessMode&       in_mode,
                             const AccessCoverage&   in_coverage,
                             const SubresourceRange& in_subresource_range,
                             const AccessRectangle&  in_rectangle = AccessRectangle());

            /* Fills the structure with the resource keys at draw time, or as if no pass had written to them. */
            void fill_draw_structure(const DrawDescription& in_draw,
                                     const bool&            in_uses_unwritten_keys,
                                     const bool&            in_is_orthographic,
                                     DrawStructure*         out_structure_ptr) const;

            /* How much of the view a clear reaches: all of it, or what the viewport and the clear rectangles leave of it. */
            AccessCoverage get_clear_coverage(const ClearOperation& in_clear,
                                              const ResourceView&   in_view) const;

            /* Whether the projection of the constants is orthographic; found again only when a constant has been updated since. */
            bool get_is_orthographic();

            /* The part of a resource that a view covers. */
            static SubresourceRange get_subresource_range(const ResourceView& in_view);

            void open_pass_if_needed();
            void record_draw_access (const DrawDescription& in_draw);

            /* Adds the histograms of a frame to those of the model, under the shape of the frame. */
            static void merge_depth_histograms(const uint32_t&                              in_shape_id,
                                               const std::vector<DepthHistogram>&           in_histogram_vec,
                                               const std::vector<PassInstance>&             in_pass_instance_vec,
                                               std::map<DepthHistogramKey, DepthHistogram>* inout_map_ptr);

            IAccessSink*                              m_access_sink_ptr;
            ResourceAccessTracker                     m_access_tracker;
            AttachmentSet                             m_attachments;
            bool                                      m_caches_draw_keys;
            std::vector<DrawKeys>                     m_call_draw_key_vec;
            uint64_t                                  m_call_index;
            std::set<ResourceId>                      m_color_copy_target_set; /* the resources that received a copy of the color of the picture */
            CompletedFrame                            m_completed_frame;
            uint64_t                                  m_constant_state_epoch;   /* counts the updates of the constants */
            ConstantStateMap                          m_constant_state_map;
            DrawKeyCache                              m_draw_key_cache;
            DrawStructure                             m_draw_signature_scratch; /* on_draw() scratch, avoids allocation */
            DrawStructure                             m_draw_structure_scratch;
            FrameInProgress                           m_frame;
            bool                                      m_has_last_draw;          /* the m_last_draw_* values are those of a draw */
            bool                                      m_is_orthographic;        /* as of m_orthographic_epoch */
            bool                                      m_keeps_history;
            ResourceKeyTable                          m_key_table;
            uint64_t                                  m_last_draw_cover_key;
            uint64_t                                  m_last_draw_signature_key;
            PrimitiveTopology                         m_last_draw_topology;
            uint32_t                                  m_last_draw_vertex_count;
            Model                                     m_model;
            uint64_t                                  m_n_frames;
            uint64_t                                  m_orthographic_epoch;
            PassTracker                               m_pass_tracker;
            std::vector<ResourceId>                   m_pending_forget_vec;
            Analysis::APIDumpTextures::ITextureSink*  m_texture_sink_ptr;
            uint64_t                                  m_timestamp_qpc;
            Viewport                                  m_viewport;
        };
    }
}

#endif /* APIDUMPANALYSIS_MODEL_BUILDER_H */
