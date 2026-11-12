/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for SrwMutex: it keeps threads out of each other's way as std::mutex does, and works with std::lock_guard.
 */
#include <gtest/gtest.h>
#include "utils/parallel/srw_mutex.h"
#include <mutex>
#include <thread>
#include <vector>

TEST(SrwMutex, ThreadsThatTakeItDoNotRunTogether)
{
    constexpr int N_TURNS_PER_THREAD = 50000;
    constexpr int N_THREADS          = 6;

    Utils::Parallel::SrwMutex mutex;
    int64_t                   counter = 0;
    std::vector<std::thread>  thread_vec;

    for (int n_thread = 0;
             n_thread < N_THREADS;
           ++n_thread)
    {
        thread_vec.emplace_back([&]()
        {
            for (int n_turn = 0;
                     n_turn < N_TURNS_PER_THREAD;
                   ++n_turn)
            {
                std::lock_guard<Utils::Parallel::SrwMutex> lock(mutex);

                counter++;
            }
        });
    }

    for (auto& thread : thread_vec)
    {
        thread.join();
    }

    /* A counter that is not guarded loses turns. */
    EXPECT_EQ(counter,
              static_cast<int64_t>(N_THREADS) * N_TURNS_PER_THREAD);
}

TEST(SrwMutex, ItCannotBeTakenWhileSomeoneHasItAndCanBeTakenAgainAfter)
{
    Utils::Parallel::SrwMutex mutex;

    mutex.lock();

    /* Another thread (a thread that has it asking again is not allowed: it is not recursive). */
    bool was_taken_by_other = true;

    std::thread([&]()
    {
        was_taken_by_other = mutex.try_lock();
    }).join();

    EXPECT_FALSE(was_taken_by_other);

    mutex.unlock();

    EXPECT_TRUE(mutex.try_lock() );

    mutex.unlock();
}
