/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_CALL_CONTEXT_H
#define STEREOREPLAYENGINEDLL_CALL_CONTEXT_H

#include "call_walker.h"
#include "object_remapper.h"

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* The call which an executor replays at the moment: its arguments, the data chunks they refer to, and the objects
         * replayed so far. The arguments which the dumper added to the ones of the API function (data it stored apart) follow
         * the first n_head_arguments.
         */
        class CallContext
        {
        public:
            CallContext(ObjectRemapper* in_remapper_ptr);

            void                                       begin_call          (const Capture::APIDumpLoader::DumpedAPICall*        in_call_ptr,
                                                                                     const CallWalker*                          in_walker_ptr,
                                                                                     const size_t&                              in_n_head_arguments);
            const Capture::APIDumpLoader::DumpedAPICall&        get_call            ()                                                         const;
            const uint8_t*                             get_chunk_data_ptr  (const APIInterceptor::APIFunctionArgument* in_argument_ptr,
                                                                            size_t*                                    out_n_bytes_ptr) const;
            const APIInterceptor::APIFunctionArgument* get_in_argument_ptr (const size_t&                              in_n_argument) const;
            size_t                                     get_n_head_arguments()                                                         const;
            const APIInterceptor::APIFunctionArgument* get_out_argument_ptr(const size_t&                              in_n_argument) const;
            ObjectRemapper*                            get_remapper_ptr    ()                                                         const;

        private:
            const Capture::APIDumpLoader::DumpedAPICall* m_call_ptr;
            size_t                              m_n_head_arguments;
            ObjectRemapper*                     m_remapper_ptr;
            const CallWalker*                   m_walker_ptr;
        };

        /* Reads a number of any size or kind from a dumped argument. Returns false if the argument is of another kind. */
        template<typename T>
        bool read_number(const APIInterceptor::APIFunctionArgument& in_argument,
                         T*                                         out_value_ptr)
        {
            bool result = true;

            switch (in_argument.type)
            {
                case APIInterceptor::ARGTYPE_F32:        *out_value_ptr = static_cast<T>(in_argument.get_fp32() );       break;
                case APIInterceptor::ARGTYPE_F64:        *out_value_ptr = static_cast<T>(in_argument.get_fp64() );       break;
                case APIInterceptor::ARGTYPE_I8:         *out_value_ptr = static_cast<T>(in_argument.get_i8() );         break;
                case APIInterceptor::ARGTYPE_I16:        *out_value_ptr = static_cast<T>(in_argument.get_i16() );        break;
                case APIInterceptor::ARGTYPE_I32:        *out_value_ptr = static_cast<T>(in_argument.get_i32() );        break;
                case APIInterceptor::ARGTYPE_I32_GLENUM: *out_value_ptr = static_cast<T>(in_argument.get_i32_glenum() ); break;
                case APIInterceptor::ARGTYPE_I64:        *out_value_ptr = static_cast<T>(in_argument.get_i64() );        break;
                case APIInterceptor::ARGTYPE_U8:         *out_value_ptr = static_cast<T>(in_argument.get_u8() );         break;
                case APIInterceptor::ARGTYPE_U16:        *out_value_ptr = static_cast<T>(in_argument.get_u16() );        break;
                case APIInterceptor::ARGTYPE_U32:        *out_value_ptr = static_cast<T>(in_argument.get_u32() );        break;
                case APIInterceptor::ARGTYPE_U32_GLENUM: *out_value_ptr = static_cast<T>(in_argument.get_u32_glenum() ); break;
                case APIInterceptor::ARGTYPE_U64:        *out_value_ptr = static_cast<T>(in_argument.get_u64() );        break;

                default:
                {
                    result = false;

                    break;
                }
            }

            return result;
        }
    }
}

#endif /* STEREOREPLAYENGINEDLL_CALL_CONTEXT_H */
