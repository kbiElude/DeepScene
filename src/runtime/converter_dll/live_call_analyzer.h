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
#include <functional>
#include <map>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Feeds the calls of the application to the analysis (see AnalysisSession, created without history) as they are made, in the form of a dump.
         * The API front end therefore tracks the device the same way the offline analysis did, and a draw gets the same signature
         * (see MatchKeyBuilder::compute_draw_signature_key) here as in the analysis the recipe was baked from.
         *
         * A call is analysed when it is done, because only then are its results known. A draw is the exception: its signature is needed
         * before the draw is made, so it is analysed at once. The data chunks of a call are moved out of the interceptor when the call is analysed.
         *
         * The call analyzer works with any API: it uses the front end that matches the API of the calls (see AnalysisSession).
         *
         * Not thread-safe: the caller serializes the calls.
         */
        class LiveCallAnalyzer
        {
        public:
            /* Lets the caller change the contents of a data chunk of a call before the call is analysed. */
            typedef std::function<void(const APIInterceptor::APIFunction& in_function,
                                       std::vector<uint8_t>*              inout_chunk_ptr)> ChunkFunc;

            LiveCallAnalyzer();

            /* Returns a data chunk of the call that was analysed last. The call analyzer took the chunk out of the interceptor. Returns false if the call has no such
             * chunk. The data stays valid until the next call is analysed.
             */
            bool get_last_call_chunk(const uint32_t& in_chunk_id,
                                     const void**    out_data_ptr_ptr,
                                     uint32_t*       out_n_bytes_ptr) const;

            /* Returns the keys of each draw made by the call that was analysed last (see ModelBuilder::get_call_draw_key_vec). A call of a display list can make
             * several draws, or none. The keys stay valid until the next call is analysed.
             */
            const std::vector<Analysis::APIDumpAnalysis::DrawKeys>& get_last_call_draw_key_vec() const;

            /* Returns the key of the draw that on_pre() analysed last, looked up by its signature and size. */
            uint64_t get_last_draw_cover_key() const;

            /* Returns the signature of the draw that on_pre() analysed last. */
            uint64_t get_last_draw_signature_key() const;

            /* Returns the viewport that the application has set. The next draw is made with it. */
            const Analysis::APIDumpAnalysis::Viewport& get_viewport() const;

            /* Returns true for the calls that on_pre() analyses at once, which are the draws. */
            static bool is_draw(const APIInterceptor::APIFunction& in_function);

            /* Returns true for the calls that the front ends follow: Direct3D 7, Direct3D 8 and OpenGL, plus the GDI32 SwapBuffers, which ends an OpenGL frame. */
            static bool is_followed(const APIInterceptor::APIFunction& in_function);

            /* Called when the call is done. Analyses the call with its results, unless it is a draw (on_pre() already analysed draws). */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments_out,
                         const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr,
                         const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

            /* Called before the call is made. A draw is analysed at once and the function returns true. Any other call is stored until on_post() is called
             * for it.
             */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* Sets the function that changes the data of the calls before the analysis sees them. Without it, the analysis sees the data the device was given. */
            void set_chunk_func(const ChunkFunc& in_chunk_func);

            /* Returns true, once per frame, if the last analysed call ended a frame since the previous call of this function. Then it sets the shape key of the
             * frame (see CompletedFrame::shape_key).
             */
            bool take_completed_frame_shape_key(uint64_t* out_shape_key_ptr);

        private:
            /* The calls of one thread that have been started but are not done. The slots from n_calls on are free. They keep their vectors, so that the next
             * calls reuse the memory and allocate nothing.
             */
            struct PendingCalls
            {
                std::vector<Capture::APIDumpLoader::DumpedAPICall> call_vec;
                size_t                                             n_calls = 0;
            };

            typedef std::map<uint32_t, PendingCalls>                     PendingCallsByThreadMap;
            typedef std::unique_ptr<APIInterceptor::APIFunctionArgument> ReturnedValueUniquePtr;

            /* Returns a returned-value object that holds in_value. It reuses an object from the pool when there is one (see fill_call()). */
            ReturnedValueUniquePtr acquire_returned_value(const APIInterceptor::APIFunctionArgument&   in_value);
            void                   analyze               (const Capture::APIDumpLoader::DumpedAPICall& in_call);

            /* Fills in the call record for the function. The returned-value object that the record still holds goes back to the pool. */
            void          fill_call            (const APIInterceptor::APIFunction&         in_function,
                                                const uint32_t&                            in_n_arguments,
                                                const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                Capture::APIDumpLoader::DumpedAPICall*     out_call_ptr);
            PendingCalls* get_pending_calls_ptr();


            ChunkFunc                                             m_chunk_func;
            std::vector<uint32_t>                                 m_chunk_id_vec;                /* scratch of analyze()
 */
            APIInterceptor::ChunkPool                             m_chunk_pool;                  /* vectors of the chunks of the calls that were analysed, kept for reuse
 */
            uint64_t                                              m_completed_frame_shape_key;
            std::vector<std::vector<uint8_t>*>                    m_destination_ptr_vec;         /* scratch of analyze()
 */
            bool                                                  m_has_completed_frame;
            std::vector<uint8_t>                                  m_is_taken_u8_vec;             /* scratch of analyze()
 */
            uint32_t                                              m_last_thread_id;              /* thread that owns m_last_thread_pending_calls_ptr
 */
            PendingCalls*                                         m_last_thread_pending_calls_ptr;
            PendingCallsByThreadMap                               m_pending_calls_by_thread_map;
            uint64_t                                              m_qpc_frequency;
            std::vector<ReturnedValueUniquePtr>                   m_returned_value_pool;         /* returned-value objects that finished calls released, kept for reuse
 */
            Analysis::APIDumpAnalysis::Frontends::AnalysisSession m_session;
            Capture::APIDumpLoader::Workload                      m_workload;                    /* scratch of analyze()
 */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_LIVE_CALL_ANALYZER_H */
