/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef UTILS_FILES_PROGRESS_CALLBACK_H
#define UTILS_FILES_PROGRESS_CALLBACK_H

#include <functional>
#include <stdint.h>

namespace Utils
{
    namespace Files
    {
        /* Receives the progress of a job: the bytes done so far and the bytes in total (0 if the total is not known). */
        typedef std::function<void(const uint64_t& in_n_bytes_done,
                                   const uint64_t& in_n_bytes_total)> ProgressCallback;
    }
}

#endif /* UTILS_FILES_PROGRESS_CALLBACK_H */
