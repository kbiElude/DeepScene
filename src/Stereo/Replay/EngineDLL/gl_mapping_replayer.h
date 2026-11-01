/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_GL_MAPPING_REPLAYER_H
#define STEREO_REPLAY_ENGINEDLL_GL_MAPPING_REPLAYER_H

#include "gl_replay_context.h"
#include <functional>
#include <map>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* Replays the mapping of buffers. The app writes into the memory a mapping gives out, and the dump holds what it wrote
             * with the call that unmaps (or flushes) it: at that call, the data goes to the memory that the replay's own mapping
             * gave out.
             */
            class GlMappingReplayer
            {
            public:
                typedef std::function<void*(const char*)> ResolveFunc;

                GlMappingReplayer(GlReplayContext* in_context_ptr,
                                  ResolveFunc      in_resolve_func);

                bool replay(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                            std::string*                                 out_error_ptr);
                void reset ();

                /* True if the function maps, unmaps or flushes a buffer. */
                static bool is_mapping_function(const APIInterceptor::APIFunction& in_function);

            private:
                /* What the replay's own mapping gave out. */
                struct Mapping
                {
                    int64_t  n_bytes;
                    uint8_t* live_data_ptr;
                };

                bool     flush          (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const bool&                                  in_is_named,
                                         std::string*                                 out_error_ptr);
                uint64_t get_mapping_key(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const bool&                                  in_is_named) const;
                bool     map            (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const bool&                                  in_is_named,
                                         const bool&                                  in_is_range,
                                         std::string*                                 out_error_ptr);
                bool     unmap          (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                         const bool&                                  in_is_named,
                                         std::string*                                 out_error_ptr);


                static uint32_t get_first_argument(const Capture::APIDumpLoader::DumpedAPICall& in_call);


                GlReplayContext*            m_context_ptr;
                std::map<uint64_t, Mapping> m_mapping_map;
                ResolveFunc                 m_resolve_func;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_GL_MAPPING_REPLAYER_H */
