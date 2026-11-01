/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_D3D8_REPLAY_CONTEXT_H
#define STEREO_REPLAY_ENGINEDLL_D3D8_REPLAY_CONTEXT_H

#include "call_context.h"
#include "live_object_list.h"
#include <string>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* What the replay of one D3D8 call needs to know: the call, the objects replayed so far, and where the data that
             * the dumper stored apart from the arguments is (see the hook policies of the D3D8 library).
             */
            class D3D8ReplayContext : public CallContext
            {
            public:
                D3D8ReplayContext(ObjectRemapper* in_remapper_ptr);

                /* Starts the call: the expectations of the previous one are gone. */
                void begin_call(const Capture::APIDumpLoader::DumpedAPICall* in_call_ptr,
                                const IChunkSource*                          in_chunk_source_ptr,
                                const size_t&                                in_n_head_arguments);

                /* An argument asks for an object: if the call fails, so does the replay. */
                void expect_creation();

                /* The object has been destroyed (its last reference is gone). */
                void forget_object(const uint64_t& in_dumped_value);

                /* The device that the replay has created; nullptr if there is none. */
                void* get_device_ptr() const;

                const uint8_t* get_extra_data_ptr  (const size_t&                              in_n_argument,
                                                    size_t*                                    out_n_bytes_ptr) const;
                bool           get_live_handle     (const ObjectKind&                          in_kind,
                                                    const APIInterceptor::APIFunctionArgument& in_argument,
                                                    uint64_t*                                  out_live_value_ptr) const;
                bool           get_live_object     (const APIInterceptor::APIFunctionArgument& in_argument,
                                                    void**                                     out_live_object_ptr,
                                                    std::string*                               out_error_ptr) const;
                bool           is_creation_expected() const;

                /* A handle (a shader, a state block) which the method takes: the live one, or the value itself if the replay
                 * has not got it (a vertex shader argument can hold an FVF code, too).
                 */
                uint32_t map_handle_argument(const size_t&   in_n_argument,
                                             const uint32_t& in_dumped_value) const;

                /* The call has created the object (or handed out a new reference to it). */
                void register_handle(const size_t&   in_n_argument,
                                     const uint32_t& in_live_value);

                void register_object(const uint64_t& in_dumped_value,
                                     void*           in_live_object_ptr);

                /* Releases what is left of the objects, the latest ones first. */
                void release_all_objects();

                /* The object that the output argument stands for, after the call has created it, is the device. */
                void set_device_from_argument(const size_t& in_n_argument);

            private:
                /* An argument of a method which points to data that the dumper stored in an argument after the method's own. */
                struct ExtraBinding
                {
                    APIInterceptor::APIFunction function;
                    size_t                      n_argument;
                    size_t                      n_extra_argument;
                };

                /* A DWORD argument which is a handle that the replay gets from the API (a shader, a state block). */
                struct HandleBinding
                {
                    APIInterceptor::APIFunction function;
                    bool                        is_output;
                    ObjectKind                  kind;
                    size_t                      n_argument;
                };


                static const HandleBinding* get_handle_binding_ptr(const APIInterceptor::APIFunction& in_function,
                                                                   const size_t&                      in_n_argument,
                                                                   const bool&                        in_is_output);


                void*                               m_device_ptr;
                bool                                m_is_creation_expected;
                LiveObjectList                      m_live_objects;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_D3D8_REPLAY_CONTEXT_H */
