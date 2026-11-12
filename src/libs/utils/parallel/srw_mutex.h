/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A mutex that costs little to take when no other thread has it.
 */
#ifndef UTILS_PARALLEL_SRW_MUTEX_H
#define UTILS_PARALLEL_SRW_MUTEX_H

namespace Utils
{
    namespace Parallel
    {
        /* What std::mutex is for the code that takes it (lock, try_lock, unlock, so that a std::lock_guard of it works): not recursive, and not to be unlocked by a thread that
         * does not have it. It is the slim reader-writer lock of the system, taken for writing: taking it is one atomic operation and no more, where std::mutex of the library of
         * the compiler asks which thread has it (so that it can tell a thread that takes it twice) and takes a critical section. It is for a lock that is taken for a few
         * instructions of work and very many times, as the one of a callback is.
         */
        class SrwMutex
        {
        public:
            SrwMutex();

            SrwMutex           (const SrwMutex&) = delete;
            SrwMutex& operator=(const SrwMutex&) = delete;

            void lock    ();
            bool try_lock();
            void unlock  ();

        private:
            void* m_lock_ptr; /* the SRWLOCK, which is a pointer in size (kept so, so that this header needs no header of the system) */
        };
    }
}

#endif /* UTILS_PARALLEL_SRW_MUTEX_H */
