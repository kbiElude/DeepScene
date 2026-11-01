/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_GL_INDEX_REPLAYER_H
#define STEREOREPLAYENGINEDLL_GL_INDEX_REPLAYER_H

#include "gl_replay_context.h"
#include <functional>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Replays the calls that take an array whose positions are numbers that a program hands out: the n-th of the draw
         * buffers is that of the n-th fragment shader output, the n-th of the subroutines is that of the n-th subroutine
         * uniform. The driver of the replay can number them otherwise than the driver of the dump, so the array is permuted
         * to what the replay's program has, and the subroutines are those of its own.
         */
        class GlIndexReplayer
        {
        public:
            typedef std::function<void*(const char*)> ResolveFunc;

            GlIndexReplayer(GlReplayContext* in_context_ptr,
                            ResolveFunc      in_resolve_func);

            /* The array in the order of the replay's numbers: where the n-th element goes is told by the map. */
            std::vector<uint32_t> permute(const std::vector<uint32_t>& in_dumped_vec,
                                          const ObjectKind&            in_kind,
                                          const uint32_t&              in_scope,
                                          const uint32_t&              in_empty_value) const;

            bool replay(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                        std::string*                                 out_error_ptr);


            /* True if the function takes such an array. */
            static bool is_index_function(const APIInterceptor::APIFunction& in_function);

        private:
            bool replay_draw_buffers       (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            const bool&                                  in_is_named,
                                            std::string*                                 out_error_ptr);
            bool replay_uniform_subroutines(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                            std::string*                                 out_error_ptr);


            /* The elements of an array of 32-bit numbers which the dump holds. */
            static std::vector<uint32_t> get_numbers(const GlReplayContext& in_context,
                                                     const size_t&          in_n_argument);


            GlReplayContext* m_context_ptr;
            ResolveFunc      m_resolve_func;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_GL_INDEX_REPLAYER_H */
