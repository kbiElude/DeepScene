/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "scoped_tick_adder.h"
#include <windows.h>

Stereo::ConverterDLL::ScopedTickAdder::ScopedTickAdder(std::atomic<int64_t>* inout_total_ptr,
                                                       const bool&           in_is_enabled)
    :m_is_enabled (in_is_enabled),
     m_start_ticks(0),
     m_total_ptr  (inout_total_ptr)
{
    if (m_is_enabled)
    {
        LARGE_INTEGER start;

        ::QueryPerformanceCounter(&start);

        m_start_ticks = start.QuadPart;
    }
}

Stereo::ConverterDLL::ScopedTickAdder::~ScopedTickAdder()
{
    if (m_is_enabled)
    {
        LARGE_INTEGER end;

        ::QueryPerformanceCounter(&end);

        *m_total_ptr += end.QuadPart - m_start_ticks;
    }
}
