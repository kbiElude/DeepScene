/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of what every hooked call goes through: the registry of the callbacks and the gate that the dumper closes. Each is reached by a pointer that is
 * made the first time it is asked for, and every thread must get the same one.
 */
#include <gtest/gtest.h>
#include "common/api_call_gate.h"
#include "common/callback_registry.h"
#include "common/callbacks.h"
#include <atomic>
#include <thread>
#include <vector>

namespace
{
    std::atomic<uint32_t> g_n_pre_calls = {0};

    void count_pre_call(APIInterceptor::APIFunction,
                        uint32_t,
                        const APIInterceptor::APIFunctionArgument*,
                        void*,
                        bool*)
    {
        g_n_pre_calls++;
    }
}

TEST(CallbackDispatch, EveryThreadGetsTheSameRegistryAndTheSameGate)
{
    constexpr uint32_t N_THREADS = 8;

    std::vector<APIInterceptor::APICallGate*>    gate_ptr_vec   (N_THREADS, nullptr);
    std::vector<APIInterceptor::CallbackRegistry*> registry_ptr_vec(N_THREADS, nullptr);
    std::vector<std::thread>                     thread_vec;

    for (uint32_t n_thread = 0;
                  n_thread < N_THREADS;
                ++n_thread)
    {
        const auto thread_func = [&, n_thread]()
        {
            gate_ptr_vec.at    (n_thread) = APIInterceptor::APICallGate::get_ptr();
            registry_ptr_vec.at(n_thread) = APIInterceptor::CallbackRegistry::get_ptr();
        };

        thread_vec.emplace_back(thread_func);
    }

    for (auto& thread : thread_vec)
    {
        thread.join();
    }

    for (uint32_t n_thread = 0;
                  n_thread < N_THREADS;
                ++n_thread)
    {
        EXPECT_NE(gate_ptr_vec.at(n_thread),
                  nullptr);
        EXPECT_EQ(gate_ptr_vec.at(n_thread),
                  APIInterceptor::APICallGate::get_ptr() );
        EXPECT_EQ(registry_ptr_vec.at(n_thread),
                  APIInterceptor::CallbackRegistry::get_ptr() );
    }
}

TEST(CallbackDispatch, ARegisteredCallbackIsFoundForItsFunctionOnly)
{
    APIInterceptor::PFNPRECALLBACKFUNCPROC callback_func_ptr = nullptr;
    void*                                  user_arg_ptr      = nullptr;
    int                                    marker            = 0;

    APIInterceptor::register_for_pre_callback(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                              count_pre_call,
                                             &marker);

    EXPECT_TRUE(APIInterceptor::get_pre_callback_for_function(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                                             &callback_func_ptr,
                                                             &user_arg_ptr) );
    EXPECT_EQ  (callback_func_ptr,
                &count_pre_call);
    EXPECT_EQ  (user_arg_ptr,
               &marker);

    EXPECT_FALSE(APIInterceptor::get_pre_callback_for_function(APIInterceptor::APIFUNCTION_GL_GLDISABLE,
                                                              &callback_func_ptr,
                                                              &user_arg_ptr) );

    /* Callbacks stop for the thread that asks, and for it only. */
    bool is_found_on_other_thread  = false;
    bool is_found_after_disabling  = true;

    const auto thread_func = [&]()
    {
        APIInterceptor::PFNPRECALLBACKFUNCPROC other_func_ptr = nullptr;
        void*                                  other_arg_ptr  = nullptr;

        is_found_on_other_thread = APIInterceptor::get_pre_callback_for_function(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                                                                &other_func_ptr,
                                                                                &other_arg_ptr);

        APIInterceptor::disable_callbacks_for_this_thread();

        is_found_after_disabling = APIInterceptor::get_pre_callback_for_function(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                                                                &other_func_ptr,
                                                                                &other_arg_ptr);
    };

    std::thread thread(thread_func);

    thread.join();

    EXPECT_TRUE (is_found_on_other_thread);
    EXPECT_FALSE(is_found_after_disabling);

    EXPECT_TRUE(APIInterceptor::get_pre_callback_for_function(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                                             &callback_func_ptr,
                                                             &user_arg_ptr) );

    APIInterceptor::register_for_pre_callback(APIInterceptor::APIFUNCTION_GL_GLENABLE,
                                              nullptr,
                                              nullptr);
}

TEST(CallbackDispatch, TheGateWaitsForAThreadInsideACallAndOpensAgain)
{
    std::atomic<bool> is_inside = {false};
    std::atomic<bool> may_leave = {false};

    const auto thread_func = [&]()
    {
        APIInterceptor::on_gate_scope_entered();

        is_inside = true;

        while (may_leave.load() == false)
        {
            std::this_thread::yield();
        }

        APIInterceptor::on_gate_scope_left();
    };

    std::thread thread(thread_func);

    while (is_inside.load() == false)
    {
        std::this_thread::yield();
    }

    /* A thread is inside: the gate cannot be quiesced, and stays open. */
    EXPECT_FALSE(APIInterceptor::try_quiesce_api_calls(50) );

    may_leave = true;

    thread.join();

    /* Nobody is inside: it can, and then holds back entries until it is resumed. */
    EXPECT_TRUE(APIInterceptor::try_quiesce_api_calls(1000) );

    APIInterceptor::resume_api_calls();

    APIInterceptor::on_gate_scope_entered();
    APIInterceptor::on_gate_scope_left   ();
}
