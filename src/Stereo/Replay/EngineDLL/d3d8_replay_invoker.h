/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Calls a D3D8 method of an object of the replay with the arguments that the dump holds, whatever the method is: the
 * invoker is generated from the method's function pointer type (see D3D8/generic_hook.h, which does the opposite).
 */
#ifndef STEREO_REPLAY_ENGINEDLL_D3D8_REPLAY_INVOKER_H
#define STEREO_REPLAY_ENGINEDLL_D3D8_REPLAY_INVOKER_H

#include "d3d8_argument_restorer.h"
#include <stdio.h>
#include <tuple>
#include <utility>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            namespace D3D8Replay
            {
                /* What a method returns tells whether the call worked. */
                template<typename ReturnType>
                struct ReturnTraits
                {
                    static bool is_success(const ReturnType& in_value)
                    {
                        (void) in_value;

                        return true;
                    }

                    static long to_number(const ReturnType& in_value)
                    {
                        (void) in_value;

                        return 0;
                    }
                };

                /* HRESULT. */
                template<>
                struct ReturnTraits<long>
                {
                    static bool is_success(const long& in_value)
                    {
                        return (in_value >= 0);
                    }

                    static long to_number(const long& in_value)
                    {
                        return in_value;
                    }
                };

                /* Calls the method, and keeps its value. */
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

                    bool is_success() const
                    {
                        return ReturnTraits<ReturnType>::is_success(value);
                    }

                    long to_number() const
                    {
                        return ReturnTraits<ReturnType>::to_number(value);
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

                    bool is_success() const
                    {
                        return true;
                    }

                    long to_number() const
                    {
                        return 0;
                    }
                };

                template<typename FunctionPtr, uint32_t vtable_index>
                struct Invoker;

                template<typename ReturnType, typename... Arguments, uint32_t vtable_index>
                struct Invoker<ReturnType (__stdcall*)(Arguments...), vtable_index>
                {
                    typedef ReturnType (__stdcall* MethodPtr)(Arguments...);

                    static constexpr size_t get_n_arguments()
                    {
                        return sizeof...(Arguments);
                    }

                    /* The context holds the call (see D3D8ReplayContext::begin_call()). */
                    static bool invoke(D3D8ReplayContext* in_context_ptr,
                                       std::string*       out_error_ptr)
                    {
                        return invoke_impl(in_context_ptr,
                                           out_error_ptr,
                                           std::index_sequence_for<Arguments...>() );
                    }

                private:
                    template<size_t... Indices>
                    static bool invoke_impl(D3D8ReplayContext* in_context_ptr,
                                            std::string*       out_error_ptr,
                                            std::index_sequence<Indices...>)
                    {
                        std::tuple<D3D8Restore::ArgumentRestorer<Arguments, Interception::Core::D3D8Utils::ArgumentCategoryOf<Arguments>::value>...> restorers;
                        bool                                                                                                                         is_ok  = true;
                        ReturnHolder<ReturnType>                                                                                                     holder;
                        bool                                                                                                                         result = false;

                        /* Left to right, and no further once an argument cannot be restored. */
                        const bool prepared_dummy_bvec[] = {true, (is_ok = (is_ok && std::get<Indices>(restorers).prepare(in_context_ptr,
                                                                                                                            Indices,
                                                                                                                            out_error_ptr) ))...};

                        (void) prepared_dummy_bvec;

                        if (is_ok == false)
                        {
                            goto end;
                        }

                        {
                            /* The object is the first argument. */
                            void* const  object_ptr = std::get<0>(restorers).get();
                            void** const vtable_ptr = *reinterpret_cast<void***>(object_ptr);
                            const auto   method_ptr = reinterpret_cast<MethodPtr>(vtable_ptr[vtable_index]);

                            holder.call(method_ptr,
                                        std::get<Indices>(restorers).get()...);
                        }

                        {
                            const bool is_success            = holder.is_success();
                            const int  finished_dummy_ivec[] = {0, (std::get<Indices>(restorers).finish(in_context_ptr,
                                                                                                        Indices,
                                                                                                        is_success), 0)...};

                            (void) finished_dummy_ivec;
                        }

                        /* The dump got its object; so should the replay. */
                        {
                            const bool has_failed = (holder.is_success() == false);

                            if (has_failed)
                            {
                                const bool is_creation_expected = in_context_ptr->is_creation_expected();

                                if (is_creation_expected)
                                {
                                    char message_i8vec96[96];

                                    ::snprintf(message_i8vec96,
                                               sizeof(message_i8vec96),
                                               "A D3D8 method which creates an object failed (%08X).",
                                               static_cast<unsigned int>(holder.to_number() ));

                                    *out_error_ptr = message_i8vec96;

                                    goto end;
                                }
                            }
                        }

                        result = true;

                    end:
                        return result;
                    }
                };
            }
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_D3D8_REPLAY_INVOKER_H */
