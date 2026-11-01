/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Calls a GL function of the replay with the arguments that the dump holds, whatever the function is: the invoker is
 * generated from the type of the function (see gl_replay_table.inl).
 */
#ifndef STEREOREPLAYENGINEDLL_GL_REPLAY_INVOKER_H
#define STEREOREPLAYENGINEDLL_GL_REPLAY_INVOKER_H

#include "gl_argument_restorer.h"
#include <stdint.h>
#include <tuple>
#include <type_traits>
#include <utility>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        namespace GlReplay
        {
            /* Calls the function, and keeps the value it returns. */
            template<typename ReturnType>
            struct ReturnHolder
            {
                ReturnType value;

                ReturnHolder()
                    :value()
                {
                    /* Stub */
                }

                template<typename Function, typename... Arguments>
                void call(Function in_function, Arguments&&... in_arguments)
                {
                    value = in_function(std::forward<Arguments>(in_arguments)...);
                }

                uint64_t to_u64() const
                {
                    return convert(std::is_pointer<ReturnType>() );
                }

            private:
                uint64_t convert(std::true_type) const
                {
                    return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value) );
                }

                uint64_t convert(std::false_type) const
                {
                    return static_cast<uint64_t>(value);
                }
            };

            template<>
            struct ReturnHolder<void>
            {
                template<typename Function, typename... Arguments>
                void call(Function in_function, Arguments&&... in_arguments)
                {
                    in_function(std::forward<Arguments>(in_arguments)...);
                }

                uint64_t to_u64() const
                {
                    return 0;
                }
            };

            template<typename FunctionPtr>
            struct Invoker;

            template<typename ReturnType, typename... Arguments>
            struct Invoker<ReturnType (APIENTRY*)(Arguments...)>
            {
                typedef ReturnType (APIENTRY* FunctionPtr)(Arguments...);

                static constexpr size_t get_n_arguments()
                {
                    return sizeof...(Arguments);
                }

                /* The context holds the call (see GlReplayContext::begin_call()). */
                static bool invoke(GlReplayContext* in_context_ptr,
                                   void*            in_function_ptr,
                                   std::string*     out_error_ptr)
                {
                    return invoke_impl(in_context_ptr,
                                       in_function_ptr,
                                       out_error_ptr,
                                       std::index_sequence_for<Arguments...>() );
                }

            private:
                template<size_t... Indices>
                static bool invoke_impl(GlReplayContext*     in_context_ptr,
                                        void*                in_function_ptr,
                                        std::string*         out_error_ptr,
                                        std::index_sequence<Indices...>)
                {
                    std::tuple<GlRestore::ArgumentRestorer<Arguments, GlRestore::ArgumentCategoryOf<Arguments>::value>...> restorers;
                    bool                                                                                                    is_ok  = true;
                    ReturnHolder<ReturnType>                                                                                holder;
                    bool                                                                                                    result = false;

                    /* Left to right, and no further once an argument cannot be restored. */
                    const bool prepared_dummy_bvec[] = {true, (is_ok = (is_ok && std::get<Indices>(restorers).prepare(in_context_ptr,
                                                                                                                        Indices,
                                                                                                                        out_error_ptr) ))...};

                    (void) prepared_dummy_bvec;

                    if (is_ok == false)
                    {
                        goto end;
                    }

                    /* An argument the replay is not able to make up: the call is left out. */
                    if (in_context_ptr->is_skipped() )
                    {
                        result = true;

                        goto end;
                    }

                    holder.call(reinterpret_cast<FunctionPtr>(in_function_ptr),
                                std::get<Indices>(restorers).get()...);

                    {
                        const int finished_dummy_ivec[] = {0, (std::get<Indices>(restorers).finish(in_context_ptr,
                                                                                                   Indices,
                                                                                                   true), 0)...};

                        (void) finished_dummy_ivec;
                    }

                    in_context_ptr->set_live_return(holder.to_u64() );

                    result = true;

                end:
                    return result;
                }
            };
        }
    }
}

#endif /* STEREOREPLAYENGINEDLL_GL_REPLAY_INVOKER_H */
