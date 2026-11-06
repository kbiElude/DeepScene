/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for FrameBuffer: calls and their data chunks are copied, so that a frame outlives the segments it was read from.
 */
#include <gtest/gtest.h>
#include "frame_buffer.h"
#include "map_chunk_source.h"

static Capture::APIDumpLoader::DumpedAPICall make_call(const std::vector<uint32_t>& in_chunk_id_vec)
{
    Capture::APIDumpLoader::DumpedAPICall call;

    call.func          = APIInterceptor::APIFUNCTION_GL_GLACCUM;
    call.thread_id     = 7;
    call.timestamp_qpc = 1234;

    for (const auto& chunk_id : in_chunk_id_vec)
    {
        call.arg_in_vec.push_back(APIInterceptor::APIFunctionArgument::create_data_chunk_id(chunk_id) );
    }

    return call;
}

TEST(FrameBuffer, ACallAndItsChunksOutliveTheSource)
{
    Stereo::Replay::EngineDLL::FrameBuffer buffer;

    {
        Tests::MapChunkSource source;

        source.chunk_map[5] = {1, 2, 3};
        source.chunk_map[6] = {4};

        buffer.add_call(make_call({5, 6}),
                        source);
    }

    ASSERT_EQ(buffer.get_call_vec().size(),
              1u);
    EXPECT_EQ(buffer.get_call_vec().at(0).func,
              APIInterceptor::APIFUNCTION_GL_GLACCUM);
    EXPECT_EQ(buffer.get_call_vec().at(0).thread_id,
              7u);
    EXPECT_EQ(buffer.get_call_vec().at(0).timestamp_qpc,
              1234u);
    ASSERT_NE(buffer.get_data_chunk_ptr(5),
              nullptr);
    EXPECT_EQ(*buffer.get_data_chunk_ptr(5),
              (Capture::APIDumpLoader::DataChunk{1, 2, 3}) );
    EXPECT_EQ(*buffer.get_data_chunk_ptr(6),
              (Capture::APIDumpLoader::DataChunk{4}) );
}

TEST(FrameBuffer, CallsKeepTheirOrder)
{
    Stereo::Replay::EngineDLL::FrameBuffer buffer;
    Tests::MapChunkSource                  source;

    for (uint32_t n_call = 0;
                  n_call < 5;
                ++n_call)
    {
        auto call = make_call({});

        call.timestamp_qpc = n_call;

        buffer.add_call(call,
                        source);
    }

    ASSERT_EQ(buffer.get_call_vec().size(),
              5u);

    for (uint32_t n_call = 0;
                  n_call < 5;
                ++n_call)
    {
        EXPECT_EQ(buffer.get_call_vec().at(n_call).timestamp_qpc,
                  n_call);
    }
}

TEST(FrameBuffer, AChunkThatSeveralCallsUseIsKeptOnce)
{
    Stereo::Replay::EngineDLL::FrameBuffer buffer;
    Tests::MapChunkSource                  source;

    source.chunk_map[9] = {1, 2};

    buffer.add_call(make_call({9}),
                    source);

    /* The source changes its mind: what the buffer has is what it first copied. */
    source.chunk_map[9] = {7, 7, 7};

    buffer.add_call(make_call({9}),
                    source);

    EXPECT_EQ(*buffer.get_data_chunk_ptr(9),
              (Capture::APIDumpLoader::DataChunk{1, 2}) );
}

TEST(FrameBuffer, AChunkOfAnOutputArgumentIsCopiedToo)
{
    Stereo::Replay::EngineDLL::FrameBuffer buffer;
    auto                                   call = make_call({});
    Tests::MapChunkSource                  source;

    source.chunk_map[3] = {8, 8};

    call.arg_out_vec.push_back(APIInterceptor::APIFunctionArgument::create_data_chunk_id(3) );

    buffer.add_call(call,
                    source);

    ASSERT_NE(buffer.get_data_chunk_ptr(3),
              nullptr);
    EXPECT_EQ(buffer.get_data_chunk_ptr(3)->size(),
              2u);
}

TEST(FrameBuffer, AChunkTheSourceDoesNotHaveIsNotThereEither)
{
    Stereo::Replay::EngineDLL::FrameBuffer buffer;
    Tests::MapChunkSource                  source;

    buffer.add_call(make_call({42}),
                    source);

    EXPECT_EQ(buffer.get_data_chunk_ptr(42),
              nullptr);
    EXPECT_EQ(buffer.get_data_chunk_ptr(43),
              nullptr);
}

TEST(FrameBuffer, TheReturnedValueIsCopied)
{
    Stereo::Replay::EngineDLL::FrameBuffer buffer;
    auto                                   call = make_call({});
    Tests::MapChunkSource                  source;

    call.returned_value_ptr.reset(new APIInterceptor::APIFunctionArgument(APIInterceptor::APIFunctionArgument::create_fp32(2.5f) ));

    buffer.add_call(call,
                    source);

    call.returned_value_ptr.reset();

    ASSERT_NE(buffer.get_call_vec().at(0).returned_value_ptr,
              nullptr);
    EXPECT_FLOAT_EQ(buffer.get_call_vec().at(0).returned_value_ptr->get_fp32(),
                    2.5f);
}

TEST(FrameBuffer, ThePointerSizeIsThatOfTheSource)
{
    Stereo::Replay::EngineDLL::FrameBuffer buffer;
    Tests::MapChunkSource                  source;

    source.pointer_n_bytes = 8;

    buffer.add_call(make_call({}),
                    source);

    EXPECT_EQ(buffer.get_pointer_n_bytes(),
              8u);
}

TEST(FrameBuffer, ClearingMakesItEmpty)
{
    Stereo::Replay::EngineDLL::FrameBuffer buffer;
    Tests::MapChunkSource                  source;

    source.chunk_map[1] = {1};

    buffer.add_call(make_call({1}),
                    source);
    buffer.clear();

    EXPECT_TRUE(buffer.get_call_vec().empty() );
    EXPECT_EQ  (buffer.get_data_chunk_ptr(1),
                nullptr);
}
