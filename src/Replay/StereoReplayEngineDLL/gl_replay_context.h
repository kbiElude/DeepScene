/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_GL_REPLAY_CONTEXT_H
#define STEREOREPLAYENGINEDLL_GL_REPLAY_CONTEXT_H

#include "call_context.h"
#include <deque>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* What the table of GL functions says about one of them (see gl_replay_table.inl). */
        struct GlFunctionInfo
        {
            bool                    has_explicit_program;  /* the program of its uniform locations is its first parameter */
            uint32_t                ignored_param_mask;    /* the parameters which are not replayed (passed as null)      */
            const char*             name_ptr;
            std::vector<ObjectKind> param_kind_vec;
            ObjectKind              return_kind;
        };

        /* What the replay of one GL call needs to know. The names of the objects are mapped through the remapper; the
         * locations and the indices that a program hands out (of uniforms, uniform blocks, subroutines...) are told by the
         * program, and kept here.
         */
        class GlReplayContext : public CallContext
        {
        public:
            GlReplayContext(ObjectRemapper* in_remapper_ptr);

            /* Memory which lives till the end of the call, for the arguments which the replay has to make up. */
            std::vector<uint8_t>* allocate_scratch(const size_t& in_n_bytes);

            std::string* allocate_string();

            /* The arguments which stand for the strings of a string array. */
            std::vector<const char*>* allocate_string_pointers();

            /* Starts the call: what the previous one asked for is gone. */
            void begin_call(const Capture::APIDumpLoader::DumpedAPICall* in_call_ptr,
                            const IChunkSource*                          in_chunk_source_ptr,
                            const size_t&                                in_n_head_arguments,
                            const GlFunctionInfo*                        in_function_info_ptr);

            void forget_locations();

            /* The program that glUseProgram() set: its dumped name. */
            uint64_t get_current_program() const;

            uint64_t   get_live_return ()                            const;
            ObjectKind get_param_kind  (const size_t& in_n_argument) const;
            bool       is_param_ignored(const size_t& in_n_argument) const;
            bool       is_skipped      ()                            const;

            /* Maps what the dump holds to what the replay has: a name, or a location, or an index. Values which the replay
             * has not seen are passed on as they are.
             */
            /* A numeric argument of the call: it can be a name or a location. */
            uint32_t map_argument(const size_t&   in_n_argument,
                                  const uint32_t& in_dumped_value) const;

            uint32_t map_attrib_location(const uint32_t& in_dumped_location) const;

            /* The indices that a program hands out to the app are those of the program (and, for subroutines, of a shader
             * stage): the scope is the shader type, or 0.
             */
            uint32_t map_index(const ObjectKind& in_kind,
                               const uint64_t&   in_dumped_program,
                               const uint32_t&   in_scope,
                               const uint32_t&   in_dumped_index) const;

            uint32_t map_name                (const ObjectKind& in_kind,
                                              const uint32_t&   in_dumped_name)     const;
            int32_t  map_uniform_location    (const int32_t&    in_dumped_location) const;
            void     register_attrib_location(const uint32_t&   in_dumped_location,
                                              const uint32_t&   in_live_location);
            void     register_index          (const ObjectKind& in_kind,
                                              const uint64_t&   in_dumped_program,
                                              const uint32_t&   in_scope,
                                              const uint32_t&   in_dumped_index,
                                              const uint32_t&   in_live_index);
            void     register_name           (const ObjectKind& in_kind,
                                              const uint32_t&   in_dumped_name,
                                              const uint32_t&   in_live_name);

            /* The function returned the name of an object, or of the first of a range of them. */
            void register_returned_name(const uint64_t& in_live_value);

            void register_uniform_location(const uint64_t& in_dumped_program,
                                           const int32_t&  in_dumped_location,
                                           const int32_t&  in_live_location);
            void request_skip             ();
            void set_current_program      (const uint64_t& in_dumped_program);
            void set_live_return          (const uint64_t& in_value);

        private:
            /* The kind of the index, the dumped program, the scope and the dumped index. */
            typedef std::tuple<ObjectKind, uint64_t, uint32_t, uint32_t> IndexKey;

            /* The program an index of this call belongs to: the one the call names (its first parameter), or the current one. */
            uint64_t get_index_program() const;

            uint64_t get_uniform_program() const;

            std::map<uint32_t, uint32_t>          m_attrib_location_map;
            uint64_t                              m_current_program;
            const GlFunctionInfo*                 m_function_info_ptr;
            std::map<IndexKey, uint32_t>          m_index_map;
            bool                                  m_is_skipped;
            uint64_t                              m_live_return;
            std::deque<std::vector<uint8_t> >     m_scratch_deque;
            std::deque<std::vector<const char*> > m_string_pointers_deque;
            std::deque<std::string>               m_string_deque;
            std::map<uint64_t, int32_t>           m_uniform_location_map;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_GL_REPLAY_CONTEXT_H */
