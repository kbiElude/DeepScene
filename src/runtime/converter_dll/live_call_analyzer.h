/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_LIVE_CALL_ANALYZER_H
#define STEREO_CONVERTERDLL_LIVE_CALL_ANALYZER_H

#include "analysis/api_dump_analysis/frontends/analysis_session.h"
#include "common/api_function_argument.h"
#include "common/callbacks.h"
#include "common/chunk_pool.h"
#include <array>
#include <functional>
#include <map>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Feeds the application's calls live to the analysis (see AnalysisSession, no history), so draws get the same signature
         * (see MatchKeyBuilder::compute_draw_signature_key) as in the analysis the recipe was baked from.
         * A call is analysed when done (results known); draws at once, as their signature is needed before the draw. Data chunks
         * are moved out of the interceptor on analysis. Works with any API via the matching front end.
         * Not thread-safe: the caller serializes the calls. */
        class LiveCallAnalyzer
        {
        public:
            /* Lets the caller change the contents of a data chunk of a call before the call is analysed. */
            typedef std::function<void(const APIInterceptor::APIFunction& in_function,
                                       std::vector<uint8_t>*              inout_chunk_ptr)> ChunkFunc;

            LiveCallAnalyzer();

            /* Returns a data chunk of the last analysed call (taken out of the interceptor). Returns false if there is no such chunk.
             * Valid until the next call is analysed. */
            bool get_last_call_chunk(const uint32_t& in_chunk_id,
                                     const void**    out_data_ptr_ptr,
                                     uint32_t*       out_n_bytes_ptr) const;

            /* Returns the model-view and projection matrices that the analysis tracked for the OpenGL context of the calling thread,
             * and its matrix mode. Returns false if the thread has no current context. */
            bool get_current_gl_matrices(std::array<float, 16>* out_modelview_f32mat_ptr,
                                         std::array<float, 16>* out_projection_f32mat_ptr,
                                         uint32_t*              out_matrix_mode_ptr) const;

            /* Returns the keys of each draw of the last analysed call (see ModelBuilder::get_call_draw_key_vec); a display list call
             * can make several or none. Valid until the next call is analysed. */
            const std::vector<Analysis::APIDumpAnalysis::DrawKeys>& get_last_call_draw_key_vec() const;

            /* Returns the key of the draw that on_pre() analysed last, looked up by its signature and size. */
            uint64_t get_last_draw_cover_key() const;

            /* Returns the signature of the draw that on_pre() analysed last. */
            uint64_t get_last_draw_signature_key() const;

            /* Returns the viewport that the application has set. The next draw is made with it. */
            const Analysis::APIDumpAnalysis::Viewport& get_viewport() const;

            /* Returns true for the calls that on_pre() analyses before they are made, and that need no on_post(): the calls that pass a vertex (OpenGL), which have no result. */
            static bool is_analyzed_before_the_call(const APIInterceptor::APIFunction& in_function);

            /* Returns true for the calls that set a value for the vertices that follow (a color, a normal, a texture coordinate), which the analysis leaves out by design. */
            static bool is_attribute_call(const APIInterceptor::APIFunction& in_function);

            /* Returns true for the calls on_pre() analyses at once (draws). */
            static bool is_draw(const APIInterceptor::APIFunction& in_function);

            /* Returns true for the calls the front ends follow: Direct3D 7, Direct3D 8, OpenGL, and GDI32 SwapBuffers (ends an OpenGL frame). */
            static bool is_followed(const APIInterceptor::APIFunction& in_function);

            /* Called when the call is done. Analyses the call with its results, unless it is a draw (on_pre() already analysed draws). */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments_out,
                         const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr,
                         const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

            /* Called before the call is made. A draw is analysed at once (returns true); other calls are stored until on_post(). */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Sets the function that changes call data before analysis; without it the analysis sees the data the device was given. */
            void set_chunk_func(const ChunkFunc& in_chunk_func);

            /* Returns true once per frame if the last analysed call ended a frame since the previous call; then sets the frame's
             * shape key (see CompletedFrame::shape_key). */
            bool take_completed_frame_shape_key(uint64_t* out_shape_key_ptr);

        private:
            /* The started, unfinished calls of one thread. Slots from n_calls on are free and keep their vectors for reuse. */
            struct PendingCalls
            {
                std::vector<Capture::APIDumpLoader::DumpedAPICall> call_vec;
                size_t                                             n_calls = 0;
            };

            typedef std::map<uint32_t, PendingCalls>                     PendingCallsByThreadMap;
            typedef std::unique_ptr<APIInterceptor::APIFunctionArgument> ReturnedValueUniquePtr;

            /* Returns a returned-value object holding in_value, reused from the pool if possible (see fill_call()). */
            ReturnedValueUniquePtr acquire_returned_value(const APIInterceptor::APIFunctionArgument&   in_value);
            void                   analyze               (const Capture::APIDumpLoader::DumpedAPICall& in_call);

            /* Fills in the call record; the record's old returned-value object goes back to the pool. */
            void          fill_call            (const APIInterceptor::APIFunction&         in_function,
                                                const uint32_t&                            in_n_arguments,
                                                const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                Capture::APIDumpLoader::DumpedAPICall*     out_call_ptr);
            PendingCalls* get_pending_calls_ptr();


            ChunkFunc                                             m_chunk_func;
            std::vector<uint32_t>                                 m_chunk_id_vec;                /* scratch of analyze()
 */
            APIInterceptor::ChunkPool                             m_chunk_pool;                  /* chunk vectors of analysed calls, kept for reuse
 */
            uint64_t                                              m_completed_frame_shape_key;
            std::vector<std::vector<uint8_t>*>                    m_destination_ptr_vec;         /* scratch of analyze()
 */
            bool                                                  m_has_completed_frame;
            std::vector<uint8_t>                                  m_is_analyzed_before_the_call_u8_vec; /* indexed by function */
            std::vector<uint8_t>                                  m_is_taken_u8_vec;             /* scratch of analyze()
 */
            uint32_t                                              m_last_thread_id;              /* thread that owns m_last_thread_pending_calls_ptr
 */
            PendingCalls*                                         m_last_thread_pending_calls_ptr;
            PendingCallsByThreadMap                               m_pending_calls_by_thread_map;
            uint64_t                                              m_qpc_frequency;
            std::vector<ReturnedValueUniquePtr>                   m_returned_value_pool;         /* released returned-value objects, kept for reuse
 */
            Analysis::APIDumpAnalysis::Frontends::AnalysisSession m_session;
            Capture::APIDumpLoader::Workload                      m_workload;                    /* scratch of analyze()
 */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_LIVE_CALL_ANALYZER_H */
