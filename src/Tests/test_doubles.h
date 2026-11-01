/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * What the tests of the replayer stand in for the real thing with, shared by them.
 */
#ifndef TESTS_TEST_DOUBLES_H
#define TESTS_TEST_DOUBLES_H

#include "Stereo/Replay/EngineDLL/entrypoints.h"
#include "call_walker.h"
#include "gl_index_replayer.h"
#include "gl_replay_context.h"
#include "i_call_executor.h"
#include "i_chunk_source.h"
#include <map>
#include <string>

namespace Tests
{
    /* Counts what it is told: a listener for the tests of ListenerList. */
    struct CountingListener
    {
        int n_calls = 0;
    };

    /* A call of a function of GL that has no parameters of interest, in a context that nothing else is remapped in. */
    struct GlContextFixture
    {
        GlContextFixture();

        Capture::APIDumpLoader::DumpedAPICall      call;
        Stereo::Replay::EngineDLL::GlFunctionInfo  info;
        Stereo::Replay::EngineDLL::GlReplayContext context;
        Stereo::Replay::EngineDLL::ObjectRemapper  remapper;
        Stereo::Replay::EngineDLL::CallWalker      walker;
    };

    /* The same, in which a program is current and its indices can be mapped. */
    struct GlIndexFixture : public GlContextFixture
    {
        static const uint64_t PROGRAM;

        GlIndexFixture();

        Stereo::Replay::EngineDLL::GlIndexReplayer replayer;
    };

    /* Holds chunks, as a segment does. */
    class MapChunkSource : public Stereo::Replay::EngineDLL::IChunkSource
    {
    public:
        const Capture::APIDumpLoader::DataChunk* get_data_chunk_ptr (const uint32_t& in_data_chunk_id) const override;
        uint32_t                                 get_pointer_n_bytes()                                 const override;

        std::map<uint32_t, Capture::APIDumpLoader::DataChunk> chunk_map;
        uint32_t                                              pointer_n_bytes = 4;
    };

    /* Pauses the replay, or skips the calls, as told. */
    class PausingInterceptor : public Stereo::Replay::EngineDLL::ICallInterceptor
    {
    public:
        PausingInterceptor(Stereo::Replay::EngineDLL::IPlaybackController* in_controller_ptr,
                           const bool&                                     in_should_pause_on_first_call,
                           const bool&                                     in_should_skip_calls);

        void on_call_replayed   (const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                 const uint32_t&                              in_n_frame,
                                 const bool&                                  in_was_executed) override;
        bool should_execute_call(const Capture::APIDumpLoader::DumpedAPICall& in_call,
                                 const uint32_t&                              in_n_frame) override;

    private:
        Stereo::Replay::EngineDLL::IPlaybackController* m_controller_ptr;
        bool                                            m_should_pause_on_first_call;
        bool                                            m_should_skip_calls;
    };

    /* Executes nothing, and succeeds. */
    class StubCallExecutor : public Stereo::Replay::EngineDLL::ICallExecutor
    {
    public:
        bool execute           (const Capture::APIDumpLoader::DumpedAPICall&   in_call,
                                const Stereo::Replay::EngineDLL::IChunkSource& in_chunk_source,
                                std::string*                                   out_error_ptr) override;
        void on_frame_presented() override;
        void reset             () override;
    };
}

#endif /* TESTS_TEST_DOUBLES_H */
