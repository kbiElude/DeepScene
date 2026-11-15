/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_PLAYER_QUIT_HANDLER_H
#define STEREO_REPLAY_PLAYER_QUIT_HANDLER_H

#include <condition_variable>
#include <functional>
#include <mutex>

namespace Stereo
{
    namespace Replay
    {
        namespace Player
        {
            /* Ends the player. Any thread may request the quit (a replay or preview window is closed, or the user quits). The main
             * thread either waits for the request, or registers a target function that ends whatever the main thread is blocked in.
             */
            class QuitHandler
            {
            public:
                typedef std::function<void()> TargetFunc;

                QuitHandler();

                bool is_requested() const;

                /* Any thread. */
                void request();

                /* The target function is called, with the lock held, on every request, and immediately if a request was already made.
                 * It must therefore be quick and must not call request(). Passing an empty function removes the target. Remove the
                 * target before anything it uses is destroyed.
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
}

#endif /* STEREO_REPLAY_PLAYER_QUIT_HANDLER_H */
