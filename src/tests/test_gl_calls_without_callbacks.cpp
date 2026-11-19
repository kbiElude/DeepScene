/* API Interceptor (c) 2025 Dominik Witczak
 *
 * This code is licensed under MIT license (see LICENSE.txt for details)
 *
 * Checks of the OpenGL calls that the converter does not need a callback after (the calls that pass a vertex) or at all (the calls that set a color, a normal or a texture
 * coordinate): the analysis finds the same of a frame whether or not it sees what the vertex calls returned and the calls that set an attribute.
 */
#include <gtest/gtest.h>
#include "analysis/api_dump_analysis/frontends/OpenGL/gl_function_policy.h"
#include "capture/api_dump_names/function_names.h"
#include "common/callbacks.h"
#include "gl_frontend_fixture.h"
#include "live_call_analyzer.h"
#include <vector>

namespace
{
    class GLCallsWithoutCallbacks : public Tests::GLFrontendFixture
    {
    protected:
        /* A frame of two blocks with every kind of vertex call and with calls that set attributes, under a perspective projection. */
        void make_frame()
        {
            constexpr uint32_t QUAD_STRIP = 0x8;
            constexpr uint32_t TRIANGLES  = 0x4;

            start(640,
                  480);

            m_builder.matrix_mode  (PROJECTION);
            m_builder.load_identity();
            m_builder.frustum      (-1.0,
                                    1.0,
                                    -1.0,
                                    1.0,
                                    1.0,
                                    100.0);
            m_builder.matrix_mode  (MODELVIEW);
            m_builder.load_identity();
            m_builder.clear        (COLOR_BUFFER_BIT);

            for (uint32_t n_block = 0;
                          n_block < 2;
                        ++n_block)
            {
                const float depth = -2.0f - 3.0f * static_cast<float>(n_block);

                m_builder.begin((n_block == 0) ? TRIANGLES
                                               : QUAD_STRIP);

                m_builder.add_call(APIInterceptor::APIFUNCTION_GL_GLCOLOR4F,
                                   {APIInterceptor::APIFunctionArgument::create_fp32(1.0f),
                                    APIInterceptor::APIFunctionArgument::create_fp32(0.5f),
                                    APIInterceptor::APIFunctionArgument::create_fp32(0.25f),
                                    APIInterceptor::APIFunctionArgument::create_fp32(1.0f)});
                m_builder.add_call(APIInterceptor::APIFUNCTION_GL_GLTEXCOORD2F,
                                   {APIInterceptor::APIFunctionArgument::create_fp32(0.0f),
                                    APIInterceptor::APIFunctionArgument::create_fp32(1.0f)});
                m_builder.vertex3f(0.0f,
                                   0.0f,
                                   depth);
                m_builder.add_call(APIInterceptor::APIFUNCTION_GL_GLNORMAL3F,
                                   {APIInterceptor::APIFunctionArgument::create_fp32(0.0f),
                                    APIInterceptor::APIFunctionArgument::create_fp32(0.0f),
                                    APIInterceptor::APIFunctionArgument::create_fp32(1.0f)});
                m_builder.vertex3fv({1.0f, 0.0f, depth});
                m_builder.vertex2f (0.0f,
                                    1.0f);
                m_builder.vertex3f (1.0f,
                                    1.0f,
                                    depth - 1.0f);
                m_builder.end();
            }

            m_builder.swap_buffers(DEVICE_CONTEXT_ADDRESS);
        }
    };

    /* What of a frame the analysis finds: the keys, the facts of the draws, and the counts. */
    std::vector<uint64_t> summarize(const Analysis::APIDumpAnalysis::Model& in_model)
    {
        std::vector<uint64_t> result_vec;

        for (const auto& entry : in_model.draw_structure_map)
        {
            result_vec.push_back(entry.first);
        }

        for (const auto& entry : in_model.draw_facts_map)
        {
            const auto& facts = entry.second;

            result_vec.push_back(entry.first.first);
            result_vec.push_back(entry.first.second);
            result_vec.push_back(facts.n_draws);
            result_vec.push_back(facts.n_draws_flat_layer);
            result_vec.push_back(facts.n_draws_with_footprint);
            result_vec.push_back(*reinterpret_cast<const uint32_t*>(&facts.view_depth_max) );
            result_vec.push_back(*reinterpret_cast<const uint32_t*>(&facts.view_depth_min) );
            result_vec.push_back(*reinterpret_cast<const uint32_t*>(&facts.x_max) );
            result_vec.push_back(*reinterpret_cast<const uint32_t*>(&facts.y_max) );
        }

        for (const auto& frame : in_model.frame_record_vec)
        {
            result_vec.push_back(frame.n_draws);
            result_vec.push_back(frame.n_passes);
            result_vec.push_back(frame.shape_id);
        }

        return result_vec;
    }
}

