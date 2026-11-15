/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * A mutex that is cheap to lock when no other thread holds it.
 */
#ifndef UTILS_PARALLEL_SRW_MUTEX_H
#define UTILS_PARALLEL_SRW_MUTEX_H

namespace Utils
{
    namespace Parallel
    {
        /* A drop-in replacement for std::mutex: it has lock(), try_lock() and unlock(), so std::lock_guard works with it. Like std::mutex it is not recursive,
         * and only the thread that locked it may unlock it.
         *
         * It wraps the Windows slim reader-writer lock in exclusive mode, so locking is a single atomic operation. The std::mutex of the MSVC library also
         * records the owning thread (to detect a thread that locks twice) and takes a critical section.
         *
         * Use it for locks that are held for a few instructions and taken very often, such as the lock of a callback.
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
            void* m_lock_ptr; /* The SRWLOCK. It is stored as a pointer so that this header does not need the Windows headers. */
        };
    }
}

#endif /* UTILS_PARALLEL_SRW_MUTEX_H */
