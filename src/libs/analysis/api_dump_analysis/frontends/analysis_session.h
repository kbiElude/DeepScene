/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Analyzes a workload: reads it one segment at a time, hands each call to the front end of its API, and keeps the model that the front ends
 * build. The calls of an API that has no front end are counted and skipped.
 */
#ifndef APIDUMPANALYSIS_FRONTENDS_ANALYSIS_SESSION_H
#define APIDUMPANALYSIS_FRONTENDS_ANALYSIS_SESSION_H

#include "analysis/api_dump_analysis/frontends/D3D7/d3d7_frontend.h"
#include "analysis/api_dump_analysis/frontends/D3D8/d3d8_frontend.h"
#include "analysis/api_dump_analysis/frontends/OpenGL/opengl_frontend.h"
#include "analysis/api_dump_analysis/frontends/shared/types.h"
#include "analysis/api_dump_analysis/model_builder.h"
#include "analysis/api_dump_analysis/types.h"
#include <functional>
#include <string>

namespace Analysis
{
    namespace APIDumpAnalysis
    {
        namespace Frontends
        {
            class AnalysisSession
            {
            public:
                typedef std::function<void(const SessionProgress& in_progress)> ProgressFunc;

                /* If in_keeps_history is false, no frames are kept. Use this to play calls back instead of analyzing them (see ModelBuilder). */
                explicit AnalysisSession(const bool& in_keeps_history = true);


                /* Counts vertices in the open OpenGL block of the thread that were not given as calls (see GLDrawHandler::add_block_vertices). */
                void add_block_vertices(const uint32_t& in_thread_id,
                                        const uint32_t& in_n_vertices);

                /* The keys of each draw that the last call made (see ModelBuilder::get_call_draw_key_vec). */
                const std::vector<Analysis::APIDumpAnalysis::DrawKeys>& get_call_draw_key_vec() const;

                /* The frame that the last call completed. Valid only if process_call() returned true. */
                const Analysis::APIDumpAnalysis::CompletedFrame& get_completed_frame() const;

                /* The resource that an application object stands for, looked up by its address (see IAPIFrontend::find_resource_view). */
                bool find_resource_view(const Capture::APIDumpLoader::APIGroup&  in_group,
                                        const uint64_t&                          in_address,
                                        Analysis::APIDumpAnalysis::ResourceView* out_view_ptr) const;

                /* The key of the last analyzed draw, built from its signature and size (see ModelBuilder::get_last_draw_cover_key). */
                uint64_t get_last_draw_cover_key() const;

                /* Whether the last analyzed draw samples a copy of the picture (see ModelBuilder::get_last_draw_is_sampling_copied_color). */
                bool get_last_draw_is_sampling_copied_color() const;

                /* The signature of the last analyzed draw (see ModelBuilder::get_last_draw_signature_key). */
                uint64_t get_last_draw_signature_key() const;

                const Analysis::APIDumpAnalysis::Model& get_model() const;

                /* The front end of OpenGL, for what it tracks of the state of the contexts. */
                const OpenGLFrontend& get_opengl_frontend() const;

                /* What the workload contains and how it was read. Final once the analysis is over. */
                WorkloadSummary get_summary() const;

                /* The viewport the application has set (see ModelBuilder::get_viewport). */
                const Analysis::APIDumpAnalysis::Viewport& get_viewport() const;

                /* The workload that run() read; empty before run() is called. */
                WorkloadReference get_workload_reference() const;

                /* True once the limit of set_max_frames() is reached. */
                bool has_reached_frame_limit() const;

                /* Analyzes one call, after the calls before it. The segment holds the data chunks of the call. Returns true if the call ended a
                 * frame.
                 */
                bool process_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                  const Capture::APIDumpLoader::Workload&      in_segment);

                /* Analyzes the calls of a segment, after the calls of the earlier segments. */
                void process_segment(const Capture::APIDumpLoader::Workload& in_segment);

                /* Analyzes a workload file and reports progress after each segment (an empty function means no reporting). Returns false and sets
                 * the error if the file cannot be read. A file that ends abruptly is analyzed as far as it goes, and the summary says so.
                 */
                bool run(const char*         in_filename_ptr,
                         const ProgressFunc& in_progress_func,
                         std::string*        out_error_ptr);

                /* The sink that is told about uses of resources (see IAccessSink and ModelBuilder::set_access_sink). The default, nullptr, means no sink. */
                void set_access_sink(Analysis::APIDumpAnalysis::IAccessSink* in_sink_ptr);

                /* Stops the analysis after this many frames. The default, 0, means no limit. */
                void set_max_frames(const uint64_t& in_max_frames);

                /* The number of threads that read the segments of the file ahead of the analysis (see WorkloadReader::Options). The default, 0,
                 * reads each segment when the analysis asks for it. The result of the analysis does not depend on this setting.
                 */
                void set_n_reader_threads(const uint32_t& in_n_threads);

                /* Data chunks larger than this many bytes are not read from the workload (see WorkloadReader::Options). Use it for an analysis
                 * that does not look at vertices and pixels, or for a session without history, which does not sample the geometry. The default, 0, reads all chunks.
                 */
                void set_skip_data_chunks_above_n_bytes(const uint32_t& in_n_bytes);

                /* The sink that is told about what happens to the textures (see ITextureSink and ModelBuilder::set_texture_sink). The default, nullptr, means no sink. */
                void set_texture_sink(Analysis::APIDumpTextures::ITextureSink* in_sink_ptr);

                /* Moves the model out of the session instead of copying it. Call it when the analysis is done; do not ask for the progress afterwards. */
                Analysis::APIDumpAnalysis::Model take_model();

            private:
                static constexpr size_t N_API_GROUPS = static_cast<size_t>(Capture::APIDumpLoader::APIGroup::UNKNOWN) + 1;

                /* The front end of the API, or nullptr if there is none. */
                IAPIFrontend* get_frontend_ptr(const Capture::APIDumpLoader::APIGroup& in_group);

                SessionProgress get_progress() const;

                Analysis::APIDumpAnalysis::ModelBuilder m_builder;
                D3D7Frontend                            m_d3d7_frontend;
                D3D8Frontend                            m_d3d8_frontend;
                uint64_t                                m_max_frames;
                std::array<uint64_t, N_API_GROUPS>      m_n_calls_by_group_array; /* counted here, and put into the summary when the summary is requested */
                uint64_t                                m_n_calls_since_frame;
                uint32_t                                m_n_reader_threads;
                OpenGLFrontend                          m_opengl_frontend;
                uint32_t                                m_skip_data_chunks_above_n_bytes;
                WorkloadSummary                         m_summary;
                WorkloadReference                       m_workload_reference;
            };
        }
    }
}

#endif /* APIDUMPANALYSIS_FRONTENDS_ANALYSIS_SESSION_H */
