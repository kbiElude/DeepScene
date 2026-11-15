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
        /* The state of the job of a JobThread. */
        enum class JobState
        {
            DONE,    /* the thread has finished it, and nobody has taken the result    */
            IDLE,    /* there is no job                                                */
            QUEUED,  /* waiting for the thread                                         */
            RUNNING
        };
    }
}

#endif /* UTILS_PARALLEL_TYPES_H */
