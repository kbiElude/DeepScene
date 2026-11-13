/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#ifndef STEREO_CONVERTERDLL_SCOPED_TICK_ADDER_H
#define STEREO_CONVERTERDLL_SCOPED_TICK_ADDER_H

#include <atomic>
#include <stdint.h>

namespace Stereo
{
    namespace ConverterDLL
    {
        /* Adds the ticks of the performance counter that pass during its life to a total, when it ends. A disabled one reads no counter and adds nothing, so that a scope
         * which only sometimes counts (a call that may or may not be a draw) costs nothing when it does not.
         */
        class ScopedTickAdder
        {
        public:
            /* The total must outlive the adder. */
             ScopedTickAdder(std::atomic<int64_t>* inout_total_ptr,
                             const bool&           in_is_enabled);
            ~ScopedTickAdder();

            ScopedTickAdder           (const ScopedTickAdder&) = delete;
            ScopedTickAdder& operator=(const ScopedTickAdder&) = delete;

        private:
            bool                  m_is_enabled;
            int64_t               m_start_ticks;
            std::atomic<int64_t>* m_total_ptr;
        };
    }
}

#endif /* STEREO_CONVERTERDLL_SCOPED_TICK_ADDER_H */