TEST_F(GLCallsWithoutCallbacks, TheFrontEndFindsTheSameWithoutWhatTheVertexCallsReturned)
{
    make_frame();
    feed();

    ASSERT_EQ(m_harness.get_model().frame_record_vec.size(),
              1u);
    ASSERT_EQ(m_harness.get_model().frame_record_vec.at(0).n_draws,
              2u);

    const auto with_outputs = summarize(m_harness.get_model() );

    /* The calls as a call that is analysed before it is made has them: no outputs, and nothing that it returned. */
    auto& call_vec = *m_builder.get_workload().dumped_api_call_vec_ptr;

    for (auto& call : call_vec)
    {
        if (Stereo::ConverterDLL::LiveCallAnalyzer::is_analyzed_before_the_call(call.func) ||
            Stereo::ConverterDLL::LiveCallAnalyzer::is_attribute_call           (call.func) )
        {
            call.arg_out_vec.clear();

            call.returned_value_ptr.reset();
        }
    }

    Harness harness;

    harness.feed(m_builder);

    EXPECT_EQ(summarize(harness.get_model() ),
              with_outputs);
    EXPECT_EQ(harness.get_frontend().get_n_unsupported_calls(),
              0u);
}

TEST_F(GLCallsWithoutCallbacks, TheFrontEndFindsTheSameWithoutTheCallsThatSetAnAttribute)
{
    make_frame();
    feed();

    const auto with_attributes = summarize(m_harness.get_model() );

    Harness harness;

    {
        const auto& call_vec = *m_builder.get_workload().dumped_api_call_vec_ptr;

        for (size_t n_call = 0;
                    n_call < call_vec.size();
                  ++n_call)
        {
            if (Stereo::ConverterDLL::LiveCallAnalyzer::is_attribute_call(call_vec.at(n_call).func) == false)
            {
                harness.get_builder().begin_call(n_call,
                                                 call_vec.at(n_call).timestamp_qpc);
                harness.get_frontend().on_call  (call_vec.at(n_call),
                                                 m_builder.get_workload() );
            }
        }
    }

    EXPECT_EQ(summarize(harness.get_model() ),
              with_attributes);
}

TEST(LiveCallAnalyzerPredicates, VertexCallsAreAnalysedBeforeTheyAreMadeAndNothingElseIs)
{
    using Stereo::ConverterDLL::LiveCallAnalyzer;

    EXPECT_TRUE (LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3F) );
    EXPECT_TRUE (LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV) );
    EXPECT_TRUE (LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX2I) );
    EXPECT_TRUE (LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_GL_GLVERTEX4DV) );
    EXPECT_FALSE(LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_GL_GLBEGIN) );
    EXPECT_FALSE(LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_GL_GLEND) );
    EXPECT_FALSE(LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_GL_GLCOLOR4F) );
    EXPECT_FALSE(LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_GL_GLBINDTEXTURE) );
    EXPECT_FALSE(LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_GL_GLGENTEXTURES) );
    EXPECT_FALSE(LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_GDI32_SWAPBUFFERS) );
    EXPECT_FALSE(LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_D3D8_IDIRECT3DDEVICE8_PRESENT) );
    EXPECT_FALSE(LiveCallAnalyzer::is_analyzed_before_the_call(APIInterceptor::APIFUNCTION_D3D7_IDIRECT3DDEVICE7_SETRENDERSTATE) );
}

TEST(LiveCallAnalyzerPredicates, EveryAttributeCallIsOneThatTheFrontEndLeavesOut)
{
    using Stereo::ConverterDLL::LiveCallAnalyzer;

    uint32_t n_attribute_calls = 0;

    for (uint32_t n_api_func = 0;
                  n_api_func < APIInterceptor::APIFUNCTION_COUNT;
                ++n_api_func)
    {
        const auto function = static_cast<APIInterceptor::APIFunction>(n_api_func);

        if (LiveCallAnalyzer::is_attribute_call(function) )
        {
            n_attribute_calls++;

            EXPECT_TRUE (Analysis::APIDumpAnalysis::Frontends::GLFunctionPolicy::is_left_out_by_design(function) ) << Capture::APIDumpNames::get_function_name(function);
            EXPECT_FALSE(LiveCallAnalyzer::is_analyzed_before_the_call(function) )                                  << Capture::APIDumpNames::get_function_name(function);
            EXPECT_FALSE(LiveCallAnalyzer::is_draw(function) )                                                      << Capture::APIDumpNames::get_function_name(function);
        }
    }

    EXPECT_GT(n_attribute_calls,
              100u);
}

TEST(LiveCallAnalyzerPredicates, AVertexCallGivesUpItsChunkWhenItStartsAndNeedsNoCallAfterIt)
{
    Stereo::ConverterDLL::LiveCallAnalyzer call_analyzer;

    const std::vector<uint8_t> bytes_u8_vec(12, 7);
    const auto                 held_before = APIInterceptor::get_n_held_data_chunks();
    const auto                 chunk_id    = APIInterceptor::register_data_chunk(bytes_u8_vec.data(),
                                                                                 static_cast<uint32_t>(bytes_u8_vec.size() ) );
    const auto                 argument    = APIInterceptor::APIFunctionArgument::create_data_chunk_id(chunk_id);

    call_analyzer.on_pre(APIInterceptor::APIFUNCTION_GL_GLVERTEX3FV,
                         1,
                        &argument);

    const void* data_ptr = nullptr;
    uint32_t    n_bytes  = 0;

    EXPECT_TRUE(call_analyzer.get_last_call_chunk(chunk_id,
                                                 &data_ptr,
                                                 &n_bytes) );
    EXPECT_EQ  (n_bytes,
                bytes_u8_vec.size() );
    EXPECT_EQ  (APIInterceptor::get_n_held_data_chunks(),
                held_before);
}
