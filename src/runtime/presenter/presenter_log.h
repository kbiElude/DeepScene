/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_PRESENTER_PRESENTER_LOG_H
#define STEREO_PRESENTER_PRESENTER_LOG_H

#include <mutex>
#include <string>
#include <Windows.h>

namespace Stereo
{
    namespace Presenter
    {
        /* The messages of the presenter go to the log of the session, the same log in which the converter inside the application records what it does. */
        class PresenterLog
        {
        public:
            /* Appends a line to the file, which is kept open (reopening it for every line is slow when something watches the files). Nothing is
             * written if the file cannot be opened, because there is nobody to report that to. Callable from any thread.
             */
            static void write_line(const std::string& in_path,
                                   const std::string& in_text);

        private:
            /* The file that was written to most recently. */
            struct State
            {
                HANDLE      file = INVALID_HANDLE_VALUE;
                std::mutex  mutex;
                std::string path;
            };

            static State& get_state();
        };
    }
}

#endif /* STEREO_PRESENTER_PRESENTER_LOG_H */
