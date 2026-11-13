/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/parallel/job_thread.h"

Utils::Parallel::JobThread::JobThread(const WorkFunc& in_work_func)
    :m_is_stopping(false),
     m_state      (JobState::IDLE),
     m_work_func  (in_work_func)
{
    const auto run_thread_func = [this]()
    {
        run_thread();
    };

    m_thread = std::thread(run_thread_func);
}

Utils::Parallel::JobThread::~JobThread()
{
    stop();
}

void Utils::Parallel::JobThread::queue(const LockedFunc& in_prepare_func)
{
    queue_job(false, /* in_only_if_free */
              in_prepare_func);
}

bool Utils::Parallel::JobThread::queue_job(const bool&       in_only_if_free,
                                           const LockedFunc& in_prepare_func)
{
    bool result = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        result = (in_only_if_free == false          ||
                  m_state         == JobState::IDLE ||
                  m_state         == JobState::DONE);

        if (result)
        {
            if (in_prepare_func)
            {
                in_prepare_func();
            }

            m_state = JobState::QUEUED;
        }
    }

    if (result)
    {
        m_condition.notify_one();
    }

    return result;
}

void Utils::Parallel::JobThread::run_thread()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    while (true)
    {
        const auto is_job_queued_func = [this]()
        {
            return (m_is_stopping            ||
                    m_state == JobState::QUEUED);
        };

        m_condition.wait(lock,
                         is_job_queued_func);

        if (m_is_stopping)
        {
            break;
        }

        m_state = JobState::RUNNING;

        lock.unlock();

        m_work_func();

        lock.lock();

        m_state = JobState::DONE;

        m_done_condition.notify_all();
    }
}

void Utils::Parallel::JobThread::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        m_is_stopping = true;
    }

    m_condition.notify_all();

    if (m_thread.joinable() )
    {
        m_thread.join();
    }
}

bool Utils::Parallel::JobThread::try_queue(const LockedFunc& in_prepare_func)
{
    return queue_job(true, /* in_only_if_free */
                     in_prepare_func);
}

bool Utils::Parallel::JobThread::wait(const LockedFunc& in_collect_func)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    const auto is_job_finished_func = [this]()
    {
        return (m_state == JobState::DONE ||
                m_state == JobState::IDLE);
    };

    m_done_condition.wait(lock,
                          is_job_finished_func);

    const bool result = (m_state == JobState::DONE);

    if (result)
    {
        if (in_collect_func)
        {
            in_collect_func();
        }

        m_state = JobState::IDLE;
    }

    return result;
}
