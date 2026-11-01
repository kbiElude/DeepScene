/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYPLAYER_QUIT_HANDLER_H
#define STEREOREPLAYPLAYER_QUIT_HANDLER_H

#include <condition_variable>
#include <functional>
#include <mutex>

namespace Replay
{
    namespace StereoReplayPlayer
    {
        /* Ends the player: any thread may ask (a window of the replay or the preview is closed, the user quits), and the main thread
         * either waits for the request, or has a target which is called to end what it is blocked in.
         */
        class QuitHandler
        {
        public:
            typedef std::function<void()> TargetFunc;

            QuitHandler();

            bool is_requested() const;

            /* Any thread. */
            void request();

            /* The target is called, with the lock held, by every request, and at once if one was made already, so it must be quick and
             * must not ask for a quit. An empty target is taken away. It must be taken away before what it uses is gone.
             */
            void set_target(const TargetFunc& in_target_func);

            void wait();

        private:
            /* Lock order: no other lock is taken while it is held. */
            struct State
            {
                std::condition_variable cv;
                bool                    is_requested = false;
                std::mutex              mutex;
                TargetFunc              target_func;
            };

            mutable State m_state;
        };
    }
}

#endif /* STEREOREPLAYPLAYER_QUIT_HANDLER_H */
