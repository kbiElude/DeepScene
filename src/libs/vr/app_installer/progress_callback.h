/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef APPINSTALLER_PROGRESS_CALLBACK_H
#define APPINSTALLER_PROGRESS_CALLBACK_H

#include <functional>
#include <stdint.h>

namespace VR
{
    namespace AppInstaller
    {
        /* Told how much of a job is done, and how much it is (0 if that is not known). */
        typedef std::function<void(const uint64_t& in_n_bytes_done,
                                   const uint64_t& in_n_bytes_total)> ProgressCallback;
    }
}

#endif /* APPINSTALLER_PROGRESS_CALLBACK_H */
