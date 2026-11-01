/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for ListenerList: listeners are called in order, a removed one is not called, and removing waits for a call in progress.
 */
#include <gtest/gtest.h>
#include "listener_list.h"
#include "test_doubles.h"
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

TEST(ListenerList, ListenersAreCalledInTheOrderTheyWereAdded)
{
    Replay::StereoReplayEngineDLL::ListenerList<Tests::CountingListener> list;
    Tests::CountingListener                                        first;
    Tests::CountingListener                                        second;
    std::vector<Tests::CountingListener*>                          called_vec;

    list.add(&first);
    list.add(&second);

    list.for_each([&](Tests::CountingListener* in_listener_ptr)
                  {
                      called_vec.push_back(in_listener_ptr);
                  });

    EXPECT_EQ(called_vec,
              (std::vector<Tests::CountingListener*>{&first, &second}) );
}

TEST(ListenerList, ARemovedListenerIsNotCalled)
{
    Replay::StereoReplayEngineDLL::ListenerList<Tests::CountingListener> list;
    Tests::CountingListener                                        listener;

    list.add   (&listener);
    list.remove(&listener);
    list.for_each([](Tests::CountingListener* in_listener_ptr)
                  {
                      ++in_listener_ptr->n_calls;
                  });

    EXPECT_EQ(listener.n_calls,
              0);
}

TEST(ListenerList, AListenerRemovedByAnEarlierCallOfTheRoundIsNotCalled)
{
    Replay::StereoReplayEngineDLL::ListenerList<Tests::CountingListener> list;
    Tests::CountingListener                                        first;
    Tests::CountingListener                                        second;

    list.add(&first);
    list.add(&second);

    list.for_each([&](Tests::CountingListener* in_listener_ptr)
                  {
                      ++in_listener_ptr->n_calls;

                      list.remove(&second);
                  });

    EXPECT_EQ(first.n_calls,
              1);
    EXPECT_EQ(second.n_calls,
              0);
}

TEST(ListenerList, RemovingWaitsForACallInProgress)
{
    Replay::StereoReplayEngineDLL::ListenerList<Tests::CountingListener> list;
    Tests::CountingListener                                        listener;
    std::promise<void>                      call_started_promise;
    std::promise<void>                      let_call_end_promise;
    std::atomic<bool>                       has_call_ended(false);
    std::atomic<bool>                       has_remove_returned(false);

    list.add(&listener);

    std::thread calling_thread([&]
                               {
                                   list.for_each([&](Tests::CountingListener*)
                                                 {
                                                     call_started_promise.set_value();
                                                     let_call_end_promise.get_future().wait();

                                                     has_call_ended = true;
                                                 });
                               });

    call_started_promise.get_future().wait();

    std::thread removing_thread([&]
                                {
                                    list.remove(&listener);

                                    has_remove_returned = true;
                                });

    /* Long enough for a remove which does not wait to be through. */
    std::this_thread::sleep_for(std::chrono::milliseconds(100) );

    EXPECT_FALSE(has_remove_returned);

    let_call_end_promise.set_value();

    calling_thread.join();
    removing_thread.join();

    EXPECT_TRUE(has_call_ended);
    EXPECT_TRUE(has_remove_returned);
}
