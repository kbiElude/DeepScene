/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "null_call_executor.h"

Stereo::Replay::EngineDLL::NullCallExecutor::NullCallExecutor()
{
    /* Stub */
}

bool Stereo::Replay::EngineDLL::NullCallExecutor::execute(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                          const IChunkSource&                          in_chunk_source,
                                                          std::string*                                 out_error_ptr)
{
    (void) in_call;
    (void) in_chunk_source;
    (void) out_error_ptr;

    return true;
}

void Stereo::Replay::EngineDLL::NullCallExecutor::on_frame_presented()
{
    /* Stub */
}

void Stereo::Replay::EngineDLL::NullCallExecutor::reset()
{
    /* Stub */
}
