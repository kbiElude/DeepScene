/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_CALL_CONTEXT_H
#define STEREO_REPLAY_ENGINEDLL_CALL_CONTEXT_H

#include "Common/argument_reader.h"
#include "i_chunk_source.h"
#include "object_remapper.h"

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* The call which an executor replays at the moment: its arguments, the data chunks they refer to, and the objects
             * replayed so far. The arguments which the dumper added to the ones of the API function (data it stored apart) follow
             * the first n_head_arguments.
             */
            class CallContext
            {
            public:
                CallContext(ObjectRemapper* in_remapper_ptr);

                void                                         begin_call          (const Capture::APIDumpLoader::DumpedAPICall* in_call_ptr,
                                                                                  const IChunkSource*                          in_chunk_source_ptr,
                                                                                  const size_t&                                in_n_head_arguments);
                const Capture::APIDumpLoader::DumpedAPICall& get_call            ()                                                           const;
                const uint8_t*                               get_chunk_data_ptr  (const APIInterceptor::APIFunctionArgument*   in_argument_ptr,
                                                                                  size_t*                                      out_n_bytes_ptr) const;
                const APIInterceptor::APIFunctionArgument*   get_in_argument_ptr (const size_t&                                in_n_argument) const;
                size_t                                       get_n_head_arguments()                                                           const;
                const APIInterceptor::APIFunctionArgument*   get_out_argument_ptr(const size_t&                                in_n_argument) const;
                ObjectRemapper*                              get_remapper_ptr    ()                                                           const;

            private:
                const Capture::APIDumpLoader::DumpedAPICall* m_call_ptr;
                size_t                                       m_n_head_arguments;
                ObjectRemapper*                              m_remapper_ptr;
                const IChunkSource*                          m_chunk_source_ptr;
            };

            /* Reads a number of any size or kind from a dumped argument (see APIInterceptor::read_number()). */
            using APIInterceptor::read_number;
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_CALL_CONTEXT_H */
