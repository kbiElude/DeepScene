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
        /* What the host has to say goes in the log of the session, which is where the converter in the application says what it does. */
        class PresenterLog
        {
        public:
            /* Adds a line to the file, which is kept open (opening and closing it for each line is slow, when something watches files).
             * Nothing is said if the file cannot be opened: there is nobody to say it to. Any thread.
             */
            static void write_line(const std::string& in_path,
                                   const std::string& in_text);

        private:
            /* The file that was written to last. */
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
