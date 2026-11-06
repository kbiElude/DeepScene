/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Runs the same work for many items on several threads.
 */
#ifndef APIDUMPDEPENDENCIES_PARALLEL_FOR_H
#define APIDUMPDEPENDENCIES_PARALLEL_FOR_H

#include <functional>
#include <stdint.h>

namespace Analysis
{
    namespace APIDumpDependencies
    {
        class ParallelFor
        {
        public:
            /* Called for the items from the first up to (not including) the end. */
            typedef std::function<void(const uint64_t& in_first_item, const uint64_t& in_end_item)> RangeFunc;

            /* Calls the function for all the items 0 .. n_items - 1, in ranges which are given to the threads as they get done with the last
             * (so that an item which takes long does not hold up the rest). The calling thread is one of them. With one thread, or few items, there
             * are no others. The function is called for each item once, by one thread, and must not need the items to be done in order.
             */
            static void run(const uint64_t&  in_n_items,
                            const uint32_t&  in_n_threads,
                            const RangeFunc& in_func);
        };
    }
}

#endif /* APIDUMPDEPENDENCIES_PARALLEL_FOR_H */
