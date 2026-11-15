/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/parallel/srw_mutex.h"
#include <windows.h>

static_assert(sizeof(SRWLOCK) == sizeof(void*),
              "SrwMutex keeps the lock where a pointer is");

Utils::Parallel::SrwMutex::SrwMutex()
    :m_lock_ptr(nullptr)
{
    /* An SRWLOCK is initialized to all zero (SRWLOCK_INIT). */
}

void Utils::Parallel::SrwMutex::lock()
{
    ::AcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&m_lock_ptr) );
}

bool Utils::Parallel::SrwMutex::try_lock()
{
    return (::TryAcquireSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&m_lock_ptr) ) != FALSE);
}

void Utils::Parallel::SrwMutex::unlock()
{
    ::ReleaseSRWLockExclusive(reinterpret_cast<PSRWLOCK>(&m_lock_ptr) );
}
