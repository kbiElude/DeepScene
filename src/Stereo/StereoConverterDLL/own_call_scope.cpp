/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "own_call_scope.h"

Stereo::StereoConverterDLL::OwnCallScope::OwnCallScope()
{
    get_depth_ref()++;
}

Stereo::StereoConverterDLL::OwnCallScope::~OwnCallScope()
{
    get_depth_ref()--;
}

uint32_t& Stereo::StereoConverterDLL::OwnCallScope::get_depth_ref()
{
    static thread_local uint32_t depth = 0;

    return depth;
}

bool Stereo::StereoConverterDLL::OwnCallScope::is_active()
{
    return (get_depth_ref() != 0);
}
