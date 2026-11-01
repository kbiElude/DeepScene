/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_REPLAY_ENGINEDLL_LIVE_OBJECT_LIST_H
#define STEREO_REPLAY_ENGINEDLL_LIVE_OBJECT_LIST_H

#include <map>
#include <stdint.h>
#include <vector>

namespace Stereo
{
    namespace Replay
    {
        namespace EngineDLL
        {
            /* The objects which the replay has made and not released, in the order they were first added. */
            class LiveObjectList
            {
            public:
                LiveObjectList();

                /* An object which is on the list already keeps its place. */
                void add(void* in_object_ptr);

                void clear();

                /* The newest first: what depends on an object (the device is the first object) is released before it. */
                std::vector<void*> get_newest_first_vec() const;

                void remove(void* in_object_ptr);

            private:
                std::map<void*, uint64_t> m_order_map; /* the number of objects added before the object */
                uint64_t                  m_n_added;
            };
        }
    }
}

#endif /* STEREO_REPLAY_ENGINEDLL_LIVE_OBJECT_LIST_H */
