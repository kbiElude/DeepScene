/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 */
#include "test_doubles.h"

const uint64_t Tests::GlIndexFixture::PROGRAM = 5;

Tests::GlContextFixture::GlContextFixture()
    :context(&remapper)
{
    info.has_explicit_program = false;
    info.ignored_param_mask   = 0;
    info.name_ptr             = "glTest";
    info.return_kind          = Stereo::Replay::EngineDLL::ObjectKind::NONE;

    call.func = APIInterceptor::APIFUNCTION_GL_GLACCUM;

    context.begin_call(&call,
                       &walker,
                       0,
                       &info);
}

Tests::GlIndexFixture::GlIndexFixture()
    :replayer(&context,
              [](const char*) -> void*
              {
                  return nullptr;
              })
{
    context.set_current_program(PROGRAM);
}

Tests::PausingInterceptor::PausingInterceptor(Stereo::Replay::EngineDLL::IPlaybackController* in_controller_ptr,
                                              const bool&                                     in_should_pause_on_first_call,
                                              const bool&                                     in_should_skip_calls)
    :m_controller_ptr            (in_controller_ptr),
     m_should_pause_on_first_call(in_should_pause_on_first_call),
     m_should_skip_calls         (in_should_skip_calls)
{
    /* Stub */
}

bool Tests::StubCallExecutor::execute(const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                      const Stereo::Replay::EngineDLL::IChunkSource& in_chunk_source,
                                      std::string*                                   out_error_ptr)
{
    (void) in_call;
    (void) in_chunk_source;
    (void) out_error_ptr;

    return true;
}

const Capture::APIDumpLoader::DataChunk* Tests::MapChunkSource::get_data_chunk_ptr(const uint32_t& in_data_chunk_id) const
{
    const auto iterator = chunk_map.find(in_data_chunk_id);

    return (iterator != chunk_map.end() ) ? &iterator->second
                                          : nullptr;
}

uint32_t Tests::MapChunkSource::get_pointer_n_bytes() const
{
    return pointer_n_bytes;
}

void Tests::PausingInterceptor::on_call_replayed(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                 const uint32_t&                              in_n_frame,
                                                 const bool&                                  in_was_executed)
{
    (void) in_call;
    (void) in_was_executed;

    if (m_should_pause_on_first_call &&
        in_n_frame == 0)
    {
        m_controller_ptr->pause();
    }
}

void Tests::StubCallExecutor::on_frame_presented()
{
    /* Stub */
}

void Tests::StubCallExecutor::reset()
{
    /* Stub */
}

bool Tests::PausingInterceptor::should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                                    const uint32_t&                              in_n_frame)
{
    (void) in_call;
    (void) in_n_frame;

    return (m_should_skip_calls == false);
}
