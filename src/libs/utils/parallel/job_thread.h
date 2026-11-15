/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A thread that runs one job at a time, repeatedly, for an owner thread that hands the jobs over. The owner queues a job, continues with other work, and waits
 * for the thread when it needs the result. What the job needs (its input, where its output goes) belongs to the owner. The thread only calls the work function
 * it was created with.
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
            /* The work done for each job. It is called without holding the lock. */
            typedef std::function<void()> WorkFunc;

            /* A step that queue(), try_queue() and wait() run while holding the thread's lock. */
            typedef std::function<void()> LockedFunc;

            /* Starts the thread. It is idle until a job is queued. */
            explicit JobThread(const WorkFunc& in_work_func);

            /* Ends the thread after its current job, if any. */
            ~JobThread();

            JobThread           (const JobThread&) = delete;
            JobThread& operator=(const JobThread&) = delete;

            /* Hands the thread a job. The step runs with the lock held, before the thread is woken. The owner uses it to put the job's input where the work
             * function reads it. The owner must have waited for the previous job, because the thread runs one job at a time.
             */
            void queue(const LockedFunc& in_prepare_func);

            /* Ends the thread after its current job, if any. No job runs after that. Does nothing if the thread has already ended. */
            void stop();

            /* For an owner that does not wait for its jobs. Hands the thread a job like queue(), but only if the thread is not busy. A finished job that
             * nobody waited for does not count as busy. Returns false, without running the step, if a job is queued or running.
             */
            bool try_queue(const LockedFunc& in_prepare_func);

            /* Waits until the thread has finished the job that was queued last (returns at once if there is none). Returns true if there was a job. The step
             * then runs with the lock held, and the owner uses it to take what the job produced. The thread is then ready for the next job.
             */
            bool wait(const LockedFunc& in_collect_func);

        private:
            /* Queues the job if the thread is free or in_only_if_free is false. Returns false if the job was not queued. */
            bool queue_job(const bool&       in_only_if_free,
                           const LockedFunc& in_prepare_func);

            void run_thread();

            std::condition_variable m_condition;      /* wakes the thread                       */
            std::condition_variable m_done_condition; /* wakes the owner when the job is done   */
            bool                    m_is_stopping;
            std::mutex              m_mutex;          /* guards the state and whatever the steps touch */
            JobState                m_state;
            std::thread             m_thread;
            WorkFunc                m_work_func;
        };
    }
}

#endif /* UTILS_PARALLEL_JOB_THREAD_H */
