/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_GL_CLIENT_ARRAY_REPLAYER_H
#define STEREOREPLAYENGINEDLL_GL_CLIENT_ARRAY_REPLAYER_H

#include "gl_replay_context.h"
#include <functional>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Replays the vertex arrays that the app keeps in its own memory. The dump holds the part of each which a draw call
         * reads (see Interception::Hooks::OpenGL::ClientArrayManager for the layout) as the last argument of the call; before the call, the arrays
         * are pointed at the copy.
         */
        class GlClientArrayReplayer
        {
        public:
            typedef std::function<void*(const char*)> ResolveFunc;

            GlClientArrayReplayer(GlReplayContext* in_context_ptr,
                                  ResolveFunc      in_resolve_func);

            /* Points the arrays at the snapshot (a chunk, as the dumper made it). */
            bool apply(const uint8_t* in_snapshot_ptr,
                       const size_t&  in_n_bytes);

        private:
            /* One array of a snapshot, as the dumper wrote it. */
            #pragma pack(push, 1)
            struct SnapshotArray
            {
                uint32_t kind;
                uint32_t index;
                int32_t  size;
                uint32_t type;
                uint32_t stride;
                uint32_t flags;
                uint32_t divisor;
                uint64_t pointer;
                uint32_t first_element;
                uint32_t n_elements;
                uint32_t n_bytes;
            };
            #pragma pack(pop)

            static_assert(sizeof(SnapshotArray) == 48,
                          "Unexpected layout of an array of a snapshot");

            void apply_array(const SnapshotArray& in_array,
                             const uint8_t*       in_data_ptr);

            /* The elements are one stride apart; a stride of 0 means that they follow each other. */
            static uint32_t get_element_stride(const SnapshotArray& in_array);

            void point_edge_flag   (const SnapshotArray& in_array,
                                    const void*          in_pointer);
            void point_generic     (const SnapshotArray& in_array,
                                    const void*          in_pointer);
            void point_tex_coord   (const SnapshotArray& in_array,
                                    const void*          in_pointer);
            void point_with_size   (const char*          in_function_name_ptr,
                                    const SnapshotArray& in_array,
                                    const void*          in_pointer);
            void point_without_size(const char*          in_function_name_ptr,
                                    const SnapshotArray& in_array,
                                    const void*          in_pointer);

            GlReplayContext* m_context_ptr;
            ResolveFunc      m_resolve_func;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_GL_CLIENT_ARRAY_REPLAYER_H */
