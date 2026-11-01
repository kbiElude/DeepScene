/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "own_call_scope.h"

Stereo::ConverterDLL::OwnCallScope::OwnCallScope()
{
    get_depth_ref()++;
}

Stereo::ConverterDLL::OwnCallScope::~OwnCallScope()
{
    get_depth_ref()--;
}

uint32_t& Stereo::ConverterDLL::OwnCallScope::get_depth_ref()
{
    static thread_local uint32_t depth = 0;

    return depth;
}

bool Stereo::ConverterDLL::OwnCallScope::is_active()
{
    return (get_depth_ref() != 0);
}
