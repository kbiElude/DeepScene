/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Tests for what the call analyzer does with the data chunks of the calls it analyses: they come out of the interceptor with the bytes that were put in, whatever their sizes, in the
 * vectors that the call analyzer keeps for the next calls, and what a call leaves in a vector is not seen by the call after it.
 */
#include <gtest/gtest.h>
#include "live_call_analyzer.h"
#include "common/callbacks.h"
#include <vector>

namespace
{
    constexpr APIInterceptor::APIFunction CALL_FUNCTION = APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV;

    std::vector<uint8_t> make_bytes(const uint8_t& in_first_value,
                                    const size_t&  in_n_bytes)
    {
        std::vector<uint8_t> result_u8_vec(in_n_bytes);

        for (size_t n_byte = 0;
                    n_byte < in_n_bytes;
                  ++n_byte)
        {
            result_u8_vec.at(n_byte) = static_cast<uint8_t>(in_first_value + n_byte);
        }

        return result_u8_vec;
    }

    /* Registers the bytes as chunks, and makes the call that has them as its arguments. */
    std::vector<uint32_t> make_call(Stereo::ConverterDLL::LiveCallAnalyzer*     inout_call_analyzer_ptr,
                                    const std::vector<std::vector<uint8_t> >&   in_bytes_vec)
    {
        std::vector<uint32_t>                          chunk_id_vec;
        std::vector<APIInterceptor::APIFunctionArgument> argument_vec;

        for (const auto& bytes : in_bytes_vec)
        {
            chunk_id_vec.push_back(APIInterceptor::register_data_chunk(bytes.data(),
                                                                       static_cast<uint32_t>(bytes.size() ) ) );

            argument_vec.push_back(APIInterceptor::APIFunctionArgument::create_data_chunk_id(chunk_id_vec.back() ) );
        }

        inout_call_analyzer_ptr->on_pre (CALL_FUNCTION,
                                         static_cast<uint32_t>(argument_vec.size() ),
                                         argument_vec.data() );
        inout_call_analyzer_ptr->on_post(CALL_FUNCTION,
                                         0,
                                         nullptr,
                                         nullptr);

        return chunk_id_vec;
    }

    std::vector<uint8_t> read_chunk(const Stereo::ConverterDLL::LiveCallAnalyzer& in_call_analyzer,
                                    const uint32_t&                               in_chunk_id,
                                    bool*                                         out_is_there_ptr)
    {
        const void*          data_ptr = nullptr;
        uint32_t             n_bytes  = 0;
        std::vector<uint8_t> result_u8_vec;

        *out_is_there_ptr = in_call_analyzer.get_last_call_chunk(in_chunk_id,
                                                                &data_ptr,
                                                                &n_bytes);

        if (*out_is_there_ptr)
        {
            result_u8_vec.assign(static_cast<const uint8_t*>(data_ptr),
                                 static_cast<const uint8_t*>(data_ptr) + n_bytes);
        }

        return result_u8_vec;
    }
}

TEST(CallFeederTest, TheChunkOfACallIsGivenWithItsBytesWhenTheCallIsDone)
{
    Stereo::ConverterDLL::LiveCallAnalyzer call_analyzer;
    bool                                   is_there = false;

    const std::vector<std::vector<uint8_t> > bytes_vec = {make_bytes(1,
                                                                     12)};
    const auto                               held_before = APIInterceptor::get_n_held_data_chunks();
    const auto                               id_vec      = make_call(&call_analyzer,
                                                                     bytes_vec);

    EXPECT_EQ(read_chunk(call_analyzer,
                         id_vec.at(0),
                        &is_there),
              bytes_vec.at(0) );
    EXPECT_TRUE(is_there);

    /* The call analyzer took the chunk out of the interceptor: nothing of it is left there. */
    EXPECT_EQ(APIInterceptor::get_n_held_data_chunks(),
              held_before);
}

TEST(CallFeederTest, TheChunksOfManyCallsAreEachTheBytesOfItsOwnCallWhateverTheirSizes)
{
    Stereo::ConverterDLL::LiveCallAnalyzer call_analyzer;
    const size_t                           size_array[] = {1, 3, 12, 64, 255, 256, 257, 1000, 12, 3};
    const auto                             held_before  = APIInterceptor::get_n_held_data_chunks();

    for (uint32_t n_call = 0;
                  n_call < 500;
                ++n_call)
    {
        bool       is_there = false;
        const auto bytes    = make_bytes(static_cast<uint8_t>(n_call),
                                         size_array[n_call % (sizeof(size_array) / sizeof(size_array[0]) )]);

        const std::vector<std::vector<uint8_t> > bytes_vec = {bytes};
        const auto                               id_vec    = make_call(&call_analyzer,
                                                                       bytes_vec);

        ASSERT_EQ(read_chunk(call_analyzer,
                             id_vec.at(0),
                            &is_there),
                  bytes) << "call " << n_call;
        ASSERT_TRUE(is_there);
    }

    EXPECT_EQ(APIInterceptor::get_n_held_data_chunks(),
              held_before);
}

