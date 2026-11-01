/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREOREPLAYENGINEDLL_OBJECT_REMAPPER_H
#define STEREOREPLAYENGINEDLL_OBJECT_REMAPPER_H

#include "types.h"
#include <map>
#include <stdint.h>
#include <utility>

namespace Replay
{
    namespace StereoReplayEngineDLL
    {
        /* Maps the handles and IDs which the dumped process got from the APIs to the ones that the replay gets. Zero stands
         * for itself. Used by the replay thread only.
         */
        class ObjectRemapper
        {
        public:
            ObjectRemapper();

            void add   (const ObjectKind& in_kind,
                        const uint64_t&   in_dumped_value,
                        const uint64_t&   in_live_value);
            void clear ();
            bool get   (const ObjectKind& in_kind,
                        const uint64_t&   in_dumped_value,
                        uint64_t*         out_live_value_ptr) const;
            void remove(const ObjectKind& in_kind,
                        const uint64_t&   in_dumped_value);

        private:
            std::map<std::pair<ObjectKind, uint64_t>, uint64_t> m_live_value_map;
        };
    }
}

#endif /* STEREOREPLAYENGINEDLL_OBJECT_REMAPPER_H */
