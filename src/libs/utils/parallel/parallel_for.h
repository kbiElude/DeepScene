/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Runs the same work for many items on several threads.
 */
#ifndef UTILS_PARALLEL_PARALLEL_FOR_H
#define UTILS_PARALLEL_PARALLEL_FOR_H

#include <functional>
#include <stdint.h>

namespace Utils
{
    namespace Parallel
    {
        class ParallelFor
        {
        public:
            /* Processes the items from in_first_item up to, but not including, in_end_item. */
            typedef std::function<void(const uint64_t& in_first_item, const uint64_t& in_end_item)> RangeFunc;

            /* Calls the function for every item 0 .. n_items - 1. The items are split into ranges, and each thread takes a new range when it finishes
             * its previous one, so a slow item does not hold up the rest. The calling thread is one of the threads. With one thread, or with few
             * items, no other thread is used. The function is called once for each item, by one thread, and must not depend on the order of the items.
             */
            static void run(const uint64_t&  in_n_items,
                            const uint32_t&  in_n_threads,
                            const RangeFunc& in_func);
        };
    }
}

#endif /* UTILS_PARALLEL_PARALLEL_FOR_H */
