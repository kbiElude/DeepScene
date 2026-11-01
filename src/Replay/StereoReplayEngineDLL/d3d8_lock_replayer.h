/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_D3D8_LOCK_REPLAYER_H
#define STEREOREPLAYENGINEDLL_D3D8_LOCK_REPLAYER_H

#include "d3d8_replay_context.h"
#include <Windows.h>
#include <map>
#include <string>
#include <tuple>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Replays the locks of surfaces, volumes and buffers. The app writes into the memory a lock gives out, and the dump
         * holds what it wrote with the call that unlocks: at that call, the data goes to the memory that the replay's own
         * lock gave out (the pitches of the two can differ), and the resource is unlocked.
         */
        class D3D8LockReplayer
        {
        public:
            D3D8LockReplayer(D3D8ReplayContext* in_context_ptr);

            bool lock  (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                 std::string*                        out_error_ptr);
            void reset ();
            bool unlock(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                 std::string*                        out_error_ptr);

            /* True if the function locks, or unlocks, something. */
            static bool is_lock_function(const APIInterceptor::APIFunction& in_function);

            static bool is_unlock_function(const APIInterceptor::APIFunction& in_function);

        private:
            enum class LockKind
            {
                BOX,
                BUFFER,
                RECT
            };

            /* What the replay's own lock gave out, and how the dumped one laid the data out. */
            struct LockRecord
            {
                uint32_t depth;
                int32_t  dumped_row_pitch;
                int32_t  dumped_slice_pitch;
                DWORD    flags;
                uint8_t* live_data_ptr;
                int32_t  live_row_pitch;
                int32_t  live_slice_pitch;
                uint32_t n_buffer_bytes;
                uint32_t n_row_bytes;
                uint32_t n_rows;
            };

            /* Where the arguments of a call that locks, or unlocks, are, and which slots of the vtable to call. */
            struct LockSpec
            {
                uint32_t                    desc_vtable_index;
                APIInterceptor::APIFunction function;
                bool                        is_unlock;
                LockKind                    kind;
                int                         n_face_argument;
                int                         n_flags_argument;
                int                         n_level_argument;
                int                         n_locked_argument;
                int                         n_offset_argument;
                int                         n_region_argument;
                int                         n_size_argument;
                uint32_t                    vtable_index;
            };

            /* The object, the cube map face and the level. */
            typedef std::tuple<void*, uint32_t, uint32_t> LockKey;

            bool lock_box   (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      void*                               in_object_ptr,
                                      const LockKey&                      in_key,
                                      std::string*                        out_error_ptr);
            bool lock_buffer(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      void*                               in_object_ptr,
                                      const LockKey&                      in_key,
                                      std::string*                        out_error_ptr);
            bool lock_rect  (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                      void*                               in_object_ptr,
                                      const LockKey&                      in_key,
                                      std::string*                        out_error_ptr);

            /* The value of an argument slot of a LockSpec which the call does not have. */
            static const int ARGUMENT_ABSENT;

            /* The int32 at an index of an argument which is a chunk holding int32 values (the lock structures); 0 if it is not. */
            static int32_t         get_dumped_int32 (const D3D8ReplayContext&           in_context,
                                                     const size_t&                      in_n_argument,
                                                     const size_t&                      in_index);
            static const LockSpec* get_lock_spec_ptr(const APIInterceptor::APIFunction& in_function);
            static uint32_t        get_u32_argument (const D3D8ReplayContext&           in_context,
                                                     const int&                         in_n_argument);

            D3D8ReplayContext*            m_context_ptr;
            std::map<LockKey, LockRecord> m_lock_record_map;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_D3D8_LOCK_REPLAYER_H */
