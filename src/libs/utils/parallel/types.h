/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_PARALLEL_TYPES_H
#define UTILS_PARALLEL_TYPES_H

namespace Utils
{
    namespace Parallel
    {
        /* Where the job of a JobThread is. */
        enum class JobState
        {
            DONE,    /* the thread is through with it, and nobody has taken the result */
            IDLE,    /* there is none                                                  */
            QUEUED,  /* waiting for the thread                                         */
            RUNNING
        };
    }
}

#endif /* UTILS_PARALLEL_TYPES_H */
