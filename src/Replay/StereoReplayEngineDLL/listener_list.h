/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_LISTENER_LIST_H
#define STEREOREPLAYENGINEDLL_LISTENER_LIST_H

#include <algorithm>
#include <mutex>
#include <vector>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* The listeners of a thread which calls them. A listener that has been removed is not being called any more: remove() waits
         * for a call in progress, so the listener can be destroyed once it returns. A listener may remove itself, or another one,
         * from a call.
         */
        template<typename Listener>
        class ListenerList
        {
        public:
            void add(Listener* in_listener_ptr)
            {
                std::lock_guard<std::mutex> lock(m_mutex);

                m_listener_ptr_vec.push_back(in_listener_ptr);
            }

            /* Calls the function with each listener, one after the other, on the calling thread. */
            template<typename Func>
            void for_each(const Func& in_func)
            {
                std::lock_guard<std::recursive_mutex> dispatch_lock(m_dispatch_mutex);

                std::vector<Listener*> listener_ptr_vec;

                {
                    std::lock_guard<std::mutex> lock(m_mutex);

                    listener_ptr_vec = m_listener_ptr_vec;
                }

                for (auto listener_ptr : listener_ptr_vec)
                {
                    /* One removed by an earlier call of this round is not called. */
                    if (is_listed(listener_ptr) )
                    {
                        in_func(listener_ptr);
                    }
                }
            }

            void remove(Listener* in_listener_ptr)
            {
                std::lock_guard<std::recursive_mutex> dispatch_lock(m_dispatch_mutex);

                {
                    std::lock_guard<std::mutex> lock(m_mutex);

                    m_listener_ptr_vec.erase(std::remove           (m_listener_ptr_vec.begin(),
                                                                    m_listener_ptr_vec.end  (),
                                                                    in_listener_ptr),
                                             m_listener_ptr_vec.end() );
                }
            }

        private:
            bool is_listed(Listener* in_listener_ptr)
            {
                std::lock_guard<std::mutex> lock(m_mutex);

                return (std::find(m_listener_ptr_vec.begin(),
                                  m_listener_ptr_vec.end  (),
                                  in_listener_ptr) != m_listener_ptr_vec.end() );
            }

            /* Lock order: the dispatch mutex, then the other one. */
            std::recursive_mutex   m_dispatch_mutex; /* held while the listeners are called */
            std::vector<Listener*> m_listener_ptr_vec;
            std::mutex             m_mutex;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_LISTENER_LIST_H */
