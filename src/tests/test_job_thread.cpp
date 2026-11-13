/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the thread that does one job at a time for the thread that hands them over.
 */
#include <gtest/gtest.h>
#include "utils/parallel/job_thread.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

TEST(JobThread, AJobIsRunAndWaitSaysThereWasOneOnlyOnce)
{
    std::atomic<int> n_runs(0);

    const auto work_func = [&n_runs]()
    {
        n_runs++;
    };

    Utils::Parallel::JobThread job_thread(work_func);

    job_thread.queue(Utils::Parallel::JobThread::LockedFunc() );

    EXPECT_TRUE (job_thread.wait(Utils::Parallel::JobThread::LockedFunc() ) );
    EXPECT_EQ   (n_runs.load(),
                 1);
    EXPECT_FALSE(job_thread.wait(Utils::Parallel::JobThread::LockedFunc() ) );
    EXPECT_EQ   (n_runs.load(),
                 1);
}

TEST(JobThread, WaitingWhenNothingWasQueuedReturnsAtOnce)
{
    const auto work_func = []()
    {
        FAIL() << "No job was queued.";
    };

    Utils::Parallel::JobThread job_thread(work_func);

    EXPECT_FALSE(job_thread.wait(Utils::Parallel::JobThread::LockedFunc() ) );
}

TEST(JobThread, TheStepsRunInOrderPrepareThenTheJobThenCollect)
{
    std::vector<int> step_vec;
    int              input = 0;

    const auto work_func = [&]()
    {
        step_vec.push_back(input * 10);
    };

    const auto prepare_func = [&]()
    {
        input = 4;

        step_vec.push_back(1);
    };

    const auto collect_func = [&]()
    {
        step_vec.push_back(2);
    };

    Utils::Parallel::JobThread job_thread(work_func);

    job_thread.queue(prepare_func);

    ASSERT_TRUE(job_thread.wait(collect_func) );

    EXPECT_EQ(step_vec,
              (std::vector<int>{1, 40, 2}) );
}

TEST(JobThread, JobsOneAfterTheOtherAreEachRunOnce)
{
    constexpr int N_JOBS = 200;

    int              input = 0;
    std::vector<int> result_vec;

    const auto work_func = [&]()
    {
        result_vec.push_back(input);
    };

    Utils::Parallel::JobThread job_thread(work_func);

    for (int n_job = 0;
             n_job < N_JOBS;
           ++n_job)
    {
        const auto prepare_func = [&input, n_job]()
        {
            input = n_job;
        };

        job_thread.queue(prepare_func);

        ASSERT_TRUE(job_thread.wait(Utils::Parallel::JobThread::LockedFunc() ) );
    }

    ASSERT_EQ(result_vec.size(),
              static_cast<size_t>(N_JOBS) );

    for (int n_job = 0;
             n_job < N_JOBS;
           ++n_job)
    {
        EXPECT_EQ(result_vec.at(n_job),
                  n_job);
    }
}

TEST(JobThread, StoppingWaitsForTheJobThatIsRunningAndMoreThanOneStopIsAllowed)
{
    std::atomic<bool> is_done(false);

    const auto work_func = [&is_done]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100) );

        is_done = true;
    };

    Utils::Parallel::JobThread job_thread(work_func);

    job_thread.queue(Utils::Parallel::JobThread::LockedFunc() );

    /* The job takes 100 ms, and the thread is in it long before this wakes. */
    std::this_thread::sleep_for(std::chrono::milliseconds(20) );

    job_thread.stop();
    job_thread.stop();

    EXPECT_TRUE(is_done.load() );
}

TEST(JobThread, AThreadThatHasNoJobEndsWhenItIsDestroyed)
{
    const auto work_func = []()
    {
        FAIL() << "No job was queued.";
    };

    {
        Utils::Parallel::JobThread job_thread(work_func);
    }

    SUCCEED();
}
