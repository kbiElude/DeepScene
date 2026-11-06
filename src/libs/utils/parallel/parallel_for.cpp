/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "utils/parallel/parallel_for.h"
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

void Utils::Parallel::ParallelFor::run(const uint64_t&  in_n_items,
                                       const uint32_t&  in_n_threads,
                                       const RangeFunc& in_func)
{
    /* Small enough for the work to be shared out evenly, large enough for the threads not to meet at the counter all the time. */
    constexpr uint64_t N_RANGES_PER_THREAD = 16;

    const uint64_t n_threads        = std::max<uint64_t>(1,
                                                         std::min<uint64_t>(in_n_threads,
                                                                            in_n_items) );
    const uint64_t n_items_in_range = std::max<uint64_t>(1,
                                                         in_n_items / (n_threads * N_RANGES_PER_THREAD) );
    std::atomic<uint64_t> next_item(0);
    std::vector<std::thread> thread_vec;

    const auto work_func = [&]()
    {
        for (;;)
        {
            const uint64_t first_item = next_item.fetch_add(n_items_in_range);

            if (first_item >= in_n_items)
            {
                break;
            }

            in_func(first_item,
                    std::min<uint64_t>(first_item + n_items_in_range,
                                       in_n_items) );
        }
    };

    for (uint64_t n_thread = 1;
                  n_thread < n_threads;
                ++n_thread)
    {
        thread_vec.emplace_back(work_func);
    }

    work_func();

    for (auto& thread : thread_vec)
    {
        thread.join();
    }
}
