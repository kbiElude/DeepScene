/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the data file which the tests of the analysis run on.
 */
#include <gtest/gtest.h>
#include "capture/api_dump_loader/workload_reader.h"
#include "capture/api_dump_loader/api_info.h"
#include "data_fixtures.h"
#include <vector>

TEST(Fr08Slice, HoldsTheChosenFramesWithTheirDraws)
{
    std::vector<uint32_t>   draws_per_frame_vec(1,
                                                0);
    uint64_t                         n_calls     = 0;
    uint32_t                         n_textures  = 0;
    Capture::APIDumpLoader::Workload segment;
    std::string                      error;

    auto reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(Tests::DataFixtures::get_fr08_slice_filename().c_str(),
                                                                        &error);

    ASSERT_NE(reader_ptr,
              nullptr) << error;

    while (reader_ptr->read_next_segment(&segment) )
    {
        for (const auto& call : *segment.dumped_api_call_vec_ptr)
        {
            n_calls++;

            if (call.func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_DRAWINDEXEDPRIMITIVE)
            {
                draws_per_frame_vec.back()++;
            }
            else
            if (call.func == APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_CREATETEXTURE)
            {
                n_textures++;
            }

            if (Capture::APIDumpLoader::is_frame_boundary(call) )
            {
                draws_per_frame_vec.push_back(0);
            }
        }
    }

    EXPECT_FALSE(reader_ptr->is_truncated() );
    EXPECT_EQ(reader_ptr->get_pointer_n_bytes(),
              4u);
    EXPECT_EQ(n_calls,
              16836u);
    EXPECT_EQ(n_textures,
              68u);

    /* The frames of the file, then the empty tail after the last present. */
    ASSERT_EQ(draws_per_frame_vec.size(),
              11u);

    EXPECT_EQ(draws_per_frame_vec.at(4),
              1u);   /* frame 100:    one pass                                */
    EXPECT_EQ(draws_per_frame_vec.at(6),
              81u);  /* frame 14364:  three passes                            */
    EXPECT_EQ(draws_per_frame_vec.at(8),
              153u); /* frame 219679: scene to texture, post-processing, final */
    EXPECT_EQ(draws_per_frame_vec.back(),
              0u);
}

TEST(Fr08Slice, HasNoBigChunks)
{
    constexpr size_t MAX_CHUNK_N_BYTES = 1024;

    Capture::APIDumpLoader::Workload segment;

    auto reader_ptr = Capture::APIDumpLoader::WorkloadReader::create(Tests::DataFixtures::get_fr08_slice_filename().c_str() );

    ASSERT_NE(reader_ptr,
              nullptr);

    while (reader_ptr->read_next_segment(&segment) )
    {
        for (const auto& chunk_ptr : segment.data_chunk_ptr_vec)
        {
            EXPECT_LE(chunk_ptr->size(),
                      MAX_CHUNK_N_BYTES);
        }
    }
}
