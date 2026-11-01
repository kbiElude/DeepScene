/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "i_call_executor.h"

void Stereo::Replay::EngineDLL::ICallExecutor::set_windows_visible(const bool& in_are_visible)
{
    (void) in_are_visible;
}

void Stereo::Replay::EngineDLL::ICallExecutor::shutdown()
{
    reset();
}
