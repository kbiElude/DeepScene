/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A thread that does one job at a time, again and again, for a thread that hands them over: the owner queues a job, goes on with something else, and waits for the
 * thread when it needs the job to be done. What the job needs (its input, where its output goes) is the owner's: the thread only runs the function it was made with.
 */
#ifndef UTILS_PARALLEL_JOB_THREAD_H
#define UTILS_PARALLEL_JOB_THREAD_H

#include "utils/parallel/types.h"
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>

namespace Utils
{
    namespace Parallel
    {
        class JobThread
        {
        public:
            /* What the thread does for each job. It is called with no lock taken. */
            typedef std::function<void()> WorkFunc;

            /* A step that the thread-safe functions run with the lock of the thread taken. */
            typedef std::function<void()> LockedFunc;

            /* Starts the thread, which has nothing to do until a job is queued. */
            explicit JobThread(const WorkFunc& in_work_func);

            /* Ends the thread, after the job that it is doing, if it is doing one. */
            ~JobThread();

            JobThread           (const JobThread&) = delete;
            JobThread& operator=(const JobThread&) = delete;

            /* Hands the thread a job. The step is run with the lock taken, before the thread is woken: it is where what the job needs is put where the work function
             * reads it. The owner must have waited for the job before, since the thread does one at a time.
             */
            void queue(const LockedFunc& in_prepare_func);

            /* Ends the thread, after the job that it is doing, if it is doing one; no job is run after that. Nothing happens if it is ended already. */
            void stop();

            /* For an owner that does not wait for the jobs: hands the thread a job as queue() does if it is not at one (a job that is done and not waited for does not
             * count), and says whether it did. False, and the step is not run, if a job is queued or running.
             */
            bool try_queue(const LockedFunc& in_prepare_func);

            /* Waits until the thread is through with the job that was queued last (at once, if it has none). True if there was one: the step is then run with the lock
             * taken, which is where the owner takes what the job made, and the thread is ready for the next job.
             */
            bool wait(const LockedFunc& in_collect_func);

        private:
            /* Queues the job, if the thread is free or the job is wanted whatever the thread is at; false if it was not queued. */
            bool queue_job(const bool&       in_only_if_free,
                           const LockedFunc& in_prepare_func);

            void run_thread();

            std::condition_variable m_condition;      /* wakes the thread                       */
            std::condition_variable m_done_condition; /* wakes the owner when the job is done   */
            bool                    m_is_stopping;
            std::mutex              m_mutex;          /* guards the state, and what the steps do */
            JobState                m_state;
            std::thread             m_thread;
            WorkFunc                m_work_func;
        };
    }
}

#endif /* UTILS_PARALLEL_JOB_THREAD_H */