TEST(CallFeederTest, ACallWithSeveralChunksGivesEachByItsNumber)
{
    Stereo::ConverterDLL::LiveCallAnalyzer call_analyzer;
    bool                                   is_there = false;

    const std::vector<std::vector<uint8_t> > bytes_vec = {make_bytes(10,
                                                                     5),
                                                          make_bytes(20,
                                                                     300),
                                                          make_bytes(30,
                                                                     1),
                                                          make_bytes(40,
                                                                     64)};

    /* The first call has the same shape, to fill the vectors that the call analyzer keeps with the bytes of other chunks. */
    make_call(&call_analyzer,
              bytes_vec);

    const auto id_vec = make_call(&call_analyzer,
                                  bytes_vec);

    for (size_t n_chunk = 0;
                n_chunk < bytes_vec.size();
              ++n_chunk)
    {
        EXPECT_EQ(read_chunk(call_analyzer,
                             id_vec.at(n_chunk),
                            &is_there),
                  bytes_vec.at(n_chunk) );
        EXPECT_TRUE(is_there);
    }
}

TEST(CallFeederTest, WhatAChunkFuncMakesOfAChunkIsNotSeenByTheCallsAfterIt)
{
    Stereo::ConverterDLL::LiveCallAnalyzer call_analyzer;
    bool                                   is_there = false;

    const auto change_func = [](const APIInterceptor::APIFunction& in_function,
                                std::vector<uint8_t>*              inout_chunk_ptr)
    {
        (void) in_function;

        inout_chunk_ptr->assign(40,
                                0xAB);
    };

    call_analyzer.set_chunk_func(change_func);

    const std::vector<std::vector<uint8_t> > first_bytes_vec = {make_bytes(1,
                                                                           8)};
    const auto                               first_id_vec    = make_call(&call_analyzer,
                                                                         first_bytes_vec);

    EXPECT_EQ(read_chunk(call_analyzer,
                         first_id_vec.at(0),
                        &is_there),
              std::vector<uint8_t>(40,
                                   0xAB) );

    /* The vector that held the 40 bytes is the one that holds the next chunk, which has the bytes it was made of. */
    call_analyzer.set_chunk_func(Stereo::ConverterDLL::LiveCallAnalyzer::ChunkFunc() );

    const std::vector<std::vector<uint8_t> > second_bytes_vec = {make_bytes(100,
                                                                            5)};
    const auto                               second_id_vec    = make_call(&call_analyzer,
                                                                          second_bytes_vec);

    EXPECT_EQ(read_chunk(call_analyzer,
                         second_id_vec.at(0),
                        &is_there),
              second_bytes_vec.at(0) );
    EXPECT_TRUE(is_there);
}

TEST(CallFeederTest, AChunkThatIsNotInTheInterceptorIsNotGiven)
{
    Stereo::ConverterDLL::LiveCallAnalyzer call_analyzer;
    bool                                   is_there = true;

    constexpr uint32_t CHUNK_ID_NEVER_MADE = 0x7FFFFFF0u;

    const APIInterceptor::APIFunctionArgument argument = APIInterceptor::APIFunctionArgument::create_data_chunk_id(CHUNK_ID_NEVER_MADE);

    call_analyzer.on_pre (CALL_FUNCTION,
                          1,
                         &argument);
    call_analyzer.on_post(CALL_FUNCTION,
                          0,
                          nullptr,
                          nullptr);

    read_chunk(call_analyzer,
               CHUNK_ID_NEVER_MADE,
              &is_there);

    EXPECT_FALSE(is_there);
}

TEST(LiveCallAnalyzerTest, TheCallsThatMakeTheWindowOfAnOpenGLProgramAreFollowedAndTheRestOfUSER32IsNot)
{
    EXPECT_TRUE(Stereo::ConverterDLL::LiveCallAnalyzer::is_followed(APIInterceptor::APIFUNCTION_USER32_CREATEWINDOWEXA) );
    EXPECT_TRUE(Stereo::ConverterDLL::LiveCallAnalyzer::is_followed(APIInterceptor::APIFUNCTION_USER32_CREATEWINDOWEXW) );
    EXPECT_TRUE(Stereo::ConverterDLL::LiveCallAnalyzer::is_followed(APIInterceptor::APIFUNCTION_USER32_GETDC) );
    EXPECT_FALSE(Stereo::ConverterDLL::LiveCallAnalyzer::is_followed(APIInterceptor::APIFUNCTION_USER32_RELEASEDC) );
    EXPECT_FALSE(Stereo::ConverterDLL::LiveCallAnalyzer::is_followed(APIInterceptor::APIFUNCTION_USER32_ADJUSTWINDOWRECTEX) );
}
