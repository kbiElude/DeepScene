/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_CALL_FEEDER_H
#define STEREO_CONVERTERDLL_CALL_FEEDER_H

#include "Analysis/APIDumpAnalysis/frontends/analysis_session.h"
#include "Common/callbacks.h"
#include <functional>
#include <map>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Gives the analysis (see AnalysisSession, without history) the calls of the application as they are made, in the form of a dump:
         * what the front end of Direct3D 8 follows of the device is then what the offline analysis followed, and a draw has the same
         * signature (see KeyBuilder::compute_draw_signature_key) here as in the analysis a recipe was baked from.
         *
         * A call has its results only when it is done, so it is analysed then. A draw has none, and its signature is needed before it is
         * made: it is analysed at once. The data chunks of a call are moved out of the interceptor as the call is analysed.
         *
         * Not thread-safe: the caller serializes the calls.
         */
        class CallFeeder
        {
        public:
            /* Changes the contents of a data chunk of a call before it is analysed. */
            typedef std::function<void(const APIInterceptor::APIFunction& in_function,
                                       std::vector<uint8_t>*              inout_chunk_ptr)> ChunkFunc;

            CallFeeder();

            /* The key of the draw that on_pre() last analysed by its signature and size. */
            uint64_t get_last_draw_cover_key() const;

            /* The signature of the draw that on_pre() last analysed. */
            uint64_t get_last_draw_signature_key() const;

            /* True for the calls that on_pre() analyses at once. */
            static bool is_draw(const APIInterceptor::APIFunction& in_function);

            /* True for the calls of Direct3D 8, which the front end follows. */
            static bool is_followed(const APIInterceptor::APIFunction& in_function);

            /* The call is done: analyses it, if it was not a draw, with its results. */
            void on_post(const APIInterceptor::APIFunction&         in_function,
                         const uint32_t&                            in_n_arguments_out,
                         const APIInterceptor::APIFunctionArgument* in_arguments_out_ptr,
                         const APIInterceptor::APIFunctionArgument* in_returned_value_ptr);

            /* The call is about to be made: analyses it if it is a draw (and returns true), otherwise keeps what it was made with until it
             * is done.
             */
            bool on_pre(const APIInterceptor::APIFunction&         in_function,
                        const uint32_t&                            in_n_arguments,
                        const APIInterceptor::APIFunctionArgument* in_arguments_ptr);

            /* What the analysis is to see of the data of the calls, if it is not what the device was given. */
            void set_chunk_func(const ChunkFunc& in_chunk_func);

            /* The shape key (see CompletedFrame::shape_key) of the frame which the last call ended, if one was ended since this was last
             * asked: returns true once for each frame.
             */
            bool take_completed_frame_shape_key(uint64_t* out_shape_key_ptr);

        private:
            /* The calls of a thread that are made and not done. The calls past n_calls are kept for their vectors, which the next calls
             * fill again: a call then allocates nothing.
             */
            struct PendingCalls
            {
                std::vector<Capture::APIDumpLoader::DumpedAPICall> call_vec;
                size_t                                             n_calls = 0;
            };

            typedef std::map<uint32_t, PendingCalls>                     PendingCallsByThreadMap;
            typedef std::unique_ptr<APIInterceptor::APIFunctionArgument> ReturnedValueUniquePtr;

            /* A returned value with the value, from the ones that calls gave back (see fill_call()) if there is one. */
            ReturnedValueUniquePtr acquire_returned_value(const APIInterceptor::APIFunctionArgument&   in_value);
            void                   analyze               (const Capture::APIDumpLoader::DumpedAPICall& in_call);

            /* Makes the call of the function: a call that was kept for its room is given the room back. */
            void          fill_call            (const APIInterceptor::APIFunction&         in_function,
                                                const uint32_t&                            in_n_arguments,
                                                const APIInterceptor::APIFunctionArgument* in_arguments_ptr,
                                                Capture::APIDumpLoader::DumpedAPICall*     out_call_ptr);
            PendingCalls* get_pending_calls_ptr();


            ChunkFunc                                             m_chunk_func;
            std::vector<uint32_t>                                 m_chunk_id_vec;                /* scratch of analyze() */
            uint64_t                                              m_completed_frame_shape_key;
            bool                                                  m_has_completed_frame;
            uint32_t                                              m_last_thread_id;              /* the thread of m_last_thread_pending_calls_ptr */
            PendingCalls*                                         m_last_thread_pending_calls_ptr;
            PendingCallsByThreadMap                               m_pending_calls_by_thread_map;
            uint64_t                                              m_qpc_frequency;
            std::vector<ReturnedValueUniquePtr>                   m_returned_value_pool;         /* the ones that calls gave back */
            Analysis::APIDumpAnalysis::Frontends::AnalysisSession m_session;
            Capture::APIDumpLoader::Workload                      m_workload;                    /* scratch of analyze() */
        };
    }
}

#endif /* STEREO_CONVERTERDLL_CALL_FEEDER_H */
